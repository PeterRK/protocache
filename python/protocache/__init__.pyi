from typing import Any, ClassVar, Dict, Generic, List, Optional, Tuple, Type, TypeVar, Union

_ReadableBuffer = Union[bytes, bytearray, memoryview]

NONE: int
MESSAGE: int
BYTES: int
STRING: int
F64: int
F32: int
U64: int
U32: int
I64: int
I32: int
BOOL: int
ENUM: int
ARRAY: int
MAP: int

_MessageT = TypeVar("_MessageT", bound="Message")
_ArrayT = TypeVar("_ArrayT")
_ArraySelf = TypeVar("_ArraySelf", bound="Array[Any]")
_MapKeyT = TypeVar("_MapKeyT")
_MapValueT = TypeVar("_MapValueT")
_MapSelf = TypeVar("_MapSelf", bound="Map[Any, Any]")


class Message:
    _schema: ClassVar[Tuple[Any, ...]]
    _internal_schema: ClassVar[Any]

    def __init__(self, **kwargs: Any) -> None: ...

    @classmethod
    def Deserialize(cls: Type[_MessageT], data: _ReadableBuffer) -> _MessageT: ...

    def HasField(self, field_id: int) -> bool: ...
    def Serialize(self) -> bytes: ...


class Array(List[_ArrayT], Generic[_ArrayT]):
    schema: ClassVar[Optional[Tuple[Any, ...]]]
    _internal_schema: ClassVar[Any]

    @classmethod
    def Deserialize(cls: Type[_ArraySelf], data: _ReadableBuffer) -> _ArraySelf: ...

    def Serialize(self) -> bytes: ...


class Map(Dict[_MapKeyT, _MapValueT], Generic[_MapKeyT, _MapValueT]):
    schema: ClassVar[Optional[Tuple[Any, ...]]]
    _internal_schema: ClassVar[Any]

    @classmethod
    def Deserialize(cls: Type[_MapSelf], data: _ReadableBuffer) -> _MapSelf: ...

    def Serialize(self) -> bytes: ...


def compress(data: _ReadableBuffer, /) -> bytes: ...
def decompress(data: _ReadableBuffer, /) -> bytes: ...
