# ProtoCache Python

The Python binding is a format-compatible convenience layer for ProtoCache.
Generated modules describe a schema, while the shared native runtime fully
serializes and deserializes normal Python object graphs. It supports CPython
3.10 through 3.14 on 64-bit Linux, macOS, and Windows.

The API favors a concise Python object model and format compatibility. Unlike
the C++ binding, it is not a zero-copy view over the input buffer.

## Installation

Install directly from a repository checkout:

```sh
python -m pip install ./python
```

The package contains a C++17 extension. Building from source therefore requires
a supported Python development environment and a C++ compiler. Release wheels
do not require a compiler at installation time.

Generate Python schema classes with the ProtoCache protoc plugin:

```sh
protoc --pcpy_out=. test.proto
```

## Usage

```python
import protocache as pc
import test_pc

# Deserialization fully materializes nested messages, containers, strings, and
# bytes before returning.
root = test_pc.Main.Deserialize(data)
print(root.i32)
for item in root.objectv:
    print(item.str)

# Serialization walks the generated schema and produces ProtoCache bytes.
built = test_pc.Main(i32=1, str="hello", i32v=[1, 2])
encoded = built.Serialize()

packed = pc.compress(encoded)
assert pc.decompress(packed) == encoded
```

Generated message classes inherit from `protocache.Message`. Standalone
repeated containers inherit from `protocache.Array`, and standalone maps inherit
from `protocache.Map`. Ordinary repeated fields and maps accept normal `list`
and `dict` values.

The distribution includes a `py.typed` marker and public type stubs, so mypy and
other PEP 561-aware type checkers can type-check `Message`, `Array`, `Map`, and
the compression helpers.

## Development and packaging

Build the extension in place and run the tests from the repository root:

```sh
cd python
python setup.py build_ext --inplace
cd ..
python -m unittest discover -s test -p '*_test.py'
python python/smoke_test.py
python -m mypy --strict python/typing_check.py
python -m mypy.stubtest protocache
```

Build the source and wheel distributions with the standard PEP 517 frontend:

```sh
cd python
python -m pip install build
python -m build
```

The source distribution carries the C++ core files required to build the
extension without a Git checkout. During a checkout build those files are
staged temporarily from the repository's canonical `src/` and `include/`
directories, then removed automatically; no second maintained copy is kept in
the Python package.

## Design notes

- Generated `*_pc.py` files contain field numbers and lightweight type
  metadata. Message schemas are 6-tuples:
  `(name, field_id, repeated, key_kind, value_kind, value_type)`.
- Standalone repeated and map containers use a 3-tuple `schema`:
  `(key_kind, value_kind, value_type)`. `key_kind` is `protocache.NONE` when no
  map key is present.
- `Deserialize()` recursively materializes values; `Serialize()` reflects over
  the same schema tuples to emit compatible ProtoCache bytes.
- `compress()` and `decompress()` are thin bindings over the C++ helpers and
  accept bytes-like inputs.
- The binding does not bridge Python protobuf message classes. Interchange is
  through ProtoCache bytes.

ProtoCache is distributed under the repository's
[BSD 3-Clause license](../LICENSE).
