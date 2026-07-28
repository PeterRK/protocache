# Schema Compatibility with Protobuf

ProtoCache uses Protobuf declarations as a schema language, but it is not a
second implementation of the Protobuf wire format or object model. A
ProtoCache buffer contains field slots and values, not a serialized descriptor;
the reader and writer must therefore use compatible schemas.

This document defines the portable ProtoCache schema subset. Producers and
consumers may provide additional APIs, but data is portable only when its schema
and observable semantics conform to this document.

## Portable schema rules

| Schema feature | Portable ProtoCache rule | Difference from Protobuf |
|---|---|---|
| Syntax | Use `proto3`. | Required fields, explicit defaults, closed enums, groups, and proto2 presence are outside the ProtoCache schema contract. |
| Normal message | Must declare at least one field. | Protobuf permits empty messages. Deprecated fields still reserve their numbers even though they are excluded from the ProtoCache schema view. |
| Field number | Must be in `1..6387`. ProtoCache field ID is `number - 1`. | Protobuf permits field numbers through `2^29 - 1`, excluding its reserved interval. |
| Field density | With `field_count` declared fields, `max_number - field_count` must be at most 6 or `max_number` must be at most `2 * field_count`. | Protobuf allows arbitrarily sparse legal field numbers. |
| Field name `_` | Reserved for the alias convention described below. It is invalid as an ordinary field. | `_` is an ordinary legal Protobuf identifier. |
| Duplicate numbers | Invalid. | Normally rejected by `protoc` as well; ProtoCache also validates descriptors supplied without `protoc`. |
| Declaration order | Not semantic; fields are addressed by number. | This matches Protobuf wire compatibility. Source API layout can still change when declarations move. |

