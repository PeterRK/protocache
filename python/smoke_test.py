import protocache as pc


class Item(pc.Message):
    _field_value = 0


Item._schema = (
    ("value", Item._field_value, False, pc.NONE, pc.I32, None),
)


class Values(pc.Array):
    schema = (pc.NONE, pc.I32, None)


item = Item(value=42)
decoded_item = Item.Deserialize(item.Serialize())
assert decoded_item.value == 42
assert decoded_item.HasField(Item._field_value)

values = Values([-7, 0, 42])
assert list(Values.Deserialize(values.Serialize())) == [-7, 0, 42]

payload = item.Serialize()
assert pc.decompress(pc.compress(memoryview(payload))) == payload
print("Installed ProtoCache Python package smoke test: OK")
