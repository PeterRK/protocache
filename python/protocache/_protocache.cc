#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <vector>

#include "protocache/access.h"
#include "protocache/serialize.h"

namespace {

class PyObjectPtr final {
public:
	PyObjectPtr() noexcept = default;
	explicit PyObjectPtr(PyObject* ptr) noexcept : ptr_(ptr) {}
	PyObjectPtr(const PyObjectPtr&) = delete;
	PyObjectPtr& operator=(const PyObjectPtr&) = delete;
	PyObjectPtr(PyObjectPtr&& other) noexcept : ptr_(other.release()) {}
	PyObjectPtr& operator=(PyObjectPtr&& other) noexcept {
		if (this != &other) {
			reset(other.release());
		}
		return *this;
	}
	~PyObjectPtr() {
		Py_XDECREF(ptr_);
	}

	PyObject* get() const noexcept { return ptr_; }
	explicit operator bool() const noexcept { return ptr_ != nullptr; }

	PyObject* release() noexcept {
		auto* ptr = ptr_;
		ptr_ = nullptr;
		return ptr;
	}
	void reset(PyObject* ptr = nullptr) noexcept {
		if (ptr_ != ptr) {
			Py_XDECREF(ptr_);
			ptr_ = ptr;
		}
	}

private:
	PyObject* ptr_ = nullptr;
};

class ScopedPyBuffer final {
public:
	ScopedPyBuffer() = default;
	ScopedPyBuffer(const ScopedPyBuffer&) = delete;
	ScopedPyBuffer& operator=(const ScopedPyBuffer&) = delete;
	~ScopedPyBuffer() {
		if (view_.obj != nullptr) {
			PyBuffer_Release(&view_);
		}
	}

	bool Get(PyObject* obj, int flags) {
		return PyObject_GetBuffer(obj, &view_, flags) == 0;
	}
	Py_buffer* get() noexcept { return &view_; }
	Py_buffer release() noexcept {
		auto view = view_;
		view_.obj = nullptr;
		return view;
	}

private:
	Py_buffer view_{};
};

class ViewBuffer final {
public:
	explicit ViewBuffer(Py_buffer&& view) noexcept : view_(view) {
		view.obj = nullptr;
	}
	ViewBuffer(const ViewBuffer&) = delete;
	ViewBuffer& operator=(const ViewBuffer&) = delete;
	~ViewBuffer() {
		if (view_.obj != nullptr) {
			PyBuffer_Release(&view_);
		}
	}

