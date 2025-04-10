#pragma once
#ifndef PROTOCACHE_INCLUDED_test_proto
#define PROTOCACHE_INCLUDED_test_proto

#include <protocache/access.h>

namespace test {

class Small;
class Main;
class CyclicA;
class CyclicB;
class Deprecated;

enum Mode : int32_t {
	MODE_A = 0,
	MODE_B = 1,
	MODE_C = 2
};

class Small final {
private:
	Small() = default;
public:
	struct _ {
		static constexpr unsigned i32 = 0;
		static constexpr unsigned flag = 1;
		static constexpr unsigned str = 3;
	};

	bool operator!() const noexcept { return !protocache::Message::Cast(this); }
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept { return protocache::Message::Cast(this).HasField(id,end); }

	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		auto view = protocache::Message::Detect(ptr, end);
		if (!view) return {};
		protocache::Message core(ptr);
		protocache::Slice<uint32_t> t;
		t = protocache::DetectField<protocache::Slice<char>>(core, _::str, end);
		if (t.end() > view.end()) return {view.data(), static_cast<size_t>(t.end()-view.data())};
		return view;
	}

	int32_t i32(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<int32_t>(protocache::Message::Cast(this), _::i32, end);
	}
	bool flag(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<bool>(protocache::Message::Cast(this), _::flag, end);
	}
	protocache::Slice<char> str(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<protocache::Slice<char>>(protocache::Message::Cast(this), _::str, end);
	}
};

struct Vec2D final {
	struct Vec1D final {
		using ALIAS = protocache::ArrayT<float>;
	};

	using ALIAS = protocache::ArrayT<::test::Vec2D::Vec1D::ALIAS>;
};

struct ArrMap final {
	struct Array final {
		using ALIAS = protocache::ArrayT<float>;
	};

	using ALIAS = protocache::MapT<protocache::Slice<char>,::test::ArrMap::Array::ALIAS>;
};

class Main final {
private:
	Main() = default;
public:
	struct _ {
		static constexpr unsigned i32 = 0;
		static constexpr unsigned u32 = 1;
		static constexpr unsigned i64 = 2;
		static constexpr unsigned u64 = 3;
		static constexpr unsigned flag = 4;
		static constexpr unsigned mode = 5;
		static constexpr unsigned str = 6;
		static constexpr unsigned data = 7;
		static constexpr unsigned f32 = 8;
		static constexpr unsigned f64 = 9;
		static constexpr unsigned object = 10;
		static constexpr unsigned i32v = 11;
		static constexpr unsigned u64v = 12;
		static constexpr unsigned strv = 13;
		static constexpr unsigned datav = 14;
		static constexpr unsigned f32v = 15;
		static constexpr unsigned f64v = 16;
		static constexpr unsigned flags = 17;
		static constexpr unsigned objectv = 18;
		static constexpr unsigned t_u32 = 19;
		static constexpr unsigned t_i32 = 20;
		static constexpr unsigned t_s32 = 21;
		static constexpr unsigned t_u64 = 22;
		static constexpr unsigned t_i64 = 23;
		static constexpr unsigned t_s64 = 24;
		static constexpr unsigned index = 25;
		static constexpr unsigned objects = 26;
		static constexpr unsigned matrix = 27;
		static constexpr unsigned vector = 28;
		static constexpr unsigned arrays = 29;
	};

