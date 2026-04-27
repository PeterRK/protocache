from . import _protocache

NONE = _protocache.NONE
MESSAGE = _protocache.MESSAGE
BYTES = _protocache.BYTES
STRING = _protocache.STRING
F64 = _protocache.F64
F32 = _protocache.F32
U64 = _protocache.U64
U32 = _protocache.U32
I64 = _protocache.I64
I32 = _protocache.I32
BOOL = _protocache.BOOL
ENUM = _protocache.ENUM
ARRAY = _protocache.ARRAY
MAP = _protocache.MAP

_MessageView = _protocache.MessageView
_ArrayView = _protocache.ArrayView
_MapView = _protocache.MapView

_S_NAME = 0
_S_ID = 1
_S_REPEATED = 2
_S_KEY_KIND = 3
_S_VALUE_KIND = 4
_S_VALUE_TYPE = 5

_SCALAR_KINDS = frozenset({
    BYTES,
    STRING,
    F64,
    F32,
    U64,
    U32,
    I64,
    I32,
    BOOL,
    ENUM,
})

_MAP_KEY_KINDS = frozenset({
    STRING,
    U64,
    U32,
    I64,
    I32,
    ENUM,
})

_DEFAULTS = {
    BOOL: False,
    I32: 0,
    U32: 0,
    I64: 0,
    U64: 0,
    ENUM: 0,
    F32: 0.0,
    F64: 0.0,
    STRING: "",
    BYTES: b"",
}


def _check_value_type(value_kind, value_type):
    if value_kind in _SCALAR_KINDS:
        if value_type is not None:
            raise TypeError("scalar ProtoCache TYPE must not have value_type")
    elif value_kind not in (MESSAGE, ARRAY, MAP):
        raise TypeError("invalid ProtoCache value kind")
    elif not isinstance(value_type, type):
        raise TypeError("complex ProtoCache TYPE must have class value_type")


def _alias_type(cls):
    if not isinstance(cls, type):
        raise TypeError("invalid ProtoCache alias type")
    type_spec = cls.TYPE
    if not isinstance(type_spec, tuple) or len(type_spec) != 3:
        raise TypeError("invalid ProtoCache alias TYPE")
    key_kind, value_kind, value_type = type_spec
    _check_value_type(value_kind, value_type)
    return key_kind, value_kind, value_type


def _array_type(cls):
    key_kind, item_kind, item_type = _alias_type(cls)
    if key_kind != NONE:
        raise TypeError("ProtoCache array TYPE expected")
    return item_kind, item_type


def _map_type(cls):
    key_kind, value_kind, value_type = _alias_type(cls)
    if key_kind not in _MAP_KEY_KINDS:
        raise TypeError("ProtoCache map TYPE expected")
    return key_kind, value_kind, value_type


def _default_internal(repeated, key_kind, value_kind, value_type):
    if key_kind != NONE:
        return Map()
    if repeated:
        return Array()
    if value_kind in _DEFAULTS:
        return _DEFAULTS[value_kind]
    if value_kind == MESSAGE:
        return None
    if value_kind in (ARRAY, MAP):
        return value_type()
    return None


def _wrap_internal(value, repeated, key_kind, value_kind, value_type):
    if value is None:
        return None
    if key_kind != NONE:
        return _map_from_view(value, key_kind, value_kind, value_type)
    if repeated:
        return _array_from_view(value, value_kind, value_type)
    if value_kind == MESSAGE:
        return _message_from_view(value_type, value)
    if value_kind == ARRAY:
        item_kind, item_type = _array_type(value_type)
        return _array_from_view(value, item_kind, item_type, value_type)
    if value_kind == MAP:
        map_key_kind, map_value_kind, map_value_type = _map_type(value_type)
        return _map_from_view(
            value, map_key_kind, map_value_kind, map_value_type, value_type
        )
    return value


def _field_from_view(view, field):
    field_id = field[_S_ID]
    repeated = field[_S_REPEATED]
    key_kind = field[_S_KEY_KIND]
    value_kind = field[_S_VALUE_KIND]
    value_type = field[_S_VALUE_TYPE]
    if key_kind != NONE:
        return _map_from_view(view.get_map(field_id), key_kind, value_kind, value_type)
    if repeated:
        return _array_from_view(view.get_array(field_id), value_kind, value_type)
    return _wrap_internal(
        view.get(field_id, value_kind),
        repeated,
        key_kind,
        value_kind,
        value_type,
    )


def _message_from_view(cls, view):
    obj = cls()
    for field in cls._schema:
        name = field[_S_NAME]
        field_id = field[_S_ID]
        if view.has_field(field_id):
            obj._present.add(field_id)
            setattr(obj, name, _field_from_view(view, field))
    return obj


def _array_from_view(view, item_kind, item_type, cls=None):
    out = cls() if cls is not None else Array()
    for i in range(view.size(item_kind)):
        out.append(_wrap_internal(view.get(i, item_kind), False, NONE, item_kind, item_type))
    return out


def _map_from_view(view, key_kind, value_kind, value_type, cls=None):
    out = cls() if cls is not None else Map()
    for key, value in view.items(key_kind, value_kind):
        out[key] = _wrap_internal(value, False, NONE, value_kind, value_type)
    return out


class Message:
    # Immutable reflection-style schema supplied by generated code or a
    # hand-written binding.
    _schema = ()

    def __init__(self, **kwargs):
        self._present = set()
        for field in type(self)._schema:
            name = field[_S_NAME]
            field_id = field[_S_ID]
            repeated = field[_S_REPEATED]
            key_kind = field[_S_KEY_KIND]
            value_kind = field[_S_VALUE_KIND]
            value_type = field[_S_VALUE_TYPE]
            if name in kwargs:
                value = kwargs.pop(name)
                if not repeated and key_kind == NONE and value_kind == ARRAY:
                    if not isinstance(value, value_type):
                        value = value_type(value)
                elif not repeated and key_kind == NONE and value_kind == MAP:
                    if not isinstance(value, value_type):
                        value = value_type(value)
                self._present.add(field_id)
            else:
                value = _default_internal(repeated, key_kind, value_kind, value_type)
            setattr(self, name, value)
        if kwargs:
            names = ", ".join(sorted(kwargs))
            raise TypeError(f"unknown field(s): {names}")

    @classmethod
    def Deserialize(cls, data):
        return _message_from_view(cls, _MessageView.from_bytes(data))

    def HasField(self, field_id):
        return field_id in self._present

    def Serialize(self):
        return _protocache.serialize_model(self, type(self)._schema)


class Array(list):
    TYPE = None

    @classmethod
    def Deserialize(cls, data):
        item_kind, item_type = _array_type(cls)
        return _array_from_view(_ArrayView.from_bytes(data), item_kind, item_type, cls)

    def Serialize(self):
        item_kind, item_type = _array_type(type(self))
        return _protocache.serialize_alias_array(self, item_kind, item_type)


class Map(dict):
    TYPE = None

    @classmethod
    def Deserialize(cls, data):
        key_kind, value_kind, value_type = _map_type(cls)
        return _map_from_view(
            _MapView.from_bytes(data), key_kind, value_kind, value_type, cls
        )

    def Serialize(self):
        key_kind, value_kind, value_type = _map_type(type(self))
        return _protocache.serialize_alias_map(
            self, key_kind, value_kind, value_type
        )
