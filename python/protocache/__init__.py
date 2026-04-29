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

compress = _protocache.compress
decompress = _protocache.decompress

def _compile_internal_schema(cls):
    schema = cls.__dict__.get("_internal_schema")
    if schema is None:
        schema = _protocache.compile_type(cls.schema)
        cls._internal_schema = schema
    return schema


class Message:
    # Immutable reflection-style schema supplied by generated code or a
    # hand-written binding.
    _schema = ()
    _internal_schema = None

    def __init__(self, **kwargs):
        self.__dict__.update(kwargs)

    @classmethod
    def Deserialize(cls, data):
        return _protocache.deserialize_model(cls, data, cls._get_internal_schema())

    @classmethod
    def _get_internal_schema(cls):
        schema = cls.__dict__.get("_internal_schema")
        if schema is None:
            schema = _protocache.compile_schema(cls._schema)
            cls._internal_schema = schema
        return schema

    def HasField(self, field_id):
        present = self.__dict__.get("_present")
        return present is not None and field_id in present

    def Serialize(self):
        return _protocache.serialize_model(self, type(self)._get_internal_schema())


class _ContainerMixin:
    schema = None
    _internal_schema = None

    @classmethod
    def _get_internal_schema(cls):
        return _compile_internal_schema(cls)

    @classmethod
    def Deserialize(cls, data):
        return _protocache.deserialize_container(cls, data, cls._get_internal_schema())

    def Serialize(self):
        return _protocache.serialize_container(self, type(self)._get_internal_schema())


class Array(_ContainerMixin, list):
    pass


class Map(_ContainerMixin, dict):
    pass
