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

struct CompiledType {
	CompiledType() noexcept = default;
	CompiledType(const CompiledType&) = delete;
	CompiledType& operator=(const CompiledType&) = delete;
	CompiledType(CompiledType&& other) noexcept
			: repeated(other.repeated),
			  key_kind(other.key_kind),
			  value_kind(other.value_kind),
			  value_type(other.value_type) {
		other.value_type = nullptr;
	}
	CompiledType& operator=(CompiledType&& other) noexcept {
		if (this != &other) {
			Py_XDECREF(value_type);
			repeated = other.repeated;
			key_kind = other.key_kind;
			value_kind = other.value_kind;
			value_type = other.value_type;
			other.value_type = nullptr;
		}
		return *this;
	}
	~CompiledType() {
		Py_XDECREF(value_type);
	}

	void SetValueType(PyObject* value) noexcept {
		Py_XINCREF(value);
		Py_XDECREF(value_type);
		value_type = value;
	}

	bool repeated = false;
	int key_kind = 0;
	int value_kind = 0;
	PyObject* value_type = nullptr;
};

struct SchemaField {
	SchemaField() noexcept = default;
	SchemaField(const SchemaField&) = delete;
	SchemaField& operator=(const SchemaField&) = delete;
	SchemaField(SchemaField&& other) noexcept
			: name(other.name),
			  id(other.id),
			  type(std::move(other.type)) {
		other.name = nullptr;
	}
	SchemaField& operator=(SchemaField&& other) noexcept {
		if (this != &other) {
			Py_XDECREF(name);
			name = other.name;
			id = other.id;
			type = std::move(other.type);
			other.name = nullptr;
		}
		return *this;
	}
	~SchemaField() {
		Py_XDECREF(name);
	}

	void SetName(PyObject* value) noexcept {
		Py_XINCREF(value);
		Py_XDECREF(name);
		name = value;
	}

	PyObject* name = nullptr;
	unsigned id = 0;
	CompiledType type;
};

