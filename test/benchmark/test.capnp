@0xdeadbeefdeadbeef;

# using Cxx = import "/capnp/c++.capnp";
# $Cxx.namespace("test::capn");

enum Mode {
	modeA @0;
	modeB @1;
	modeC @2;
}

struct Small {
	i32 @0 :Int32;
	flag @1 :Bool;
	str @2 :Text;
}

struct Vec2D {
	struct Vec1D {
		x @0 :List(Float32);
	}
	x @0 :List(Vec1D);
}

struct ArrMap {
	struct Array {
		x @0 :List(Float32);
	}
	struct Entry {
		key @0 :Text;
		value @1 :Array;
	}
	x @0 :List(Entry);
}

struct Map1Entry {
	key @0 :Text;
	value @1 :Int32;
}

struct Map2Entry {
	key @0 :Int32;
	value @1 :Small;
}

struct Main {
	i32 @0 :Int32;
	u32 @1 :UInt32;
	i64 @2 :Int64;
	u64 @3 :UInt64;
	flag @4 :Bool;
	mode @5 :Mode;
	str @6 :Text;
	data @7 :Data;
	f32 @8 :Float32;
	f64 @9 :Float64;
	object @10 :Small;
	i32v @11 :List(Int32);
	u64v @12 :List(UInt64);
	strv @13 :List(Text);
	datav @14 :List(Data);
	f32v @15 :List(Float32);
	f64v @16 :List(Float64);
	flags @17 :List(Bool);
	objectv @18 :List(Small);
	tU32 @19 :UInt32;
	tI32 @20 :Int32;
	tS32 @21 :Int32;
	tU64 @22 :UInt64;
	tI64 @23 :Int64;
	tS64 @24 :Int64;
	index @25 :List(Map1Entry);
	objects @26 :List(Map2Entry);
	matrix @27 :Vec2D;
	vector @28 :List(ArrMap);
	arrays @29 :ArrMap;
}