	const uint32_t* data() const noexcept {
		return reinterpret_cast<const uint32_t*>(view_.buf);
	}
	size_t size() const noexcept {
		return static_cast<size_t>(view_.len / static_cast<Py_ssize_t>(sizeof(uint32_t)));
	}

private:
	Py_buffer view_{};
};

enum Kind {
	KIND_NONE = 0,
	KIND_MESSAGE = 1,
	KIND_BYTES = 2,
	KIND_STRING = 3,
	KIND_F64 = 4,
	KIND_F32 = 5,
	KIND_U64 = 6,
	KIND_U32 = 7,
	KIND_I64 = 8,
	KIND_I32 = 9,
	KIND_BOOL = 10,
	KIND_ENUM = 11,
	KIND_ARRAY = 20,
	KIND_MAP = 21,
};

struct SchemaField {
	PyObject* name = nullptr;
	unsigned id = 0;
	bool repeated = false;
	int key_kind = 0;
	int value_kind = 0;
	PyObject* value_type = nullptr;
};

using Storage = std::shared_ptr<ViewBuffer>;
static_assert(std::is_nothrow_copy_constructible_v<Storage>);

struct MessageView {
	PyObject_HEAD
	Storage storage;
	const uint32_t* ptr;
	const uint32_t* end;
};

struct ArrayView {
	PyObject_HEAD
	Storage storage;
	const uint32_t* ptr;
	const uint32_t* end;
};

struct MapView {
	PyObject_HEAD
	Storage storage;
	const uint32_t* ptr;
	const uint32_t* end;
};

PyTypeObject MessageViewType = {PyVarObject_HEAD_INIT(nullptr, 0)};
PyTypeObject ArrayViewType = {PyVarObject_HEAD_INIT(nullptr, 0)};
PyTypeObject MapViewType = {PyVarObject_HEAD_INIT(nullptr, 0)};

static const char* DataOrEmpty(const char* data) {
	return data == nullptr ? "" : data;
}

static bool CheckStringKeySize(Py_ssize_t size) {
	if (size < 0 || static_cast<unsigned long long>(size) >
			static_cast<unsigned long long>(std::numeric_limits<unsigned>::max())) {
		PyErr_SetString(PyExc_OverflowError, "map key is too large");
		return false;
	}
	return true;
}

template <typename View>
static PyObject* CreateView(PyTypeObject* type, const Storage& storage, const uint32_t* ptr, const uint32_t* end) {
	auto* obj = PyObject_New(View, type);
	if (obj == nullptr) {
		return nullptr;
	}
	new (&obj->storage) Storage(storage);
	obj->ptr = ptr;
	obj->end = end;
	return reinterpret_cast<PyObject*>(obj);
}

static PyObject* CreateMessageView(const Storage& storage, const uint32_t* ptr, const uint32_t* end) {
	return CreateView<MessageView>(&MessageViewType, storage, ptr, end);
}

static PyObject* CreateArrayView(const Storage& storage, const uint32_t* ptr, const uint32_t* end) {
	return CreateView<ArrayView>(&ArrayViewType, storage, ptr, end);
}

static PyObject* CreateMapView(const Storage& storage, const uint32_t* ptr, const uint32_t* end) {
	return CreateView<MapView>(&MapViewType, storage, ptr, end);
}

static Storage ReadBufferWords(PyObject* src) {
	ScopedPyBuffer view;
	if (!view.Get(src, PyBUF_CONTIG_RO)) {
		return {};
	}
	// ProtoCache words are read directly as uint32_t. This binding intentionally
	// relies on callers/platforms providing buffers where such reads are valid.
	auto* raw = view.get();
	if (raw->len <= 0 || (raw->len % static_cast<Py_ssize_t>(sizeof(uint32_t))) != 0) {
		PyErr_SetString(PyExc_ValueError, "ProtoCache data must be a non-empty 32-bit aligned byte sequence");
		return {};
	}
	try {
		return std::make_shared<ViewBuffer>(view.release());
	} catch (const std::bad_alloc&) {
		PyErr_NoMemory();
		return {};
	}
}

static PyObject* FieldToPy(const protocache::Field& field, int kind,
						   const Storage& storage, const uint32_t* end) {
	switch (kind) {
		case KIND_BOOL:
			return PyBool_FromLong(protocache::FieldT<bool>(field).Get(end));
		case KIND_I32:
		case KIND_ENUM:
			return PyLong_FromLong(protocache::FieldT<int32_t>(field).Get(end));
		case KIND_U32:
			return PyLong_FromUnsignedLong(protocache::FieldT<uint32_t>(field).Get(end));
		case KIND_I64:
			return PyLong_FromLongLong(protocache::FieldT<int64_t>(field).Get(end));
		case KIND_U64:
			return PyLong_FromUnsignedLongLong(protocache::FieldT<uint64_t>(field).Get(end));
		case KIND_F32:
			return PyFloat_FromDouble(protocache::FieldT<float>(field).Get(end));
		case KIND_F64:
			return PyFloat_FromDouble(protocache::FieldT<double>(field).Get(end));
		case KIND_STRING:
		{
			auto view = protocache::FieldT<protocache::Slice<char>>(field).Get(end);
			return PyUnicode_DecodeUTF8(DataOrEmpty(view.data()), static_cast<Py_ssize_t>(view.size()), "strict");
		}
		case KIND_BYTES:
		{
			auto view = protocache::FieldT<protocache::Slice<uint8_t>>(field).Get(end);
			return PyBytes_FromStringAndSize(
					reinterpret_cast<const char*>(DataOrEmpty(reinterpret_cast<const char*>(view.data()))),
					static_cast<Py_ssize_t>(view.size()));
		}
		case KIND_MESSAGE:
		{
			auto ptr = field.GetObject(end);
			if (ptr == nullptr) {
				Py_RETURN_NONE;
			}
			protocache::Message msg(ptr, end);
			if (!msg) {
				PyErr_SetString(PyExc_ValueError, "invalid nested ProtoCache message");
				return nullptr;
			}
			return CreateMessageView(storage, ptr, end);
		}
		case KIND_ARRAY:
			return CreateArrayView(storage, field.GetObject(end), end);
		case KIND_MAP:
			return CreateMapView(storage, field.GetObject(end), end);
		default:
			PyErr_SetString(PyExc_ValueError, "unsupported ProtoCache kind");
			return nullptr;
	}
}

template <typename View>
static void View_dealloc(View* self) {
	self->storage.~Storage();
	Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

static PyObject* MessageView_from_bytes(PyObject*, PyObject* arg) {
	auto storage = ReadBufferWords(arg);
	if (!storage) {
		return nullptr;
	}
	auto ptr = storage->data();
	auto end = ptr + storage->size();
	protocache::Message msg(ptr, end);
	if (!msg) {
		PyErr_SetString(PyExc_ValueError, "invalid ProtoCache message");
		return nullptr;
	}
	return CreateMessageView(storage, ptr, end);
}

static PyObject* ArrayView_from_bytes(PyObject*, PyObject* arg) {
	auto storage = ReadBufferWords(arg);
	if (!storage) {
		return nullptr;
	}
	auto ptr = storage->data();
	return CreateArrayView(storage, ptr, ptr + storage->size());
}

static PyObject* MapView_from_bytes(PyObject*, PyObject* arg) {
	auto storage = ReadBufferWords(arg);
	if (!storage) {
		return nullptr;
	}
	auto ptr = storage->data();
	return CreateMapView(storage, ptr, ptr + storage->size());
}

static PyObject* MessageView_has_field(MessageView* self, PyObject* arg) {
	auto id = PyLong_AsUnsignedLong(arg);
	if (PyErr_Occurred()) {
		return nullptr;
	}
	protocache::Message msg(self->ptr, self->end);
	if (!msg) {
		Py_RETURN_FALSE;
	}
	return PyBool_FromLong(msg.HasField(static_cast<unsigned>(id), self->end));
}

static PyObject* MessageView_get(MessageView* self, PyObject* args) {
	unsigned id;
	int kind;
	if (!PyArg_ParseTuple(args, "Ii", &id, &kind)) {
		return nullptr;
	}
	protocache::Message msg(self->ptr, self->end);
	if (!msg) {
		PyErr_SetString(PyExc_ValueError, "invalid ProtoCache message view");
		return nullptr;
	}
	auto field = msg.GetField(id, self->end);
	return FieldToPy(field, kind, self->storage, self->end);
}

static PyObject* MessageView_get_array(MessageView* self, PyObject* arg) {
	auto id = PyLong_AsUnsignedLong(arg);
	if (PyErr_Occurred()) {
		return nullptr;
	}
	protocache::Message msg(self->ptr, self->end);
	if (!msg) {
		PyErr_SetString(PyExc_ValueError, "invalid ProtoCache message view");
		return nullptr;
	}
	return CreateArrayView(self->storage, msg.GetField(static_cast<unsigned>(id), self->end).GetObject(self->end), self->end);
}

static PyObject* MessageView_get_map(MessageView* self, PyObject* arg) {
	auto id = PyLong_AsUnsignedLong(arg);
	if (PyErr_Occurred()) {
		return nullptr;
	}
	protocache::Message msg(self->ptr, self->end);
	if (!msg) {
		PyErr_SetString(PyExc_ValueError, "invalid ProtoCache message view");
		return nullptr;
	}
	return CreateMapView(self->storage, msg.GetField(static_cast<unsigned>(id), self->end).GetObject(self->end), self->end);
}

template <typename T>
static PyObject* ArrayScalarSize(const uint32_t* ptr, const uint32_t* end) {
	protocache::ArrayT<T> view(ptr, end);
	if (!view) {
		return PyLong_FromLong(0);
	}
	return PyLong_FromUnsignedLong(view.Size());
}

static PyObject* ArrayView_size(ArrayView* self, PyObject* arg) {
	int kind = static_cast<int>(PyLong_AsLong(arg));
	if (PyErr_Occurred()) {
		return nullptr;
	}
	if (self->ptr == nullptr) {
		return PyLong_FromLong(0);
	}
	switch (kind) {
		case KIND_BOOL:
			return ArrayScalarSize<bool>(self->ptr, self->end);
		case KIND_I32:
		case KIND_ENUM:
			return ArrayScalarSize<int32_t>(self->ptr, self->end);
		case KIND_U32:
			return ArrayScalarSize<uint32_t>(self->ptr, self->end);
		case KIND_I64:
			return ArrayScalarSize<int64_t>(self->ptr, self->end);
		case KIND_U64:
			return ArrayScalarSize<uint64_t>(self->ptr, self->end);
		case KIND_F32:
			return ArrayScalarSize<float>(self->ptr, self->end);
		case KIND_F64:
			return ArrayScalarSize<double>(self->ptr, self->end);
		default:
		{
			protocache::Array view(self->ptr, self->end);
			if (!view) {
				return PyLong_FromLong(0);
			}
			return PyLong_FromUnsignedLong(view.Size());
		}
	}
}

template <typename T>
static PyObject* ArrayScalarGet(const uint32_t* ptr, const uint32_t* end, Py_ssize_t index) {
	protocache::ArrayT<T> view(ptr, end);
	if (!view || index < 0 || static_cast<uint32_t>(index) >= view.Size()) {
		PyErr_SetString(PyExc_IndexError, "ProtoCache array index out of range");
		return nullptr;
	}
	if constexpr (std::is_same_v<T, bool>) {
		return PyBool_FromLong(view[static_cast<unsigned>(index)]);
	} else if constexpr (std::is_same_v<T, uint32_t>) {
		return PyLong_FromUnsignedLong(view[static_cast<unsigned>(index)]);
	} else if constexpr (std::is_same_v<T, uint64_t>) {
		return PyLong_FromUnsignedLongLong(view[static_cast<unsigned>(index)]);
	} else if constexpr (std::is_same_v<T, int64_t>) {
		return PyLong_FromLongLong(view[static_cast<unsigned>(index)]);
	} else if constexpr (std::is_floating_point_v<T>) {
		return PyFloat_FromDouble(view[static_cast<unsigned>(index)]);
	} else {
		return PyLong_FromLong(view[static_cast<unsigned>(index)]);
	}
}

static PyObject* ArrayView_get(ArrayView* self, PyObject* args) {
	Py_ssize_t index;
	int kind;
	if (!PyArg_ParseTuple(args, "ni", &index, &kind)) {
		return nullptr;
	}
	if (self->ptr == nullptr) {
		PyErr_SetString(PyExc_IndexError, "ProtoCache array index out of range");
		return nullptr;
	}
	switch (kind) {
		case KIND_BOOL:
			return ArrayScalarGet<bool>(self->ptr, self->end, index);
		case KIND_I32:
		case KIND_ENUM:
			return ArrayScalarGet<int32_t>(self->ptr, self->end, index);
		case KIND_U32:
			return ArrayScalarGet<uint32_t>(self->ptr, self->end, index);
		case KIND_I64:
			return ArrayScalarGet<int64_t>(self->ptr, self->end, index);
		case KIND_U64:
			return ArrayScalarGet<uint64_t>(self->ptr, self->end, index);
		case KIND_F32:
			return ArrayScalarGet<float>(self->ptr, self->end, index);
		case KIND_F64:
			return ArrayScalarGet<double>(self->ptr, self->end, index);
		default:
		{
			protocache::Array view(self->ptr, self->end);
			if (!view || index < 0 || static_cast<uint32_t>(index) >= view.Size()) {
				PyErr_SetString(PyExc_IndexError, "ProtoCache array index out of range");
				return nullptr;
			}
			return FieldToPy(view[static_cast<unsigned>(index)], kind, self->storage, self->end);
		}
	}
}

static PyObject* MapView_size(MapView* self, PyObject*) {
	if (self->ptr == nullptr) {
		return PyLong_FromLong(0);
	}
	protocache::Map view(self->ptr, self->end);
	if (!view) {
		return PyLong_FromLong(0);
	}
	return PyLong_FromUnsignedLong(view.Size());
}

static PyObject* MapView_items(MapView* self, PyObject* args) {
	int key_kind;
	int value_kind;
	if (!PyArg_ParseTuple(args, "ii", &key_kind, &value_kind)) {
		return nullptr;
	}
	PyObjectPtr out(PyList_New(0));
	if (!out) {
		return nullptr;
	}
	if (self->ptr == nullptr) {
		return out.release();
	}
	protocache::Map view(self->ptr, self->end);
	if (!view) {
		return out.release();
	}
	for (auto pair : view) {
		PyObjectPtr key(FieldToPy(pair.Key(), key_kind, self->storage, self->end));
		if (!key) {
			return nullptr;
		}
		PyObjectPtr value(FieldToPy(pair.Value(), value_kind, self->storage, self->end));
		if (!value) {
			return nullptr;
		}
		PyObjectPtr item(PyTuple_Pack(2, key.get(), value.get()));
		if (!item) {
			return nullptr;
		}
		if (PyList_Append(out.get(), item.get()) < 0) {
			return nullptr;
		}
	}
	return out.release();
}

static bool StringKey(PyObject* key, const char** data, Py_ssize_t* size, PyObject** keeper) {
	*keeper = nullptr;
	if (PyUnicode_Check(key)) {
		*keeper = PyUnicode_AsUTF8String(key);
		if (*keeper == nullptr) {
			return false;
		}
		*data = PyBytes_AS_STRING(*keeper);
		*size = PyBytes_GET_SIZE(*keeper);
		if (!CheckStringKeySize(*size)) {
			Py_DECREF(*keeper);
			*keeper = nullptr;
			return false;
		}
		return true;
	}
	if (PyBytes_Check(key)) {
		if (PyBytes_AsStringAndSize(key, const_cast<char**>(data), size) < 0) {
			return false;
		}
		if (!CheckStringKeySize(*size)) {
			return false;
		}
		return true;
	}
	PyErr_SetString(PyExc_TypeError, "map key must be str or bytes");
	return false;
}

template <typename T>
static bool NumberKey(PyObject* key, T* out) {
	if constexpr (std::is_unsigned_v<T>) {
		auto value = PyLong_AsUnsignedLongLong(key);
		if (PyErr_Occurred()) {
			return false;
		}
		if (value > static_cast<unsigned long long>(std::numeric_limits<T>::max())) {
			PyErr_SetString(PyExc_OverflowError, "map key is out of range");
			return false;
		}
		*out = static_cast<T>(value);
		return true;
	} else {
		auto value = PyLong_AsLongLong(key);
		if (PyErr_Occurred()) {
			return false;
		}
		if (value < static_cast<long long>(std::numeric_limits<T>::min()) ||
			value > static_cast<long long>(std::numeric_limits<T>::max())) {
			PyErr_SetString(PyExc_OverflowError, "map key is out of range");
			return false;
		}
		*out = static_cast<T>(value);
		return true;
	}
}

template <typename T>
static PyObject* MapFindNumber(protocache::Map& view, T key, int value_kind,
							   const Storage& storage, const uint32_t* end, PyObject* missing) {
	auto it = view.Find<T>(key, end);
	if (it == view.end()) {
		Py_INCREF(missing);
		return missing;
	}
	return FieldToPy((*it).Value(), value_kind, storage, end);
}

template <typename T>
static PyObject* MapFindNumberKey(protocache::Map& view, PyObject* key, int value_kind,
								  const Storage& storage, const uint32_t* end, PyObject* missing) {
	T v;
	if (!NumberKey(key, &v)) {
		return nullptr;
	}
	return MapFindNumber(view, v, value_kind, storage, end, missing);
}

static PyObject* MapView_get(MapView* self, PyObject* args) {
	int key_kind;
	int value_kind;
	PyObject* key;
	PyObject* missing = Py_None;
	if (!PyArg_ParseTuple(args, "iiO|O", &key_kind, &value_kind, &key, &missing)) {
		return nullptr;
	}
	if (self->ptr == nullptr) {
		Py_INCREF(missing);
		return missing;
	}
	protocache::Map view(self->ptr, self->end);
	if (!view) {
		Py_INCREF(missing);
		return missing;
	}
	switch (key_kind) {
		case KIND_STRING:
		{
			const char* data;
			Py_ssize_t size;
			PyObject* raw_keeper;
			if (!StringKey(key, &data, &size, &raw_keeper)) {
				return nullptr;
			}
			PyObjectPtr keeper(raw_keeper);
			auto it = view.Find(data, static_cast<unsigned>(size), self->end);
			if (it == view.end()) {
				Py_INCREF(missing);
				return missing;
			}
			return FieldToPy((*it).Value(), value_kind, self->storage, self->end);
		}
		case KIND_I32:
		case KIND_ENUM:
			return MapFindNumberKey<int32_t>(view, key, value_kind, self->storage, self->end, missing);
		case KIND_U32:
			return MapFindNumberKey<uint32_t>(view, key, value_kind, self->storage, self->end, missing);
		case KIND_I64:
			return MapFindNumberKey<int64_t>(view, key, value_kind, self->storage, self->end, missing);
		case KIND_U64:
			return MapFindNumberKey<uint64_t>(view, key, value_kind, self->storage, self->end, missing);
		default:
			PyErr_SetString(PyExc_ValueError, "unsupported ProtoCache map key kind");
			return nullptr;
	}
}

struct PyKeyReader final : public protocache::KeyReader {
	explicit PyKeyReader(const std::vector<std::string>& keys) : keys_(keys) {}
	void Reset() override { idx_ = 0; }
	size_t Total() override { return keys_.size(); }
	protocache::Slice<uint8_t> Read() override {
		if (idx_ >= keys_.size()) {
			return {};
		}
		auto& key = keys_[idx_++];
		return {reinterpret_cast<const uint8_t*>(key.data()), key.size()};
	}

private:
	const std::vector<std::string>& keys_;
	size_t idx_ = 0;
};

static PyObject* UnitToBytes(protocache::Buffer& buf, const protocache::Unit& unit) {
	if (unit.len != 0) {
		return PyBytes_FromStringAndSize(
				reinterpret_cast<const char*>(unit.data),
				static_cast<Py_ssize_t>(unit.len * sizeof(uint32_t)));
	}
	auto view = buf.View();
	return PyBytes_FromStringAndSize(
			reinterpret_cast<const char*>(view.data()),
			static_cast<Py_ssize_t>(view.size() * sizeof(uint32_t)));
}

static bool GetSchema(PyObject* obj, PyObject** schema) {
	*schema = PyObject_GetAttrString(obj, "_schema");
	if (*schema == nullptr) {
		return false;
	}
	if (*schema == Py_None) {
		Py_DECREF(*schema);
		*schema = nullptr;
		PyErr_SetString(PyExc_TypeError, "object has no ProtoCache _schema");
		return false;
	}
	return true;
}

static bool IsEmpty(PyObject* value) {
	if (value == Py_None) {
		return true;
	}
	if (PyBool_Check(value)) {
		return value == Py_False;
	}
	if (PyLong_Check(value)) {
		auto zero = PyLong_AsLongLong(value);
		if (PyErr_Occurred()) {
			PyErr_Clear();
			return false;
		}
		return zero == 0;
	}
	if (PyFloat_Check(value)) {
		return PyFloat_AsDouble(value) == 0.0;
	}
	auto size = PyObject_Size(value);
	if (size >= 0) {
		return size == 0;
	}
	PyErr_Clear();
	return false;
}

static bool SerializeValue(PyObject* value, int kind, PyObject* value_type,
						   protocache::Buffer& buf, protocache::Unit& unit);
static bool SerializeArrayObject(PyObject* value, int item_kind, PyObject* item_cls,
								 protocache::Buffer& buf, protocache::Unit& unit);
static bool SerializeMapObject(PyObject* value, int key_kind, int value_kind, PyObject* value_type,
							   protocache::Buffer& buf, protocache::Unit& unit);

static bool IsScalarKind(int kind) {
	switch (kind) {
		case KIND_BYTES:
		case KIND_STRING:
		case KIND_F64:
		case KIND_F32:
		case KIND_U64:
		case KIND_U32:
		case KIND_I64:
		case KIND_I32:
		case KIND_BOOL:
		case KIND_ENUM:
			return true;
		default:
			return false;
	}
}

static bool CheckValueType(int kind, PyObject* value_type) {
	if (IsScalarKind(kind)) {
		if (value_type != Py_None) {
			PyErr_SetString(PyExc_TypeError, "scalar ProtoCache TYPE must not have value_type");
			return false;
		}
		return true;
	}
	if (kind != KIND_MESSAGE && kind != KIND_ARRAY && kind != KIND_MAP) {
		PyErr_SetString(PyExc_ValueError, "invalid ProtoCache value kind");
		return false;
	}
	if (!PyType_Check(value_type)) {
		PyErr_SetString(PyExc_TypeError, "complex ProtoCache TYPE must have class value_type");
		return false;
	}
	return true;
}

static bool ParseAliasTypeSpec(PyObject* spec, SchemaField* out) {
	if (!PyTuple_Check(spec)) {
		PyErr_SetString(PyExc_TypeError, "ProtoCache alias TYPE must be a tuple");
		return false;
	}
	if (PyTuple_GET_SIZE(spec) != 3) {
		PyErr_SetString(PyExc_ValueError, "ProtoCache alias TYPE must have 3 entries");
		return false;
	}
	out->repeated = true;
	out->key_kind = static_cast<int>(PyLong_AsLong(PyTuple_GET_ITEM(spec, 0)));
	if (PyErr_Occurred()) {
		return false;
	}
	out->value_kind = static_cast<int>(PyLong_AsLong(PyTuple_GET_ITEM(spec, 1)));
	if (PyErr_Occurred()) {
		return false;
	}
	out->value_type = PyTuple_GET_ITEM(spec, 2);
	return CheckValueType(out->value_kind, out->value_type);
}

static bool ParseAliasType(PyObject* cls, PyObjectPtr* keeper, SchemaField* out) {
	keeper->reset(PyObject_GetAttrString(cls, "TYPE"));
	if (!*keeper) {
		return false;
	}
	if (keeper->get() == Py_None) {
		PyErr_SetString(PyExc_TypeError, "ProtoCache alias TYPE is not set");
		return false;
	}
	return ParseAliasTypeSpec(keeper->get(), out);
}

static bool CheckSchemaField(PyObject* item) {
	if (!PyTuple_Check(item)) {
		PyErr_SetString(PyExc_TypeError, "ProtoCache schema item must be a tuple");
		return false;
	}
	if (PyTuple_GET_SIZE(item) != 6) {
		PyErr_SetString(PyExc_ValueError, "ProtoCache schema item must have 6 entries");
		return false;
	}
	return true;
}

static bool ParseSchemaField(PyObject* item, SchemaField* field) {
	if (!CheckSchemaField(item)) {
		return false;
	}
	field->name = PyTuple_GET_ITEM(item, 0);
	field->id = static_cast<unsigned>(PyLong_AsUnsignedLong(PyTuple_GET_ITEM(item, 1)));
	if (PyErr_Occurred()) {
		return false;
	}
	auto* repeated = PyTuple_GET_ITEM(item, 2);
	if (repeated != Py_True && repeated != Py_False) {
		PyErr_SetString(PyExc_TypeError, "ProtoCache schema repeated must be bool");
		return false;
	}
	field->repeated = repeated == Py_True;
	field->key_kind = static_cast<int>(PyLong_AsLong(PyTuple_GET_ITEM(item, 3)));
	if (PyErr_Occurred()) {
		return false;
	}
	field->value_kind = static_cast<int>(PyLong_AsLong(PyTuple_GET_ITEM(item, 4)));
	if (PyErr_Occurred()) {
		return false;
	}
	field->value_type = PyTuple_GET_ITEM(item, 5);
	return CheckValueType(field->value_kind, field->value_type);
}

static bool SerializeMessageObject(PyObject* obj, PyObject* schema, protocache::Buffer& buf, protocache::Unit& unit) {
	if (!PyTuple_Check(schema)) {
		PyErr_SetString(PyExc_TypeError, "ProtoCache schema must be a tuple");
		return false;
	}
	auto n = PyTuple_GET_SIZE(schema);

	unsigned max_id = 0;
	for (Py_ssize_t i = 0; i < n; i++) {
		auto* item = PyTuple_GET_ITEM(schema, i);
		if (!CheckSchemaField(item)) {
			return false;
		}
		auto field_id = static_cast<unsigned>(PyLong_AsUnsignedLong(PyTuple_GET_ITEM(item, 1)));
		if (PyErr_Occurred()) {
			return false;
		}
		if (field_id > max_id) {
			max_id = field_id;
		}
	}
	if (max_id > 12 + 25 * 255) {
		PyErr_SetString(PyExc_ValueError, "too many ProtoCache fields");
		return false;
	}

	std::vector<protocache::Unit> fields(max_id + 1);
	auto last = buf.Size();
	for (Py_ssize_t i = n; i-- > 0;) {
		SchemaField field;
		if (!ParseSchemaField(PyTuple_GET_ITEM(schema, i), &field)) {
			return false;
		}
		PyObjectPtr value(PyObject_GetAttr(obj, field.name));
		if (!value) {
			return false;
		}
		if (IsEmpty(value.get())) {
			continue;
		}
		protocache::Unit part;
		if (field.key_kind != KIND_NONE) {
			if (!SerializeMapObject(value.get(), field.key_kind, field.value_kind, field.value_type, buf, part)) {
				return false;
			}
		} else if (field.repeated) {
			if (!SerializeArrayObject(value.get(), field.value_kind, field.value_type, buf, part)) {
				return false;
			}
		} else if (!SerializeValue(value.get(), field.value_kind, field.value_type, buf, part)) {
			return false;
		}
		if (!field.repeated && field.key_kind == KIND_NONE && field.value_kind == KIND_MESSAGE && part.size() == 1) {
			if (part.len == 0) {
				buf.Shrink(1);
			}
			continue;
		}
		protocache::FoldField(buf, part);
		fields[static_cast<size_t>(field.id)] = part;
	}
	return protocache::SerializeMessage(fields, buf, last, unit);
}

template <typename T>
static bool ReadNumber(PyObject* value, T* out) {
	if constexpr (std::is_floating_point_v<T>) {
		auto v = PyFloat_AsDouble(value);
		if (PyErr_Occurred()) {
			return false;
		}
		*out = static_cast<T>(v);
		return true;
	} else if constexpr (std::is_unsigned_v<T>) {
		auto v = PyLong_AsUnsignedLongLong(value);
		if (PyErr_Occurred() || v > static_cast<unsigned long long>(std::numeric_limits<T>::max())) {
			if (!PyErr_Occurred()) {
				PyErr_SetString(PyExc_OverflowError, "integer out of range");
			}
			return false;
		}
		*out = static_cast<T>(v);
		return true;
	} else {
		auto v = PyLong_AsLongLong(value);
		if (PyErr_Occurred() ||
			v < static_cast<long long>(std::numeric_limits<T>::min()) ||
			v > static_cast<long long>(std::numeric_limits<T>::max())) {
			if (!PyErr_Occurred()) {
				PyErr_SetString(PyExc_OverflowError, "integer out of range");
			}
			return false;
		}
		*out = static_cast<T>(v);
		return true;
	}
}

template <typename T>
static bool SerializeNumber(PyObject* value, protocache::Buffer& buf, protocache::Unit& unit) {
	T v;
	if (!ReadNumber(value, &v)) {
		return false;
	}
	return protocache::Serialize(v, buf, unit);
}

static bool SerializeStringLike(PyObject* value, bool text, protocache::Buffer& buf, protocache::Unit& unit) {
	PyObjectPtr keeper;
	const char* data = nullptr;
	Py_ssize_t size = 0;
	if (text) {
		keeper.reset(PyUnicode_AsUTF8String(value));
		if (!keeper) {
			return false;
		}
		data = PyBytes_AS_STRING(keeper.get());
		size = PyBytes_GET_SIZE(keeper.get());
	} else if (PyBytes_Check(value)) {
		if (PyBytes_AsStringAndSize(value, const_cast<char**>(&data), &size) < 0) {
			return false;
		}
	} else {
		ScopedPyBuffer view;
		if (!view.Get(value, PyBUF_CONTIG_RO)) {
			return false;
		}
		auto* raw = view.get();
		return protocache::Serialize(protocache::Slice<char>(
				reinterpret_cast<const char*>(raw->buf), raw->len), buf, unit);
	}
	auto ok = protocache::Serialize(protocache::Slice<char>(data, static_cast<size_t>(size)), buf, unit);
	return ok;
}

template <typename T>
static bool KeyNumberBytes(PyObject* key, std::string* out) {
	T v;
	if (!NumberKey(key, &v)) {
		return false;
	}
	out->assign(reinterpret_cast<const char*>(&v), sizeof(v));
	return true;
}

static bool KeyBytes(PyObject* key, int kind, std::string* out) {
	switch (kind) {
		case KIND_STRING:
		{
			PyObjectPtr keeper(PyUnicode_AsUTF8String(key));
			if (!keeper) {
				return false;
			}
			if (!CheckStringKeySize(PyBytes_GET_SIZE(keeper.get()))) {
				return false;
			}
			out->assign(PyBytes_AS_STRING(keeper.get()), static_cast<size_t>(PyBytes_GET_SIZE(keeper.get())));
			return true;
		}
		case KIND_I32:
		case KIND_ENUM:
			return KeyNumberBytes<int32_t>(key, out);
		case KIND_U32:
			return KeyNumberBytes<uint32_t>(key, out);
		case KIND_I64:
			return KeyNumberBytes<int64_t>(key, out);
		case KIND_U64:
			return KeyNumberBytes<uint64_t>(key, out);
		default:
			PyErr_SetString(PyExc_ValueError, "unsupported ProtoCache map key type");
			return false;
	}
}

template <typename T>
static bool SerializeNumberMapKey(PyObject* key, protocache::Buffer& buf, protocache::Unit& unit) {
	return SerializeNumber<T>(key, buf, unit);
}

static bool SerializeMapKey(PyObject* key, int kind, protocache::Buffer& buf, protocache::Unit& unit) {
	switch (kind) {
		case KIND_STRING:
			return SerializeStringLike(key, true, buf, unit);
		case KIND_I32:
		case KIND_ENUM:
			return SerializeNumberMapKey<int32_t>(key, buf, unit);
		case KIND_U32:
			return SerializeNumberMapKey<uint32_t>(key, buf, unit);
		case KIND_I64:
			return SerializeNumberMapKey<int64_t>(key, buf, unit);
		case KIND_U64:
			return SerializeNumberMapKey<uint64_t>(key, buf, unit);
		default:
			PyErr_SetString(PyExc_ValueError, "unsupported ProtoCache map key type");
			return false;
	}
}

static bool SerializeBoolArrayObject(PyObject* value, protocache::Buffer& buf, protocache::Unit& unit) {
	PyObjectPtr seq(PySequence_Fast(value, "repeated field must be a sequence"));
	if (!seq) {
		return false;
	}
	auto n = PySequence_Fast_GET_SIZE(seq.get());
	std::vector<uint8_t> data(static_cast<size_t>(n));
	for (Py_ssize_t i = 0; i < n; i++) {
		PyObject* one = PySequence_Fast_GET_ITEM(seq.get(), i);
		if (!PyBool_Check(one)) {
			PyErr_SetString(PyExc_TypeError, "bool field must be bool");
			return false;
		}
		data[static_cast<size_t>(i)] = one == Py_True;
	}
	return protocache::Serialize(protocache::Slice<uint8_t>(data), buf, unit);
}

template <typename T>
static bool SerializeScalarArrayObject(PyObject* value, protocache::Buffer& buf, protocache::Unit& unit) {
	static_assert(std::is_scalar_v<T> && sizeof(T) % sizeof(uint32_t) == 0);
	PyObjectPtr seq(PySequence_Fast(value, "repeated field must be a sequence"));
	if (!seq) {
		return false;
	}
	auto n = PySequence_Fast_GET_SIZE(seq.get());
	constexpr unsigned m = sizeof(T) / sizeof(uint32_t);
	if (n == 0) {
		unit.len = 1;
		unit.data[0] = m;
		return true;
	}
	if (static_cast<unsigned long long>(n) * m >= (1ULL << 30U)) {
		PyErr_SetString(PyExc_OverflowError, "repeated field is too large");
		return false;
	}

	auto last = buf.Size();
	for (Py_ssize_t i = n; i-- > 0;) {
		T one;
		if (!ReadNumber(PySequence_Fast_GET_ITEM(seq.get(), i), &one)) {
			return false;
		}
		*reinterpret_cast<T*>(buf.Expand(m)) = one;
	}
	buf.Put((static_cast<uint32_t>(n) << 2U) | m);
	unit = protocache::Segment(last, buf.Size());
	return true;
}

static bool SerializeArrayObject(PyObject* value, int item_kind, PyObject* item_cls,
								 protocache::Buffer& buf, protocache::Unit& unit) {
	switch (item_kind) {
		case KIND_BOOL:
			return SerializeBoolArrayObject(value, buf, unit);
		case KIND_I32:
		case KIND_ENUM:
			return SerializeScalarArrayObject<int32_t>(value, buf, unit);
		case KIND_U32:
			return SerializeScalarArrayObject<uint32_t>(value, buf, unit);
		case KIND_I64:
			return SerializeScalarArrayObject<int64_t>(value, buf, unit);
		case KIND_U64:
			return SerializeScalarArrayObject<uint64_t>(value, buf, unit);
		case KIND_F32:
			return SerializeScalarArrayObject<float>(value, buf, unit);
		case KIND_F64:
			return SerializeScalarArrayObject<double>(value, buf, unit);
		default:
			break;
	}
	PyObjectPtr seq(PySequence_Fast(value, "repeated field must be a sequence"));
	if (!seq) {
		return false;
	}
	auto n = PySequence_Fast_GET_SIZE(seq.get());
	std::vector<protocache::Unit> elements(static_cast<size_t>(n));
	auto last = buf.Size();
	for (Py_ssize_t i = n; i-- > 0;) {
		PyObject* one = PySequence_Fast_GET_ITEM(seq.get(), i);
		if (!SerializeValue(one, item_kind, item_cls, buf, elements[static_cast<size_t>(i)])) {
			return false;
		}
	}
	return protocache::SerializeArray(elements, buf, last, unit);
}

static PyObject* MapItemPair(PyObject* items, Py_ssize_t index) {
	auto* pair = PyList_GET_ITEM(items, index);
	if (!PyTuple_Check(pair) || PyTuple_GET_SIZE(pair) != 2) {
		PyErr_SetString(PyExc_TypeError, "mapping items must be 2-tuples");
		return nullptr;
	}
	return pair;
}

static bool SerializeMapObject(PyObject* value, int key_kind, int value_kind, PyObject* value_type,
							   protocache::Buffer& buf, protocache::Unit& unit) {
	PyObjectPtr items(PyMapping_Items(value));
	if (!items) {
		return false;
	}
	if (!PyList_Check(items.get())) {
		PyErr_SetString(PyExc_TypeError, "mapping items must be a list");
		return false;
	}
	auto n = PyList_GET_SIZE(items.get());
	std::vector<std::string> keys(static_cast<size_t>(n));
	for (Py_ssize_t i = 0; i < n; i++) {
		PyObject* pair = MapItemPair(items.get(), i);
		if (pair == nullptr) {
			return false;
		}
		if (!KeyBytes(PyTuple_GET_ITEM(pair, 0), key_kind, &keys[static_cast<size_t>(i)])) {
			return false;
		}
	}
	PyKeyReader reader(keys);
	auto index = protocache::PerfectHashObject::Build(reader, true);
	if (!index) {
		PyErr_SetString(PyExc_ValueError, "failed to build ProtoCache map index");
		return false;
	}
	reader.Reset();
	std::vector<int> book(static_cast<size_t>(n));
	for (Py_ssize_t i = 0; i < n; i++) {
		auto key = reader.Read();
		auto pos = index.Locate(key.data(), key.size());
		if (pos >= book.size()) {
			PyErr_SetString(PyExc_ValueError, "failed to place ProtoCache map key");
			return false;
		}
		book[pos] = static_cast<int>(i);
	}

	std::vector<std::pair<protocache::Unit, protocache::Unit>> units(static_cast<size_t>(n));
	auto last = buf.Size();
	for (Py_ssize_t i = n; i-- > 0;) {
		PyObject* pair = MapItemPair(items.get(), book[static_cast<size_t>(i)]);
		if (pair == nullptr) {
			return false;
		}
		if (!SerializeValue(PyTuple_GET_ITEM(pair, 1), value_kind, value_type, buf, units[static_cast<size_t>(i)].second) ||
			!SerializeMapKey(PyTuple_GET_ITEM(pair, 0), key_kind, buf, units[static_cast<size_t>(i)].first)) {
			return false;
		}
	}
	return protocache::SerializeMap(index.Data(), units, buf, last, unit);
}

static bool SerializeValue(PyObject* value, int kind, PyObject* value_type,
						   protocache::Buffer& buf, protocache::Unit& unit) {
	switch (kind) {
		case KIND_BOOL:
		{
			if (!PyBool_Check(value)) {
				PyErr_SetString(PyExc_TypeError, "bool field must be bool");
				return false;
			}
			return protocache::Serialize(value == Py_True, buf, unit);
		}
		case KIND_I32:
		case KIND_ENUM:
			return SerializeNumber<int32_t>(value, buf, unit);
		case KIND_U32:
			return SerializeNumber<uint32_t>(value, buf, unit);
		case KIND_I64:
			return SerializeNumber<int64_t>(value, buf, unit);
		case KIND_U64:
			return SerializeNumber<uint64_t>(value, buf, unit);
		case KIND_F32:
			return SerializeNumber<float>(value, buf, unit);
		case KIND_F64:
			return SerializeNumber<double>(value, buf, unit);
		case KIND_STRING:
			return SerializeStringLike(value, true, buf, unit);
		case KIND_BYTES:
			return SerializeStringLike(value, false, buf, unit);
		case KIND_MESSAGE:
		{
			PyObject* raw_schema;
			if (!GetSchema(value, &raw_schema)) {
				return false;
			}
			PyObjectPtr schema(raw_schema);
			return SerializeMessageObject(value, schema.get(), buf, unit);
		}
		case KIND_ARRAY:
		{
			SchemaField nested;
			PyObjectPtr type;
			if (!ParseAliasType(value_type, &type, &nested)) {
				return false;
			}
			if (!nested.repeated || nested.key_kind != KIND_NONE) {
				PyErr_SetString(PyExc_TypeError, "ProtoCache array type expected");
				return false;
			}
			return SerializeArrayObject(value, nested.value_kind, nested.value_type, buf, unit);
		}
		case KIND_MAP:
		{
			SchemaField nested;
			PyObjectPtr type;
			if (!ParseAliasType(value_type, &type, &nested)) {
				return false;
			}
			if (!nested.repeated || nested.key_kind == KIND_NONE) {
				PyErr_SetString(PyExc_TypeError, "ProtoCache map type expected");
				return false;
			}
			return SerializeMapObject(value, nested.key_kind, nested.value_kind, nested.value_type, buf, unit);
		}
		default:
			PyErr_SetString(PyExc_ValueError, "unsupported ProtoCache kind");
			return false;
	}
}

static PyObject* Module_serialize_model(PyObject*, PyObject* args) {
	PyObject* obj;
	PyObject* schema;
	if (!PyArg_ParseTuple(args, "OO", &obj, &schema)) {
		return nullptr;
	}
	protocache::Buffer buf;
	protocache::Unit unit;
	if (!SerializeMessageObject(obj, schema, buf, unit)) {
		return nullptr;
	}
	return UnitToBytes(buf, unit);
}

static PyObject* Module_serialize_alias_array(PyObject*, PyObject* args) {
	PyObject* obj;
	int item_kind;
	PyObject* item_cls;
	if (!PyArg_ParseTuple(args, "OiO", &obj, &item_kind, &item_cls)) {
		return nullptr;
	}
	protocache::Buffer buf;
	protocache::Unit unit;
	if (!SerializeArrayObject(obj, item_kind, item_cls, buf, unit)) {
		return nullptr;
	}
	return UnitToBytes(buf, unit);
}

static PyObject* Module_serialize_alias_map(PyObject*, PyObject* args) {
	PyObject* obj;
	int key_kind;
	int value_kind;
	PyObject* value_type;
	if (!PyArg_ParseTuple(args, "OiiO", &obj, &key_kind, &value_kind, &value_type)) {
		return nullptr;
	}
	protocache::Buffer buf;
	protocache::Unit unit;
	if (!SerializeMapObject(obj, key_kind, value_kind, value_type, buf, unit)) {
		return nullptr;
	}
	return UnitToBytes(buf, unit);
}

static PyMethodDef MessageViewMethods[] = {
		{"from_bytes", reinterpret_cast<PyCFunction>(MessageView_from_bytes), METH_CLASS | METH_O,
		 "Create a message view from a bytes-like object."},
		{"has_field", reinterpret_cast<PyCFunction>(MessageView_has_field), METH_O,
		 "Return whether a field is present."},
		{"get", reinterpret_cast<PyCFunction>(MessageView_get), METH_VARARGS,
		 "Read a typed field."},
		{"get_array", reinterpret_cast<PyCFunction>(MessageView_get_array), METH_O,
		 "Read a repeated field as an array view."},
		{"get_map", reinterpret_cast<PyCFunction>(MessageView_get_map), METH_O,
		 "Read a map field as a map view."},
		{nullptr, nullptr, 0, nullptr},
};

static PyMethodDef ArrayViewMethods[] = {
		{"from_bytes", reinterpret_cast<PyCFunction>(ArrayView_from_bytes), METH_CLASS | METH_O,
		 "Create an array view from a bytes-like object."},
		{"size", reinterpret_cast<PyCFunction>(ArrayView_size), METH_O,
		 "Return array size for the supplied element kind."},
		{"get", reinterpret_cast<PyCFunction>(ArrayView_get), METH_VARARGS,
		 "Read a typed array element."},
		{nullptr, nullptr, 0, nullptr},
};

static PyMethodDef MapViewMethods[] = {
		{"from_bytes", reinterpret_cast<PyCFunction>(MapView_from_bytes), METH_CLASS | METH_O,
		 "Create a map view from a bytes-like object."},
		{"size", reinterpret_cast<PyCFunction>(MapView_size), METH_NOARGS,
		 "Return map size."},
		{"items", reinterpret_cast<PyCFunction>(MapView_items), METH_VARARGS,
		 "Return map items as typed Python values."},
		{"get", reinterpret_cast<PyCFunction>(MapView_get), METH_VARARGS,
		 "Find a map value by key."},
		{nullptr, nullptr, 0, nullptr},
};

static PyMethodDef ModuleMethods[] = {
		{"serialize_model", Module_serialize_model, METH_VARARGS,
		 "Serialize a materialized generated message object."},
		{"serialize_alias_array", Module_serialize_alias_array, METH_VARARGS,
		 "Serialize a materialized generated alias array."},
		{"serialize_alias_map", Module_serialize_alias_map, METH_VARARGS,
		 "Serialize a materialized generated alias map."},
		{nullptr, nullptr, 0, nullptr},
};

static PyModuleDef Module = {
		PyModuleDef_HEAD_INIT,
		"protocache._protocache",
		"Low-level ProtoCache helpers.",
		-1,
		ModuleMethods,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
};

static bool AddIntConstant(PyObject* module, const char* name, long value) {
	return PyModule_AddIntConstant(module, name, value) == 0;
}

static bool AddObjectRef(PyObject* module, const char* name, PyObject* value) {
	Py_INCREF(value);
	if (PyModule_AddObject(module, name, value) == 0) {
		return true;
	}
	Py_DECREF(value);
	return false;
}

} // namespace

PyMODINIT_FUNC PyInit__protocache() {
	MessageViewType.tp_name = "protocache._protocache.MessageView";
	MessageViewType.tp_basicsize = sizeof(MessageView);
	MessageViewType.tp_dealloc = reinterpret_cast<destructor>(View_dealloc<MessageView>);
	MessageViewType.tp_flags = Py_TPFLAGS_DEFAULT;
	MessageViewType.tp_doc = "ProtoCache message view";
	MessageViewType.tp_methods = MessageViewMethods;

	ArrayViewType.tp_name = "protocache._protocache.ArrayView";
	ArrayViewType.tp_basicsize = sizeof(ArrayView);
	ArrayViewType.tp_dealloc = reinterpret_cast<destructor>(View_dealloc<ArrayView>);
	ArrayViewType.tp_flags = Py_TPFLAGS_DEFAULT;
	ArrayViewType.tp_doc = "ProtoCache array view";
	ArrayViewType.tp_methods = ArrayViewMethods;

	MapViewType.tp_name = "protocache._protocache.MapView";
	MapViewType.tp_basicsize = sizeof(MapView);
	MapViewType.tp_dealloc = reinterpret_cast<destructor>(View_dealloc<MapView>);
	MapViewType.tp_flags = Py_TPFLAGS_DEFAULT;
	MapViewType.tp_doc = "ProtoCache map view";
	MapViewType.tp_methods = MapViewMethods;

	if (PyType_Ready(&MessageViewType) < 0 ||
		PyType_Ready(&ArrayViewType) < 0 ||
		PyType_Ready(&MapViewType) < 0) {
		return nullptr;
	}

	PyObjectPtr module(PyModule_Create(&Module));
	if (!module) {
		return nullptr;
	}

	if (!AddObjectRef(module.get(), "MessageView", reinterpret_cast<PyObject*>(&MessageViewType)) ||
		!AddObjectRef(module.get(), "ArrayView", reinterpret_cast<PyObject*>(&ArrayViewType)) ||
		!AddObjectRef(module.get(), "MapView", reinterpret_cast<PyObject*>(&MapViewType)) ||
		!AddIntConstant(module.get(), "NONE", KIND_NONE) ||
		!AddIntConstant(module.get(), "MESSAGE", KIND_MESSAGE) ||
		!AddIntConstant(module.get(), "BYTES", KIND_BYTES) ||
		!AddIntConstant(module.get(), "STRING", KIND_STRING) ||
		!AddIntConstant(module.get(), "F64", KIND_F64) ||
		!AddIntConstant(module.get(), "F32", KIND_F32) ||
		!AddIntConstant(module.get(), "U64", KIND_U64) ||
		!AddIntConstant(module.get(), "U32", KIND_U32) ||
		!AddIntConstant(module.get(), "I64", KIND_I64) ||
		!AddIntConstant(module.get(), "I32", KIND_I32) ||
		!AddIntConstant(module.get(), "BOOL", KIND_BOOL) ||
		!AddIntConstant(module.get(), "ENUM", KIND_ENUM) ||
		!AddIntConstant(module.get(), "ARRAY", KIND_ARRAY) ||
		!AddIntConstant(module.get(), "MAP", KIND_MAP)) {
		return nullptr;
	}

	return module.release();
}