struct Schema {
	std::vector<SchemaField> fields;
	unsigned max_id = 0;
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

struct PySchema {
	PyObject_HEAD
	Schema schema;
};

struct PyCompiledTypeObject {
	PyObject_HEAD
	CompiledType type;
};

PyTypeObject MessageViewType = {PyVarObject_HEAD_INIT(nullptr, 0)};
PyTypeObject ArrayViewType = {PyVarObject_HEAD_INIT(nullptr, 0)};
PyTypeObject MapViewType = {PyVarObject_HEAD_INIT(nullptr, 0)};
PyTypeObject SchemaType = {PyVarObject_HEAD_INIT(nullptr, 0)};
PyTypeObject CompiledTypeType = {PyVarObject_HEAD_INIT(nullptr, 0)};

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

template <typename T>
static PyObject* ScalarToPy(T value) {
	if constexpr (std::is_same_v<T, bool>) {
		return PyBool_FromLong(value);
	} else if constexpr (std::is_same_v<T, uint32_t>) {
		return PyLong_FromUnsignedLong(value);
	} else if constexpr (std::is_same_v<T, uint64_t>) {
		return PyLong_FromUnsignedLongLong(value);
	} else if constexpr (std::is_same_v<T, int64_t>) {
		return PyLong_FromLongLong(value);
	} else if constexpr (std::is_floating_point_v<T>) {
		return PyFloat_FromDouble(value);
	} else {
		return PyLong_FromLong(value);
	}
}

template <typename T>
static PyObject* ArrayScalarToList(const uint32_t* ptr, const uint32_t* end) {
	protocache::ArrayT<T> view(ptr, end);
	if (!view) {
		return PyList_New(0);
	}
	auto size = view.Size();
	PyObjectPtr out(PyList_New(static_cast<Py_ssize_t>(size)));
	if (!out) {
		return nullptr;
	}
	for (uint32_t i = 0; i < size; i++) {
		PyObject* item = ScalarToPy<T>(view[i]);
		if (item == nullptr) {
			return nullptr;
		}
		PyList_SET_ITEM(out.get(), static_cast<Py_ssize_t>(i), item);
	}
	return out.release();
}

static PyObject* ArrayFieldToList(const uint32_t* ptr, const uint32_t* end, int kind,
								  const Storage& storage) {
	protocache::Array view(ptr, end);
	if (!view) {
		return PyList_New(0);
	}
	auto size = view.Size();
	PyObjectPtr out(PyList_New(static_cast<Py_ssize_t>(size)));
	if (!out) {
		return nullptr;
	}
	for (uint32_t i = 0; i < size; i++) {
		PyObject* item = FieldToPy(view[i], kind, storage, end);
		if (item == nullptr) {
			return nullptr;
		}
		PyList_SET_ITEM(out.get(), static_cast<Py_ssize_t>(i), item);
	}
	return out.release();
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

static PyObject* ArrayView_to_list(ArrayView* self, PyObject* arg) {
	int kind = static_cast<int>(PyLong_AsLong(arg));
	if (PyErr_Occurred()) {
		return nullptr;
	}
	if (self->ptr == nullptr) {
		return PyList_New(0);
	}
	switch (kind) {
		case KIND_BYTES:
		case KIND_STRING:
			return ArrayFieldToList(self->ptr, self->end, kind, self->storage);
		case KIND_BOOL:
			return ArrayScalarToList<bool>(self->ptr, self->end);
		case KIND_I32:
		case KIND_ENUM:
			return ArrayScalarToList<int32_t>(self->ptr, self->end);
		case KIND_U32:
			return ArrayScalarToList<uint32_t>(self->ptr, self->end);
		case KIND_I64:
			return ArrayScalarToList<int64_t>(self->ptr, self->end);
		case KIND_U64:
			return ArrayScalarToList<uint64_t>(self->ptr, self->end);
		case KIND_F32:
			return ArrayScalarToList<float>(self->ptr, self->end);
		case KIND_F64:
			return ArrayScalarToList<double>(self->ptr, self->end);
		default:
			PyErr_SetString(PyExc_ValueError, "ProtoCache array kind is not scalar");
			return nullptr;
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

static PyObject* MapView_to_dict(MapView* self, PyObject* args) {
	int key_kind;
	int value_kind;
	if (!PyArg_ParseTuple(args, "ii", &key_kind, &value_kind)) {
		return nullptr;
	}
	PyObjectPtr out(PyDict_New());
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
		if (PyDict_SetItem(out.get(), key.get(), value.get()) < 0) {
			return nullptr;
		}
	}
	return out.release();
}

static bool StringKey(PyObject* key, const char** data, Py_ssize_t* size, PyObject** keeper) {
	*keeper = nullptr;
	if (PyUnicode_Check(key)) {
		*data = PyUnicode_AsUTF8AndSize(key, size);
		if (*data == nullptr) {
			return false;
		}
		if (!CheckStringKeySize(*size)) {
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
	if (PyBool_Check(key)) {
		PyErr_SetString(PyExc_TypeError, "integer map key must not be bool");
		return false;
	}
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

struct MapEntry {
	PyObject* key = nullptr;
	PyObject* value = nullptr;
	std::string key_bytes;
};

struct PyKeyReader final : public protocache::KeyReader {
	explicit PyKeyReader(const std::vector<MapEntry>& entries) : entries_(entries) {}
	void Reset() override { idx_ = 0; }
	size_t Total() override { return entries_.size(); }
	protocache::Slice<uint8_t> Read() override {
		if (idx_ >= entries_.size()) {
			return {};
		}
		auto& key = entries_[idx_++].key_bytes;
		return {reinterpret_cast<const uint8_t*>(key.data()), key.size()};
	}

private:
	const std::vector<MapEntry>& entries_;
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

static PyObject* CompileSchemaObject(PyObject* schema_obj);
static PyObject* CompileContainerSchemaSpecObject(PyObject* schema_obj);
static bool IsScalarKind(int kind);

static PyObject* GetOwnClassAttr(PyObject* cls, const char* name) {
	if (!PyType_Check(cls)) {
		Py_RETURN_NONE;
	}
	auto* dict = reinterpret_cast<PyTypeObject*>(cls)->tp_dict;
	if (dict == nullptr) {
		Py_RETURN_NONE;
	}
	PyObject* value = PyDict_GetItemString(dict, name);
	if (value == nullptr) {
		Py_RETURN_NONE;
	}
	Py_INCREF(value);
	return value;
}

static PyObject* GetCachedInternalSchemaObject(PyObject* cls, PyTypeObject* expected_type,
											   const char* invalid_message) {
	PyObjectPtr cached(GetOwnClassAttr(cls, "_internal_schema"));
	if (!cached) {
		return nullptr;
	}
	if (cached.get() != Py_None) {
		if (!PyObject_TypeCheck(cached.get(), expected_type)) {
			PyErr_SetString(PyExc_TypeError, invalid_message);
			return nullptr;
		}
		return cached.release();
	}
	return cached.release();
}

static PyObject* RuntimeClass(const char* name) {
	static PyObject* array_cls = nullptr;
	static PyObject* map_cls = nullptr;
	PyObject** cached = std::strcmp(name, "Array") == 0 ? &array_cls : &map_cls;
	if (*cached != nullptr) {
		Py_INCREF(*cached);
		return *cached;
	}
	PyObjectPtr module(PyImport_ImportModule("protocache"));
	if (!module) {
		return nullptr;
	}
	PyObject* cls = PyObject_GetAttrString(module.get(), name);
	if (cls == nullptr) {
		return nullptr;
	}
	*cached = cls;
	Py_INCREF(cls);
	return cls;
}

static PyObject* NewContainer(PyObject* cls, const char* fallback, PyObject* arg = nullptr) {
	PyObjectPtr actual(cls);
	if (actual.get() == nullptr) {
		actual.reset(RuntimeClass(fallback));
		if (!actual) {
			return nullptr;
		}
	} else {
		Py_INCREF(actual.get());
	}
	if (arg == nullptr) {
		return PyObject_CallObject(actual.get(), nullptr);
	}
	return PyObject_CallFunctionObjArgs(actual.get(), arg, nullptr);
}

static PyObject* DefaultValue(const CompiledType& type) {
	if (type.key_kind != KIND_NONE) {
		return NewContainer(nullptr, "Map");
	}
	if (type.repeated) {
		return NewContainer(nullptr, "Array");
	}
	switch (type.value_kind) {
		case KIND_BOOL:
			return PyBool_FromLong(0);
		case KIND_I32:
		case KIND_U32:
		case KIND_I64:
		case KIND_U64:
		case KIND_ENUM:
			return PyLong_FromLong(0);
		case KIND_F32:
		case KIND_F64:
			return PyFloat_FromDouble(0.0);
		case KIND_STRING:
			return PyUnicode_FromStringAndSize("", 0);
		case KIND_BYTES:
			return PyBytes_FromStringAndSize("", 0);
		case KIND_MESSAGE:
			Py_RETURN_NONE;
		case KIND_ARRAY:
		case KIND_MAP:
			return NewContainer(type.value_type, nullptr);
		default:
			Py_RETURN_NONE;
	}
}

static PyObject* GetMessageSchemaObject(PyObject* cls) {
	PyObjectPtr cached(GetCachedInternalSchemaObject(
			cls, &SchemaType, "ProtoCache message _internal_schema must be a ProtoCache Schema"));
	if (!cached || cached.get() != Py_None) {
		return cached.release();
	}
	PyObject* schema = PyObject_CallMethod(cls, "_get_internal_schema", nullptr);
	if (schema == nullptr) {
		return nullptr;
	}
	if (!PyObject_TypeCheck(schema, &SchemaType)) {
		Py_DECREF(schema);
		PyErr_SetString(PyExc_TypeError, "ProtoCache message _internal_schema must be a ProtoCache Schema");
		return nullptr;
	}
	return schema;
}

static bool IsCompiledArrayType(const CompiledType& type) {
	return type.repeated && type.key_kind == KIND_NONE;
}

static bool IsCompiledMapType(const CompiledType& type) {
	return type.repeated && type.key_kind != KIND_NONE;
}

static PyObject* GetContainerSchemaObject(PyObject* cls) {
	PyObjectPtr cached(GetCachedInternalSchemaObject(
			cls, &CompiledTypeType, "ProtoCache container _internal_schema must be a compiled ProtoCache type"));
	if (!cached || cached.get() != Py_None) {
		return cached.release();
	}
	PyObject* schema = PyObject_CallMethod(cls, "_get_internal_schema", nullptr);
	if (schema == nullptr) {
		return nullptr;
	}
	if (!PyObject_TypeCheck(schema, &CompiledTypeType)) {
		Py_DECREF(schema);
		PyErr_SetString(PyExc_TypeError, "ProtoCache container _internal_schema must be a compiled ProtoCache type");
		return nullptr;
	}
	return schema;
}

static PyObject* NestedContainerTypeObject(const CompiledType& type, int kind) {
	PyObjectPtr nested_obj(GetContainerSchemaObject(type.value_type));
	if (!nested_obj) {
		return nullptr;
	}
	const auto& nested = reinterpret_cast<PyCompiledTypeObject*>(nested_obj.get())->type;
	const char* name = kind == KIND_ARRAY ? "array" : "map";
	bool ok = kind == KIND_ARRAY ? IsCompiledArrayType(nested) : IsCompiledMapType(nested);
	if (!ok) {
		PyErr_Format(PyExc_TypeError, "ProtoCache %s type expected", name);
		return nullptr;
	}
	return nested_obj.release();
}

static const CompiledType* NestedContainerType(const CompiledType& type, int kind,
											  PyObjectPtr* owner) {
	owner->reset(NestedContainerTypeObject(type, kind));
	if (!*owner) {
		return nullptr;
	}
	return &reinterpret_cast<PyCompiledTypeObject*>(owner->get())->type;
}

static PyObject* MaterializeMessage(PyObject* cls, const Storage& storage,
									const uint32_t* ptr, const uint32_t* end,
									const Schema& schema);
static PyObject* MaterializeValue(const protocache::Field& field, const CompiledType& type,
								  const Storage& storage, const uint32_t* end);

static PyObject* MaterializeArray(const uint32_t* ptr, const CompiledType& type,
								  PyObject* cls, const Storage& storage, const uint32_t* end) {
	auto item_kind = type.value_kind;
	if (IsScalarKind(item_kind)) {
		PyObjectPtr values;
		switch (item_kind) {
			case KIND_BYTES:
			case KIND_STRING:
				values.reset(ArrayFieldToList(ptr, end, item_kind, storage));
				break;
			case KIND_BOOL:
				values.reset(ArrayScalarToList<bool>(ptr, end));
				break;
			case KIND_I32:
			case KIND_ENUM:
				values.reset(ArrayScalarToList<int32_t>(ptr, end));
				break;
			case KIND_U32:
				values.reset(ArrayScalarToList<uint32_t>(ptr, end));
				break;
			case KIND_I64:
				values.reset(ArrayScalarToList<int64_t>(ptr, end));
				break;
			case KIND_U64:
				values.reset(ArrayScalarToList<uint64_t>(ptr, end));
				break;
			case KIND_F32:
				values.reset(ArrayScalarToList<float>(ptr, end));
				break;
			case KIND_F64:
				values.reset(ArrayScalarToList<double>(ptr, end));
				break;
			default:
				PyErr_SetString(PyExc_ValueError, "unsupported ProtoCache scalar array kind");
				return nullptr;
		}
		if (!values) {
			return nullptr;
		}
		return NewContainer(cls, "Array", values.get());
	}
	PyObjectPtr out(NewContainer(cls, "Array"));
	if (!out) {
		return nullptr;
	}
	if (ptr == nullptr) {
		return out.release();
	}
	protocache::Array view(ptr, end);
	if (!view) {
		return out.release();
	}
	auto size = view.Size();
	for (uint32_t i = 0; i < size; i++) {
		PyObjectPtr value(MaterializeValue(view[i], type, storage, end));
		if (!value) {
			return nullptr;
		}
		if (PyList_Append(out.get(), value.get()) < 0) {
			return nullptr;
		}
	}
	return out.release();
}

static PyObject* MaterializeMap(const uint32_t* ptr, const CompiledType& type,
								PyObject* cls, const Storage& storage, const uint32_t* end) {
	PyObjectPtr out(NewContainer(cls, "Map"));
	if (!out) {
		return nullptr;
	}
	if (ptr == nullptr) {
		return out.release();
	}
	protocache::Map view(ptr, end);
	if (!view) {
		return out.release();
	}
	for (auto pair : view) {
		PyObjectPtr key(FieldToPy(pair.Key(), type.key_kind, storage, end));
		if (!key) {
			return nullptr;
		}
		PyObjectPtr value(MaterializeValue(pair.Value(), type, storage, end));
		if (!value) {
			return nullptr;
		}
		if (PyObject_SetItem(out.get(), key.get(), value.get()) < 0) {
			return nullptr;
		}
	}
	return out.release();
}

static PyObject* MaterializeContainer(const uint32_t* ptr, const CompiledType& type,
									  PyObject* cls, const Storage& storage, const uint32_t* end) {
	if (IsCompiledArrayType(type)) {
		return MaterializeArray(ptr, type, cls, storage, end);
	}
	if (IsCompiledMapType(type)) {
		return MaterializeMap(ptr, type, cls, storage, end);
	}
	PyErr_SetString(PyExc_TypeError, "ProtoCache array or map type expected");
	return nullptr;
}

static PyObject* MaterializeValue(const protocache::Field& field, const CompiledType& type,
								  const Storage& storage, const uint32_t* end) {
	if (IsScalarKind(type.value_kind)) {
		return FieldToPy(field, type.value_kind, storage, end);
	}
	switch (type.value_kind) {
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
			PyObject* raw_schema = GetMessageSchemaObject(type.value_type);
			if (raw_schema == nullptr) {
				return nullptr;
			}
			PyObjectPtr schema(raw_schema);
			return MaterializeMessage(type.value_type, storage, ptr, end,
									  reinterpret_cast<PySchema*>(schema.get())->schema);
		}
		case KIND_ARRAY:
		{
			PyObjectPtr nested_owner;
			const auto* nested = NestedContainerType(type, KIND_ARRAY, &nested_owner);
			if (nested == nullptr) {
				return nullptr;
			}
			return MaterializeArray(field.GetObject(end), *nested, type.value_type, storage, end);
		}
		case KIND_MAP:
		{
			PyObjectPtr nested_owner;
			const auto* nested = NestedContainerType(type, KIND_MAP, &nested_owner);
			if (nested == nullptr) {
				return nullptr;
			}
			return MaterializeMap(field.GetObject(end), *nested, type.value_type, storage, end);
		}
		default:
			PyErr_SetString(PyExc_ValueError, "unsupported ProtoCache kind");
			return nullptr;
	}
}

static PyObject* MaterializeMessage(PyObject* cls, const Storage& storage,
									const uint32_t* ptr, const uint32_t* end,
									const Schema& schema) {
	protocache::Message msg(ptr, end);
	if (!msg) {
		PyErr_SetString(PyExc_ValueError, "invalid ProtoCache message");
		return nullptr;
	}
	PyObjectPtr obj(PyObject_CallMethod(cls, "__new__", "O", cls));
	if (!obj) {
		return nullptr;
	}
	PyObjectPtr present(PySet_New(nullptr));
	if (!present) {
		return nullptr;
	}
	if (PyObject_SetAttrString(obj.get(), "_present", present.get()) < 0) {
		return nullptr;
	}
	for (const auto& field : schema.fields) {
		PyObjectPtr value;
		if (msg.HasField(field.id, end)) {
			PyObjectPtr id(PyLong_FromUnsignedLong(field.id));
			if (!id || PySet_Add(present.get(), id.get()) < 0) {
				return nullptr;
			}
			auto raw = msg.GetField(field.id, end);
			if (field.type.repeated || field.type.key_kind != KIND_NONE) {
				value.reset(MaterializeContainer(raw.GetObject(end), field.type, nullptr, storage, end));
			} else {
				value.reset(MaterializeValue(raw, field.type, storage, end));
			}
		} else {
			value.reset(DefaultValue(field.type));
		}
		if (!value) {
			return nullptr;
		}
		if (PyObject_SetAttr(obj.get(), field.name, value.get()) < 0) {
			return nullptr;
		}
	}
	return obj.release();
}

static bool LongIsZero(PyObject* value) {
	if (PyBool_Check(value) || !PyLong_Check(value)) {
		return false;
	}
	auto zero = PyLong_AsLongLong(value);
	if (PyErr_Occurred()) {
		PyErr_Clear();
		return false;
	}
	return zero == 0;
}

static bool FloatIsZero(PyObject* value) {
	auto zero = PyFloat_AsDouble(value);
	if (PyErr_Occurred()) {
		PyErr_Clear();
		return false;
	}
	return zero == 0.0;
}

static bool BytesLikeIsEmpty(PyObject* value) {
	if (PyBytes_Check(value)) {
		return PyBytes_GET_SIZE(value) == 0;
	}
	ScopedPyBuffer view;
	if (!view.Get(value, PyBUF_CONTIG_RO)) {
		PyErr_Clear();
		return false;
	}
	return view.get()->len == 0;
}

static bool IsDefaultField(const SchemaField& field, PyObject* value) {
	if (value == Py_None) {
		return true;
	}
	const auto& type = field.type;
	if (type.key_kind != KIND_NONE) {
		return PyDict_Check(value) && PyDict_Size(value) == 0;
	}
	if (type.repeated) {
		return PyList_Check(value) && PyList_GET_SIZE(value) == 0;
	}
	switch (type.value_kind) {
		case KIND_BOOL:
			return value == Py_False;
		case KIND_I32:
		case KIND_U32:
		case KIND_I64:
		case KIND_U64:
		case KIND_ENUM:
			return LongIsZero(value);
		case KIND_F32:
		case KIND_F64:
			return FloatIsZero(value);
		case KIND_STRING:
			return PyUnicode_Check(value) && PyUnicode_GET_LENGTH(value) == 0;
		case KIND_BYTES:
			return BytesLikeIsEmpty(value);
		case KIND_MESSAGE:
			return false;
		case KIND_ARRAY:
			return PyList_Check(value) && PyList_GET_SIZE(value) == 0;
		case KIND_MAP:
			return PyDict_Check(value) && PyDict_Size(value) == 0;
		default:
			return false;
	}
}

static bool SerializeValue(PyObject* value, const CompiledType& type,
						   protocache::Buffer& buf, protocache::Unit& unit);
static bool SerializeArrayObject(PyObject* value, const CompiledType& type,
								 protocache::Buffer& buf, protocache::Unit& unit);
static bool SerializeMapObject(PyObject* value, const CompiledType& type,
							   protocache::Buffer& buf, protocache::Unit& unit);
static bool SerializeContainerObject(PyObject* value, const CompiledType& type,
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
			PyErr_SetString(PyExc_TypeError, "scalar ProtoCache schema must not have value_type");
			return false;
		}
		return true;
	}
	if (kind != KIND_MESSAGE && kind != KIND_ARRAY && kind != KIND_MAP) {
		PyErr_SetString(PyExc_ValueError, "invalid ProtoCache value kind");
		return false;
	}
	if (!PyType_Check(value_type)) {
		PyErr_SetString(PyExc_TypeError, "complex ProtoCache schema must have class value_type");
		return false;
	}
	return true;
}

static bool CompileTypeParts(bool repeated, int key_kind, int value_kind,
							 PyObject* value_type, CompiledType* out) {
	out->repeated = repeated;
	out->key_kind = key_kind;
	out->value_kind = value_kind;
	out->SetValueType(value_type);
	return CheckValueType(out->value_kind, out->value_type);
}

static bool ParseContainerSchemaSpec(PyObject* spec, CompiledType* out) {
	if (!PyTuple_Check(spec)) {
		PyErr_SetString(PyExc_TypeError, "ProtoCache container schema must be a tuple");
		return false;
	}
	if (PyTuple_GET_SIZE(spec) != 3) {
		PyErr_SetString(PyExc_ValueError, "ProtoCache container schema must have 3 entries");
		return false;
	}
	int key_kind = static_cast<int>(PyLong_AsLong(PyTuple_GET_ITEM(spec, 0)));
	if (PyErr_Occurred()) {
		return false;
	}
	int value_kind = static_cast<int>(PyLong_AsLong(PyTuple_GET_ITEM(spec, 1)));
	if (PyErr_Occurred()) {
		return false;
	}
	return CompileTypeParts(true, key_kind, value_kind, PyTuple_GET_ITEM(spec, 2), out);
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
	field->SetName(PyTuple_GET_ITEM(item, 0));
	field->id = static_cast<unsigned>(PyLong_AsUnsignedLong(PyTuple_GET_ITEM(item, 1)));
	if (PyErr_Occurred()) {
		return false;
	}
	auto* repeated = PyTuple_GET_ITEM(item, 2);
	if (repeated != Py_True && repeated != Py_False) {
		PyErr_SetString(PyExc_TypeError, "ProtoCache schema repeated must be bool");
		return false;
	}
	int key_kind = static_cast<int>(PyLong_AsLong(PyTuple_GET_ITEM(item, 3)));
	if (PyErr_Occurred()) {
		return false;
	}
	int value_kind = static_cast<int>(PyLong_AsLong(PyTuple_GET_ITEM(item, 4)));
	if (PyErr_Occurred()) {
		return false;
	}
	return CompileTypeParts(repeated == Py_True, key_kind, value_kind,
							PyTuple_GET_ITEM(item, 5), &field->type);
}

static void Schema_dealloc(PySchema* self) {
	self->schema.~Schema();
	Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

static void CompiledType_dealloc(PyCompiledTypeObject* self) {
	self->type.~CompiledType();
	Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

static PyObject* CompileSchemaObject(PyObject* schema_obj) {
	if (PyObject_TypeCheck(schema_obj, &SchemaType)) {
		Py_INCREF(schema_obj);
		return schema_obj;
	}
	if (!PyTuple_Check(schema_obj)) {
		PyErr_SetString(PyExc_TypeError, "ProtoCache schema must be a tuple");
		return nullptr;
	}
	auto n = PyTuple_GET_SIZE(schema_obj);

	auto* out = PyObject_New(PySchema, &SchemaType);
	if (out == nullptr) {
		return nullptr;
	}
	new (&out->schema) Schema();
	PyObjectPtr keeper(reinterpret_cast<PyObject*>(out));
	try {
		out->schema.fields.reserve(static_cast<size_t>(n));
	} catch (const std::bad_alloc&) {
		PyErr_NoMemory();
		return nullptr;
	}

	for (Py_ssize_t i = 0; i < n; i++) {
		SchemaField field;
		if (!ParseSchemaField(PyTuple_GET_ITEM(schema_obj, i), &field)) {
			return nullptr;
		}
		if (field.id > out->schema.max_id) {
			out->schema.max_id = field.id;
		}
		try {
			out->schema.fields.emplace_back(std::move(field));
		} catch (const std::bad_alloc&) {
			PyErr_NoMemory();
			return nullptr;
		}
	}
	if (out->schema.max_id > 12 + 25 * 255) {
		PyErr_SetString(PyExc_ValueError, "too many ProtoCache fields");
		return nullptr;
	}
	return keeper.release();
}

static PyObject* CompileContainerSchemaSpecObject(PyObject* schema_obj) {
	if (PyObject_TypeCheck(schema_obj, &CompiledTypeType)) {
		Py_INCREF(schema_obj);
		return schema_obj;
	}
	auto* out = PyObject_New(PyCompiledTypeObject, &CompiledTypeType);
	if (out == nullptr) {
		return nullptr;
	}
	new (&out->type) CompiledType();
	PyObjectPtr keeper(reinterpret_cast<PyObject*>(out));
	if (!ParseContainerSchemaSpec(schema_obj, &out->type)) {
		return nullptr;
	}
	return keeper.release();
}

static PyObject* MessageInstanceDict(PyObject* obj) {
	PyObject** dict_ptr = _PyObject_GetDictPtr(obj);
	if (dict_ptr == nullptr || *dict_ptr == nullptr || !PyDict_Check(*dict_ptr)) {
		PyErr_SetString(PyExc_TypeError, "ProtoCache message object must have instance dict");
		return nullptr;
	}
	return *dict_ptr;
}

static bool SerializeMessageObject(PyObject* obj, const Schema& schema, protocache::Buffer& buf, protocache::Unit& unit) {
	std::vector<protocache::Unit> fields(schema.max_id + 1);
	PyObject* dict = MessageInstanceDict(obj);
	if (dict == nullptr) {
		return false;
	}
	auto last = buf.Size();
	for (auto it = schema.fields.rbegin(); it != schema.fields.rend(); ++it) {
		const auto& field = *it;
		PyObject* value = PyDict_GetItemWithError(dict, field.name);
		if (value == nullptr) {
			if (PyErr_Occurred()) {
				return false;
			}
			continue;
		}
		if (IsDefaultField(field, value)) {
			continue;
		}
		protocache::Unit part;
		if (field.type.repeated || field.type.key_kind != KIND_NONE) {
			if (!SerializeContainerObject(value, field.type, buf, part)) {
				return false;
			}
		} else if (!SerializeValue(value, field.type, buf, part)) {
			return false;
		}
		if (!field.type.repeated && field.type.key_kind == KIND_NONE &&
			field.type.value_kind == KIND_MESSAGE && part.size() == 1) {
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
		if (PyBool_Check(value)) {
			PyErr_SetString(PyExc_TypeError, "integer field must not be bool");
			return false;
		}
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
		if (PyBool_Check(value)) {
			PyErr_SetString(PyExc_TypeError, "integer field must not be bool");
			return false;
		}
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
	const char* data = nullptr;
	Py_ssize_t size = 0;
	if (text) {
		data = PyUnicode_AsUTF8AndSize(value, &size);
		if (data == nullptr) {
			return false;
		}
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
			Py_ssize_t size = 0;
			const char* data = PyUnicode_AsUTF8AndSize(key, &size);
			if (data == nullptr) {
				return false;
			}
			if (!CheckStringKeySize(size)) {
				return false;
			}
			out->assign(data, static_cast<size_t>(size));
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
static bool SerializeCachedNumberMapKey(const MapEntry& entry, protocache::Buffer& buf, protocache::Unit& unit) {
	if (entry.key_bytes.size() != sizeof(T)) {
		PyErr_SetString(PyExc_ValueError, "invalid cached ProtoCache map key");
		return false;
	}
	T value;
	std::memcpy(&value, entry.key_bytes.data(), sizeof(T));
	return protocache::Serialize(value, buf, unit);
}

static bool SerializeCachedMapKey(const MapEntry& entry, int kind, protocache::Buffer& buf, protocache::Unit& unit) {
	switch (kind) {
		case KIND_STRING:
			return protocache::Serialize(
					protocache::Slice<char>(entry.key_bytes.data(), entry.key_bytes.size()), buf, unit);
		case KIND_I32:
		case KIND_ENUM:
			return SerializeCachedNumberMapKey<int32_t>(entry, buf, unit);
		case KIND_U32:
			return SerializeCachedNumberMapKey<uint32_t>(entry, buf, unit);
		case KIND_I64:
			return SerializeCachedNumberMapKey<int64_t>(entry, buf, unit);
		case KIND_U64:
			return SerializeCachedNumberMapKey<uint64_t>(entry, buf, unit);
		default:
			PyErr_SetString(PyExc_ValueError, "unsupported ProtoCache map key type");
			return false;
	}
}

static bool CheckArrayObject(PyObject* value) {
	if (PyList_Check(value)) {
		return true;
	}
	PyErr_SetString(PyExc_TypeError, "repeated field must be list");
	return false;
}

static Py_ssize_t ArrayObjectSize(PyObject* value) {
	return PyList_GET_SIZE(value);
}

static PyObject* ArrayObjectItem(PyObject* value, Py_ssize_t index) {
	return PyList_GET_ITEM(value, index);
}

static bool SerializeBoolArrayObject(PyObject* value, protocache::Buffer& buf, protocache::Unit& unit) {
	if (!CheckArrayObject(value)) {
		return false;
	}
	auto n = ArrayObjectSize(value);
	std::vector<uint8_t> data(static_cast<size_t>(n));
	for (Py_ssize_t i = 0; i < n; i++) {
		PyObject* one = ArrayObjectItem(value, i);
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
	if (!CheckArrayObject(value)) {
		return false;
	}
	auto n = ArrayObjectSize(value);
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
		if (!ReadNumber(ArrayObjectItem(value, i), &one)) {
			return false;
		}
		*reinterpret_cast<T*>(buf.Expand(m)) = one;
	}
	buf.Put((static_cast<uint32_t>(n) << 2U) | m);
	unit = protocache::Segment(last, buf.Size());
	return true;
}

static bool SerializeArrayObject(PyObject* value, const CompiledType& type,
								 protocache::Buffer& buf, protocache::Unit& unit) {
	switch (type.value_kind) {
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
	if (!CheckArrayObject(value)) {
		return false;
	}
	auto n = ArrayObjectSize(value);
	std::vector<protocache::Unit> elements(static_cast<size_t>(n));
	auto last = buf.Size();
	for (Py_ssize_t i = n; i-- > 0;) {
		PyObject* one = ArrayObjectItem(value, i);
		if (!SerializeValue(one, type, buf, elements[static_cast<size_t>(i)])) {
			return false;
		}
	}
	return protocache::SerializeArray(elements, buf, last, unit);
}

static bool SerializeMapObject(PyObject* value, const CompiledType& type,
							   protocache::Buffer& buf, protocache::Unit& unit) {
	if (!PyDict_Check(value)) {
		PyErr_SetString(PyExc_TypeError, "map field must be dict");
		return false;
	}
	auto n = PyDict_Size(value);
	if (n < 0) {
		return false;
	}
	std::vector<MapEntry> entries(static_cast<size_t>(n));
	Py_ssize_t pos = 0;
	Py_ssize_t i = 0;
	PyObject* key = nullptr;
	PyObject* item = nullptr;
	while (PyDict_Next(value, &pos, &key, &item)) {
		auto idx = static_cast<size_t>(i++);
		entries[idx].key = key;
		entries[idx].value = item;
		if (!KeyBytes(key, type.key_kind, &entries[idx].key_bytes)) {
			return false;
		}
	}
	PyKeyReader reader(entries);
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
		const auto& entry = entries[static_cast<size_t>(book[static_cast<size_t>(i)])];
		if (!SerializeValue(entry.value, type, buf, units[static_cast<size_t>(i)].second) ||
			!SerializeCachedMapKey(entry, type.key_kind, buf, units[static_cast<size_t>(i)].first)) {
			return false;
		}
	}
	return protocache::SerializeMap(index.Data(), units, buf, last, unit);
}

static bool SerializeContainerObject(PyObject* value, const CompiledType& type,
									 protocache::Buffer& buf, protocache::Unit& unit) {
	if (IsCompiledArrayType(type)) {
		return SerializeArrayObject(value, type, buf, unit);
	}
	if (IsCompiledMapType(type)) {
		return SerializeMapObject(value, type, buf, unit);
	}
	PyErr_SetString(PyExc_TypeError, "ProtoCache array or map type expected");
	return false;
}

static bool SerializeValue(PyObject* value, const CompiledType& type,
						   protocache::Buffer& buf, protocache::Unit& unit) {
	switch (type.value_kind) {
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
			if (type.value_type == nullptr || type.value_type == Py_None || !PyType_Check(type.value_type)) {
				PyErr_SetString(PyExc_TypeError, "message field must have class value_type");
				return false;
			}
			if (Py_TYPE(value) != reinterpret_cast<PyTypeObject*>(type.value_type)) {
				PyErr_SetString(PyExc_TypeError, "message field must match schema type exactly");
				return false;
			}
			PyObject* raw_schema = GetMessageSchemaObject(type.value_type);
			if (raw_schema == nullptr) {
				return false;
			}
			PyObjectPtr schema(raw_schema);
			return SerializeMessageObject(value, reinterpret_cast<PySchema*>(schema.get())->schema, buf, unit);
		}
		case KIND_ARRAY:
		{
			PyObjectPtr nested_owner;
			const auto* nested = NestedContainerType(type, KIND_ARRAY, &nested_owner);
			if (nested == nullptr) {
				return false;
			}
			return SerializeArrayObject(value, *nested, buf, unit);
		}
		case KIND_MAP:
		{
			PyObjectPtr nested_owner;
			const auto* nested = NestedContainerType(type, KIND_MAP, &nested_owner);
			if (nested == nullptr) {
				return false;
			}
			return SerializeMapObject(value, *nested, buf, unit);
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
	if (!PyObject_TypeCheck(schema, &SchemaType)) {
		PyErr_SetString(PyExc_TypeError, "serialize_model schema must be a ProtoCache Schema");
		return nullptr;
	}
	if (!SerializeMessageObject(obj, reinterpret_cast<PySchema*>(schema)->schema, buf, unit)) {
		return nullptr;
	}
	return UnitToBytes(buf, unit);
}

static PyObject* Module_compile_schema(PyObject*, PyObject* arg) {
	return CompileSchemaObject(arg);
}

static PyObject* Module_compile_type(PyObject*, PyObject* arg) {
	return CompileContainerSchemaSpecObject(arg);
}

static PyObject* Module_deserialize_model(PyObject*, PyObject* args) {
	PyObject* cls;
	PyObject* src;
	PyObject* schema_obj;
	if (!PyArg_ParseTuple(args, "OOO", &cls, &src, &schema_obj)) {
		return nullptr;
	}
	auto storage = ReadBufferWords(src);
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
	if (!PyObject_TypeCheck(schema_obj, &SchemaType)) {
		PyErr_SetString(PyExc_TypeError, "deserialize_model schema must be a ProtoCache Schema");
		return nullptr;
	}
	return MaterializeMessage(cls, storage, ptr, end, reinterpret_cast<PySchema*>(schema_obj)->schema);
}

static PyObject* Module_serialize_container(PyObject*, PyObject* args) {
	PyObject* obj;
	PyObject* type_obj;
	if (!PyArg_ParseTuple(args, "OO", &obj, &type_obj)) {
		return nullptr;
	}
	if (!PyObject_TypeCheck(type_obj, &CompiledTypeType)) {
		PyErr_SetString(PyExc_TypeError, "serialize_container schema must be a compiled ProtoCache type");
		return nullptr;
	}
	const auto& type = reinterpret_cast<PyCompiledTypeObject*>(type_obj)->type;
	protocache::Buffer buf;
	protocache::Unit unit;
	if (!SerializeContainerObject(obj, type, buf, unit)) {
		return nullptr;
	}
	return UnitToBytes(buf, unit);
}

static PyObject* Module_deserialize_container(PyObject*, PyObject* args) {
	PyObject* cls;
	PyObject* src;
	PyObject* type_obj;
	if (!PyArg_ParseTuple(args, "OOO", &cls, &src, &type_obj)) {
		return nullptr;
	}
	if (!PyObject_TypeCheck(type_obj, &CompiledTypeType)) {
		PyErr_SetString(PyExc_TypeError, "deserialize_container schema must be a compiled ProtoCache type");
		return nullptr;
	}
	auto storage = ReadBufferWords(src);
	if (!storage) {
		return nullptr;
	}
	auto ptr = storage->data();
	auto end = ptr + storage->size();
	const auto& type = reinterpret_cast<PyCompiledTypeObject*>(type_obj)->type;
	return MaterializeContainer(ptr, type, cls, storage, end);
}

static PyObject* Module_compress(PyObject*, PyObject* arg) {
	ScopedPyBuffer view;
	if (!view.Get(arg, PyBUF_CONTIG_RO)) {
		return nullptr;
	}
	auto* raw = view.get();
	std::string out;
	protocache::Compress(reinterpret_cast<const uint8_t*>(raw->buf),
						  static_cast<size_t>(raw->len), &out);
	return PyBytes_FromStringAndSize(out.data(), static_cast<Py_ssize_t>(out.size()));
}

static PyObject* Module_decompress(PyObject*, PyObject* arg) {
	ScopedPyBuffer view;
	if (!view.Get(arg, PyBUF_CONTIG_RO)) {
		return nullptr;
	}
	auto* raw = view.get();
	std::string out;
	if (!protocache::Decompress(reinterpret_cast<const uint8_t*>(raw->buf),
								static_cast<size_t>(raw->len), &out)) {
		PyErr_SetString(PyExc_ValueError, "invalid ProtoCache compressed data");
		return nullptr;
	}
	return PyBytes_FromStringAndSize(out.data(), static_cast<Py_ssize_t>(out.size()));
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
		{"to_list", reinterpret_cast<PyCFunction>(ArrayView_to_list), METH_O,
		 "Materialize a scalar array as a Python list."},
		{nullptr, nullptr, 0, nullptr},
};

static PyMethodDef MapViewMethods[] = {
		{"from_bytes", reinterpret_cast<PyCFunction>(MapView_from_bytes), METH_CLASS | METH_O,
		 "Create a map view from a bytes-like object."},
		{"size", reinterpret_cast<PyCFunction>(MapView_size), METH_NOARGS,
		 "Return map size."},
		{"items", reinterpret_cast<PyCFunction>(MapView_items), METH_VARARGS,
		 "Return map items as typed Python values."},
		{"to_dict", reinterpret_cast<PyCFunction>(MapView_to_dict), METH_VARARGS,
		 "Materialize map items as a Python dict."},
		{"get", reinterpret_cast<PyCFunction>(MapView_get), METH_VARARGS,
		 "Find a map value by key."},
		{nullptr, nullptr, 0, nullptr},
};

static PyMethodDef ModuleMethods[] = {
		{"compile_schema", Module_compile_schema, METH_O,
		 "Compile a generated ProtoCache schema tuple."},
		{"compile_type", Module_compile_type, METH_O,
		 "Compile a generated ProtoCache container schema tuple."},
		{"deserialize_container", Module_deserialize_container, METH_VARARGS,
		 "Deserialize a generated container object with a compiled schema."},
		{"deserialize_model", Module_deserialize_model, METH_VARARGS,
		 "Deserialize a generated message object with a compiled schema."},
		{"serialize_container", Module_serialize_container, METH_VARARGS,
		 "Serialize a materialized generated container object."},
		{"serialize_model", Module_serialize_model, METH_VARARGS,
		 "Serialize a materialized generated message object."},
		{"compress", Module_compress, METH_O,
		 "Compress a bytes-like object with ProtoCache compression."},
		{"decompress", Module_decompress, METH_O,
		 "Decompress ProtoCache compressed bytes."},
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

	SchemaType.tp_name = "protocache._protocache.CompiledSchema";
	SchemaType.tp_basicsize = sizeof(PySchema);
	SchemaType.tp_dealloc = reinterpret_cast<destructor>(Schema_dealloc);
	SchemaType.tp_flags = Py_TPFLAGS_DEFAULT;
	SchemaType.tp_doc = "Compiled ProtoCache schema";

	CompiledTypeType.tp_name = "protocache._protocache.CompiledType";
	CompiledTypeType.tp_basicsize = sizeof(PyCompiledTypeObject);
	CompiledTypeType.tp_dealloc = reinterpret_cast<destructor>(CompiledType_dealloc);
	CompiledTypeType.tp_flags = Py_TPFLAGS_DEFAULT;
	CompiledTypeType.tp_doc = "Compiled ProtoCache type";

	if (PyType_Ready(&MessageViewType) < 0 ||
		PyType_Ready(&ArrayViewType) < 0 ||
		PyType_Ready(&MapViewType) < 0 ||
		PyType_Ready(&SchemaType) < 0 ||
		PyType_Ready(&CompiledTypeType) < 0) {
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
