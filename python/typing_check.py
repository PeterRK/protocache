import protocache as pc


class Model(pc.Message):
    value: int


model: Model = Model.Deserialize(b"\x00\x00\x00\x00")
encoded: bytes = model.Serialize()
present: bool = model.HasField(0)

values: pc.Array[int] = pc.Array([1, 2, 3])
values_encoded: bytes = values.Serialize()

mapping: pc.Map[str, int] = pc.Map({"one": 1})
mapping_encoded: bytes = mapping.Serialize()

packed: bytes = pc.compress(encoded)
unpacked: bytes = pc.decompress(packed)
