# Data Format

All data are little endian and 4 byte aligned. Byte `0x00` is used as padding to keep alignment.

## Scalar Type
Scalar type is always stored as a word (4byte) or a double word (8byte).

## Object Type

```
|30bit|
xx...xx11 -> object
|  X words  |
```
If an object is too big to store embedded, a offset word will be stored before. The lowest 2 bits of a valid offset are always 11, while any embeddable object will not have such word first. We can tell whether an element is an offset or an embedded object by this pattern.

## Array
```
|header|
xx...xyy element element element ... [object...]
```
The lowest 2 bits of array header marks the element size in words, while other bits means the numbers of elements.

- Header of **byte array** is stored as varint, and the element size is 0.
- Header of **gneral array** is stored as a word. When element size is 3, this array must contain embedded objects with size of 3 words, which means it's not empty.

## Map
```
| header |
yyzzxx...x [index] key value key value key value ... [object...]
```
Header of map is stored as a word. The 31-30bit of header means key size in word, the 29-28bit of header means value size in word, and the 27-0bit of header means numbers of key-value pairs. Key size and value size should not be 0. Map with more than one element will have an index stored before key-value pairs.

## Message
```
|24bit||8bit| |14bit||50bit|
_______xx...x yy...yy_______ ... field field field ... [object...]
|   header  | |   section  |
```
Fields of a message will be organized by sections. The first 12 fields are default section, then every 25 fields make up an extended section. Header of message is stored as a word. The lowest byte of header represents number of extended sections. An extended section is stored as a double word, highest 14 bits of which records offset in words of the first field in this section. Every field takes 2 bits to 
mark field size in word. 0 means the field is omitted. A message can have at most 6387 fields.


## Schema Mapping
```
bool,enum,fixed32,int32,uint32,float -> word
fixed64,int64,uint64,double          -> double word
bytes,string,repeated bool           -> byte array
repeated T                           -> general array
map                                  -> map
message                              -> message
```