# Data Format

All data is little-endian and aligned to 4-byte boundaries. Byte `0x00` is used
as padding to preserve alignment.

## Scalar Type
A scalar is always stored as a word (4 bytes) or a double word (8 bytes).

## Object Type

```
|30bit|
xx...xx11 -> object
|  X words  |
```
If an object is too large to embed, an offset word is stored instead. The two
lowest bits of a valid offset are always `11`, while an embeddable object never
starts with such a word. This pattern distinguishes offsets from embedded
objects.

## Array
```
|header|
xx...xyy element element element ... [object...]
```
The two lowest bits of an array header encode the element size in words; the
remaining bits encode the number of elements.

- A **byte array** header is stored as a varint and has element size 0.
- A **general array** header is stored as a word. Element size 3 denotes
  embedded objects occupying three words, so that representation is non-empty.

## Map
```
| header |
yyzzxx...x [index] key value key value key value ... [object...]
```
A map header is stored as a word. Bits 31–30 encode the key size in words,
bits 29–28 encode the value size in words, and bits 27–0 encode the number of
key-value pairs. Key and value sizes must be nonzero. A map with more than one
element stores an index before its key-value pairs.
```
seed bitmap [offset table]
```
The index is based on the [BDZ algorithm](https://cmph.sourceforge.net/bdz.html)
and consists of a 4-byte seed, an 8-byte-aligned bitmap, and an offset table.

## Message
```
|24bit||8bit| |14bit||50bit|
_______xx...x yy...yy_______ ... field field field ... [object...]
|   header  | |   section  |
```
Message fields are organized into sections. The first 12 fields form the
default section; each subsequent group of 25 fields forms an extended section.
The message header is one word whose lowest byte stores the number of extended
sections. Each extended section is a double word; its highest 14 bits record
the word offset of the section's first field. Every field uses 2 bits to encode
its size in words, with 0 meaning omitted. A message can contain at most 6387
fields.


## Schema Mapping
```
bool,enum,fixed32,int32,uint32,float -> word
fixed64,int64,uint64,double          -> double word
bytes,string,repeated bool           -> byte array
repeated T                           -> general array
map                                  -> map
message                              -> message
```
