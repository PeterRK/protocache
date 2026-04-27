# ProtoCache Python

The Python binding is a format-compatible convenience layer for ProtoCache. It
is implemented as a special reflection-style full serialization/deserialization
interface: generated Python modules only describe the schema, while the shared
runtime walks that schema to read or write complete Python object graphs.

This keeps generated code and user code small. It is not the zero-copy,
read-only access API used by the C++ binding, and it is not meant to be the
fastest way to traverse ProtoCache data. Use it when you want a simple Python
object model that can consume and produce the same ProtoCache binary format.

Build the extension in place:

```sh
cd python
python3 setup.py build_ext --inplace
```

Generate Python schema classes:

```sh
protoc --pcpy_out=. test.proto
```

Use the generated module as plain Python objects:

```python
import test_pc

# Deserialize fully materializes the message. Nested messages, repeated fields,
# maps, strings, and bytes are copied into Python objects before the call
# returns.
root = test_pc.Main.Deserialize(data)
print(root.i32)
print(root.str)
for item in root.objectv:
    print(item.str)

# Serialize walks the generated schema and writes a complete ProtoCache binary.
data2 = root.Serialize()

built = test_pc.Main(i32=1, str="hello", i32v=[1, 2])
data3 = built.Serialize()
```

Generated message classes inherit from `protocache.Message`. Repeated-field
aliases inherit from `protocache.Array`, and map aliases inherit from
`protocache.Map`. Regular repeated fields and maps can be initialized with
normal Python `list` and `dict` values; alias classes are only needed when the
schema defines a standalone repeated/map type that should be serialized on its
own.

Run the Python smoke test from the repository root after building the extension:

```sh
python3 -m unittest discover -s test -p '*_test.py'
```

## Design notes

- The generated `*_pc.py` file contains field numbers and lightweight type
  metadata. Message schemas are direct reflection-style 6-tuples:
  `(name, field_id, repeated, key_kind, value_kind, value_type)`.
  Standalone repeated/map aliases use a compact 3-tuple `TYPE`:
  `(key_kind, value_kind, value_type)`.
  `key_kind` is `_pc.NONE` (0) when there is no map key, and complex
  `value_type` entries must be generated Python classes.
- At runtime, `protocache.Message.Deserialize()` creates a temporary binary
  view and recursively materializes every schema field into Python values.
- `Serialize()` uses the same schema tuples to reflect over Python
  attributes and emit ProtoCache bytes.
- The binding does not bridge Python protobuf message classes. Convert through
  ProtoCache bytes, not through `google.protobuf` objects.
- Because values are fully materialized and serialized generically, this API
  favors concise code and format compatibility over zero-copy traversal speed.