	bool operator!() const noexcept { return !protocache::Message::Cast(this); }
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept { return protocache::Message::Cast(this).HasField(id,end); }

	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		auto view = protocache::Message::Detect(ptr, end);
		if (!view) return {};
		protocache::Message core(ptr);
		protocache::Slice<uint32_t> t;
		t = protocache::DetectField<::test::ArrMap::ALIAS>(core, _::arrays, end);
		if (t.end() > view.end()) return {view.data(), static_cast<size_t>(t.end()-view.data())};
		t = protocache::DetectField<protocache::ArrayT<::test::ArrMap::ALIAS>>(core, _::vector, end);
		if (t.end() > view.end()) return {view.data(), static_cast<size_t>(t.end()-view.data())};
		t = protocache::DetectField<::test::Vec2D::ALIAS>(core, _::matrix, end);
		if (t.end() > view.end()) return {view.data(), static_cast<size_t>(t.end()-view.data())};
		t = protocache::DetectField<protocache::MapT<int32_t,const ::test::Small*>>(core, _::objects, end);
		if (t.end() > view.end()) return {view.data(), static_cast<size_t>(t.end()-view.data())};
		t = protocache::DetectField<protocache::MapT<protocache::Slice<char>,int32_t>>(core, _::index, end);
		if (t.end() > view.end()) return {view.data(), static_cast<size_t>(t.end()-view.data())};
		t = protocache::DetectField<protocache::ArrayT<const ::test::Small*>>(core, _::objectv, end);
		if (t.end() > view.end()) return {view.data(), static_cast<size_t>(t.end()-view.data())};
		t = protocache::DetectField<protocache::ArrayT<bool>>(core, _::flags, end);
		if (t.end() > view.end()) return {view.data(), static_cast<size_t>(t.end()-view.data())};
		t = protocache::DetectField<protocache::ArrayT<double>>(core, _::f64v, end);
		if (t.end() > view.end()) return {view.data(), static_cast<size_t>(t.end()-view.data())};
		t = protocache::DetectField<protocache::ArrayT<float>>(core, _::f32v, end);
		if (t.end() > view.end()) return {view.data(), static_cast<size_t>(t.end()-view.data())};
		t = protocache::DetectField<protocache::ArrayT<protocache::Slice<uint8_t>>>(core, _::datav, end);
		if (t.end() > view.end()) return {view.data(), static_cast<size_t>(t.end()-view.data())};
		t = protocache::DetectField<protocache::ArrayT<protocache::Slice<char>>>(core, _::strv, end);
		if (t.end() > view.end()) return {view.data(), static_cast<size_t>(t.end()-view.data())};
		t = protocache::DetectField<protocache::ArrayT<uint64_t>>(core, _::u64v, end);
		if (t.end() > view.end()) return {view.data(), static_cast<size_t>(t.end()-view.data())};
		t = protocache::DetectField<protocache::ArrayT<int32_t>>(core, _::i32v, end);
		if (t.end() > view.end()) return {view.data(), static_cast<size_t>(t.end()-view.data())};
		t = protocache::DetectField<const ::test::Small*>(core, _::object, end);
		if (t.end() > view.end()) return {view.data(), static_cast<size_t>(t.end()-view.data())};
		t = protocache::DetectField<protocache::Slice<uint8_t>>(core, _::data, end);
		if (t.end() > view.end()) return {view.data(), static_cast<size_t>(t.end()-view.data())};
		t = protocache::DetectField<protocache::Slice<char>>(core, _::str, end);
		if (t.end() > view.end()) return {view.data(), static_cast<size_t>(t.end()-view.data())};
		return view;
	}

	int32_t i32(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<int32_t>(protocache::Message::Cast(this), _::i32, end);
	}
	uint32_t u32(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<uint32_t>(protocache::Message::Cast(this), _::u32, end);
	}
	int64_t i64(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<int64_t>(protocache::Message::Cast(this), _::i64, end);
	}
	uint64_t u64(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<uint64_t>(protocache::Message::Cast(this), _::u64, end);
	}
	bool flag(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<bool>(protocache::Message::Cast(this), _::flag, end);
	}
	protocache::EnumValue mode(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<protocache::EnumValue>(protocache::Message::Cast(this), _::mode, end);
	}
	protocache::Slice<char> str(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<protocache::Slice<char>>(protocache::Message::Cast(this), _::str, end);
	}
	protocache::Slice<uint8_t> data(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<protocache::Slice<uint8_t>>(protocache::Message::Cast(this), _::data, end);
	}
	float f32(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<float>(protocache::Message::Cast(this), _::f32, end);
	}
	double f64(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<double>(protocache::Message::Cast(this), _::f64, end);
	}
	const ::test::Small* object(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<const ::test::Small*>(protocache::Message::Cast(this), _::object, end);
	}
	protocache::ArrayT<int32_t> i32v(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<protocache::ArrayT<int32_t>>(protocache::Message::Cast(this), _::i32v, end);
	}
	protocache::ArrayT<uint64_t> u64v(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<protocache::ArrayT<uint64_t>>(protocache::Message::Cast(this), _::u64v, end);
	}
	protocache::ArrayT<protocache::Slice<char>> strv(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<protocache::ArrayT<protocache::Slice<char>>>(protocache::Message::Cast(this), _::strv, end);
	}
	protocache::ArrayT<protocache::Slice<uint8_t>> datav(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<protocache::ArrayT<protocache::Slice<uint8_t>>>(protocache::Message::Cast(this), _::datav, end);
	}
	protocache::ArrayT<float> f32v(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<protocache::ArrayT<float>>(protocache::Message::Cast(this), _::f32v, end);
	}
	protocache::ArrayT<double> f64v(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<protocache::ArrayT<double>>(protocache::Message::Cast(this), _::f64v, end);
	}
	protocache::ArrayT<bool> flags(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<protocache::ArrayT<bool>>(protocache::Message::Cast(this), _::flags, end);
	}
	protocache::ArrayT<const ::test::Small*> objectv(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<protocache::ArrayT<const ::test::Small*>>(protocache::Message::Cast(this), _::objectv, end);
	}
	uint32_t t_u32(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<uint32_t>(protocache::Message::Cast(this), _::t_u32, end);
	}
	int32_t t_i32(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<int32_t>(protocache::Message::Cast(this), _::t_i32, end);
	}
	int32_t t_s32(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<int32_t>(protocache::Message::Cast(this), _::t_s32, end);
	}
	uint64_t t_u64(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<uint64_t>(protocache::Message::Cast(this), _::t_u64, end);
	}
	int64_t t_i64(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<int64_t>(protocache::Message::Cast(this), _::t_i64, end);
	}
	int64_t t_s64(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<int64_t>(protocache::Message::Cast(this), _::t_s64, end);
	}
	protocache::MapT<protocache::Slice<char>,int32_t> index(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<protocache::MapT<protocache::Slice<char>,int32_t>>(protocache::Message::Cast(this), _::index, end);
	}
	protocache::MapT<int32_t,const ::test::Small*> objects(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<protocache::MapT<int32_t,const ::test::Small*>>(protocache::Message::Cast(this), _::objects, end);
	}
	::test::Vec2D::ALIAS matrix(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<::test::Vec2D::ALIAS>(protocache::Message::Cast(this), _::matrix, end);
	}
	protocache::ArrayT<::test::ArrMap::ALIAS> vector(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<protocache::ArrayT<::test::ArrMap::ALIAS>>(protocache::Message::Cast(this), _::vector, end);
	}
	::test::ArrMap::ALIAS arrays(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<::test::ArrMap::ALIAS>(protocache::Message::Cast(this), _::arrays, end);
	}
};