The 6387 limit is a limit on addressable message slots, not merely on the
number of declared fields. See [data-format.md](data-format.md#message) for the
message header layout.

The density rule is a writer-side portability constraint, not a decoder limit.
A reader can address sparse slots in an existing buffer, but serialization is
not guaranteed for a schema outside the density bound.

## Scalar mapping

Several distinct Protobuf wire encodings intentionally collapse to the same
fixed-width ProtoCache kind:

| Protobuf declaration | ProtoCache representation | Notes |
|---|---|---|
| `double` | 64-bit floating point | IEEE-754 value; NaN payload bits are not portable schema semantics. |
| `float` | 32-bit floating point | Singular `-0` is not guaranteed distinct from the default `+0`; repeated values retain their elements. |
| `int64`, `sint64`, `sfixed64` | signed 64-bit word | Zigzag and varint distinctions do not exist in ProtoCache. |
| `uint64`, `fixed64` | unsigned 64-bit word | Language APIs must preserve the full unsigned 64-bit range. |
| `int32`, `sint32`, `sfixed32` | signed 32-bit word | Stored as fixed-width two's-complement bits. |
| `uint32`, `fixed32` | unsigned 32-bit word | Stored as a fixed-width word. |
| `bool` | 32-bit word | A repeated bool uses a compact byte array. |
| `enum` | signed 32-bit word | Numeric values are stored; symbolic names are not present in the buffer. |
| `string` | UTF-8 byte array | A valid ProtoCache string contains valid UTF-8. |
| `bytes` | byte array | Physically similar to `string`, without text validity requirements. |
| message | ProtoCache message/object | The referenced schema is required externally. |

Changing a field between Protobuf types that map to the same row can be
physically compatible in ProtoCache even when the corresponding Protobuf wire
change is not. That follows from the physical representation, but it is not
permission to change a published schema casually: object APIs, range checks,
JSON conversion, and Protobuf bridges can still differ.

Unknown proto3 enum numbers are representable because the buffer stores the
signed integer. Enum aliases with the same number are therefore
indistinguishable on the wire, as they are in Protobuf. Proto2 closed-enum
behavior is not supported.

## Presence and defaults

A ProtoCache message can physically distinguish an omitted field slot from a
present slot containing a zero or empty scalar. Portable object semantics do not
require every such physical distinction to survive an object round trip:

- ProtoCache does not require an object-model round trip to preserve explicit
  presence for a singular field whose value equals its scalar default.
- Scalar defaults, empty strings/bytes, empty arrays, and empty maps have the
  canonical object value of an omitted field.
- A present but field-empty nested message has no distinct portable object value
  and may be omitted during serialization.
- Repeated fields and maps do not expose element/container presence in
  Protobuf either; empty and absent containers have the same public value.
- Proto2 `default = ...` declarations are not part of the portable subset and
  ProtoCache defaults do not implement them.

Consequently, `proto3 optional` is a valid schema construct, but ProtoCache does
not guarantee that an object-model round trip preserves the distinction between
“unset” and “set to the default”. Applications must not use that distinction as
a portable ProtoCache invariant.

## `oneof`

ProtoCache treats `oneof` members as independent fields.
The oneof declaration itself is not represented in ProtoCache schema metadata or
in the buffer. In particular:

- assigning one member does not clear another member;
- a ProtoCache buffer may contain several members of the same Protobuf oneof;
- the ProtoCache object model does not define an active-case discriminator;
- a default-valued selected member can become indistinguishable from no member
  after value-based serialization; and
- Protobuf's parsing rule for multiple wire occurrences (“last one wins”) is
  not a ProtoCache buffer invariant.

A conversion from ProtoCache to a Protobuf object must choose its own conflict
policy when several members are present; ProtoCache does not define which member
becomes active.

## Repeated fields and maps

Repeated scalar values use packed fixed-width arrays in ProtoCache regardless
of Protobuf's `packed` option. The `packed` option therefore has no ProtoCache
schema effect.

Protobuf map declarations are recognized through their synthetic map-entry
messages. Portable ProtoCache map keys are:

- `string`;
- `int32`, `sint32`, and `sfixed32`;
- `uint32` and `fixed32`;
- `int64`, `sint64`, and `sfixed64`; and
- `uint64` and `fixed64`.

Although Protobuf permits `bool` map keys, ProtoCache does not. Protobuf does
not permit floating-point, bytes, enum, or message map keys, and neither does
ProtoCache.

A valid ProtoCache map has unique keys. Duplicate entries in malformed input do
not have a portable “last entry wins” guarantee. Map iteration and serialized
entry order must not be treated as a schema-level ordering guarantee.

## Alias messages

ProtoCache has a non-Protobuf alias convention for multidimensional containers.
A message with exactly one repeated field named `_` at field number 1 is
represented directly as an array or map alias instead of as a normal one-field
message:

```proto
message Matrix {
  message Row {
    repeated float _ = 1;
  }
  repeated Row _ = 1;
}
```

This changes the object shape and the ProtoCache representation: `Row` and
`Matrix` are container types, not messages with a property named `_`. The alias
field must be repeated and numbered 1; a singular `_` field or another field
number is invalid. In a message with any additional field, `_` remains reserved
and is invalid as an ordinary field.

Because the convention is structural, adding another field to an alias message
changes it into a normal message and is not a compatible schema evolution.

### Language-binding extension `_x_`

`_x_` is not part of the portable alias convention. A language binding whose
Protobuf toolchain cannot represent the field name `_` may accept `_x_` as a
compatibility spelling. When accepted, it has the same structural requirements
and alias semantics as `_`: it must be the message's only field, be repeated,
and use field number 1.

Other bindings need not assign alias semantics to `_x_` and may treat it as an
ordinary field name. A schema that relies on `_x_` as an alias is therefore
binding-specific rather than portable ProtoCache schema.

## Deprecated declarations and options

Protobuf's `deprecated = true` is normally advisory. ProtoCache instead excludes
deprecated fields, enum values, messages, and enums from its schema view. This
has several consequences:

- a deprecated field is not part of the ProtoCache schema view;
- its field number remains a compatibility hole and must not be reused;
- a deprecated enum number can still appear as an unknown numeric value; and
- marking a declaration deprecated can be an observable ProtoCache API change,
  not merely a compiler warning.

Most other Protobuf options are not represented in a ProtoCache buffer.
`json_name`, source comments, reserved names/ranges, custom annotations,
language package options, and validation annotations remain source-level or
application concerns. Package and message names select source-level types but
are not embedded in the data. Well-known types such as `Any` and `Timestamp`
have no special format semantics; they are ordinary referenced messages whose
higher-level meaning belongs to the application.

## Unknown fields and schema evolution

ProtoCache does not define a Protobuf-style `UnknownFieldSet` in its object
model. A reader can ignore field IDs absent from its schema, but decoding and
re-encoding through that older object normally drops those fields. Do not rely
on the Protobuf pattern of preserving unknown wire data across an older-schema
round trip.

For compatible evolution:

- never reuse a field number, including a number filtered by `deprecated`;
- add fields with new numbers while staying within the field-number and density
  limits;
- treat renaming as binary-compatible but source/API-breaking;
- keep the same scalar family, cardinality, and referenced container/message
  shape unless every producer and consumer is migrated together;
- do not change an alias message into a normal message or the reverse; and
- do not infer oneof validation from the schema.

Since the buffer carries no type names or descriptors, using the wrong schema
can produce a structurally invalid read or a plausible value with the wrong
interpretation. Schema agreement is part of the application protocol.

## Features outside the portable contract

ProtoCache does not provide schema semantics for services or RPC. Protobuf
extensions and groups are not fields in a ProtoCache schema.
Required fields, proto2 custom defaults, proto2 closed enums, MessageSet, and
edition-specific features beyond the proto3-compatible field model are also
outside the portable contract.

The portable schema supports ordinary nested messages and enums, recursive
message references, imports, repeated fields, maps with the key restrictions
above, and proto3 optional declarations. Language-level names and exact public
APIs remain outside the binary schema.