class CyclicA final {
private:
	CyclicA() = default;
public:
	struct _ {
		static constexpr unsigned value = 0;
		static constexpr unsigned cyclic = 1;
	};

	bool operator!() const noexcept { return !protocache::Message::Cast(this); }
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept { return protocache::Message::Cast(this).HasField(id,end); }

	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		auto view = protocache::Message::Detect(ptr, end);
		if (!view) return {};
		protocache::Message core(ptr);
		protocache::Slice<uint32_t> t;
		t = protocache::DetectField<const ::test::CyclicB*>(core, _::cyclic, end);
		if (t.end() > view.end()) return {view.data(), static_cast<size_t>(t.end()-view.data())};
		return view;
	}

	int32_t value(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<int32_t>(protocache::Message::Cast(this), _::value, end);
	}
	const ::test::CyclicB* cyclic(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<const ::test::CyclicB*>(protocache::Message::Cast(this), _::cyclic, end);
	}
};

class CyclicB final {
private:
	CyclicB() = default;
public:
	struct _ {
		static constexpr unsigned value = 0;
		static constexpr unsigned cyclic = 1;
	};

	bool operator!() const noexcept { return !protocache::Message::Cast(this); }
	bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept { return protocache::Message::Cast(this).HasField(id,end); }

	static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
		auto view = protocache::Message::Detect(ptr, end);
		if (!view) return {};
		protocache::Message core(ptr);
		protocache::Slice<uint32_t> t;
		t = protocache::DetectField<const ::test::CyclicA*>(core, _::cyclic, end);
		if (t.end() > view.end()) return {view.data(), static_cast<size_t>(t.end()-view.data())};
		return view;
	}

	int32_t value(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<int32_t>(protocache::Message::Cast(this), _::value, end);
	}
	const ::test::CyclicA* cyclic(const uint32_t* end=nullptr) const noexcept {
		return protocache::GetField<const ::test::CyclicA*>(protocache::Message::Cast(this), _::cyclic, end);
	}
};

struct Deprecated final {
	class Valid;

	class Valid final {
	private:
		Valid() = default;
	public:
		struct _ {
			static constexpr unsigned val = 0;
		};

		bool operator!() const noexcept { return !protocache::Message::Cast(this); }
		bool HasField(unsigned id, const uint32_t* end=nullptr) const noexcept { return protocache::Message::Cast(this).HasField(id,end); }

		static protocache::Slice<uint32_t> Detect(const uint32_t* ptr, const uint32_t* end=nullptr) {
			auto view = protocache::Message::Detect(ptr, end);
			if (!view) return {};
			protocache::Message core(ptr);
			protocache::Slice<uint32_t> t;
			return view;
		}

		int32_t val(const uint32_t* end=nullptr) const noexcept {
			return protocache::GetField<int32_t>(protocache::Message::Cast(this), _::val, end);
		}
	};

};

} // test
#endif // PROTOCACHE_INCLUDED_test_proto
