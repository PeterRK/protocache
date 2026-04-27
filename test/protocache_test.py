import math
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "python"))
sys.path.insert(0, str(Path(__file__).resolve().parent))

try:
    import protocache as pc
    import test_pc
except ImportError as exc:  # pragma: no cover - depends on local extension build
    raise unittest.SkipTest(
        "build the Python extension first: cd python && python3 setup.py build_ext --inplace"
    ) from exc


def _load_root():
    return test_pc.Main.Deserialize((Path(__file__).resolve().parent / "test.pc").read_bytes())


class ProtoCachePythonTest(unittest.TestCase):
    def test_basic_scalars_and_nested_message(self):
        root = _load_root()

        self.assertEqual(root.i32, -999)
        self.assertEqual(root.u32, 1234)
        self.assertEqual(root.i64, -9876543210)
        self.assertEqual(root.u64, 98765432123456789)
        self.assertTrue(root.flag)
        self.assertEqual(root.mode, test_pc.Mode.MODE_C)
        self.assertEqual(root.str, "Hello World!")
        self.assertEqual(root.data, b"abc123!?$*&()'-=@~")
        self.assertTrue(math.isclose(root.f32, -2.1, rel_tol=0.0, abs_tol=1e-6))
        self.assertEqual(root.f64, 1.0)

        leaf = root.object
        self.assertEqual(leaf.i32, 88)
        self.assertFalse(leaf.flag)
        self.assertEqual(leaf.str, "tmp")

        self.assertTrue(root.HasField(test_pc.Main._field_objectv))
        self.assertFalse(root.HasField(test_pc.Main._field_t_i32))

    def test_repeated_fields(self):
        root = _load_root()

        self.assertIsInstance(root.i32v, list)
        self.assertEqual(list(root.i32v), [1, 2])
        self.assertEqual(list(root.u64v), [12345678987654321])
        self.assertEqual(
            list(root.strv),
            [
                "abc",
                "apple",
                "banana",
                "orange",
                "pear",
                "grape",
                "strawberry",
                "cherry",
                "mango",
                "watermelon",
            ],
        )
        self.assertEqual(len(root.datav), 0)
        self.assertTrue(math.isclose(root.f32v[0], 1.1, rel_tol=0.0, abs_tol=1e-6))
        self.assertTrue(math.isclose(root.f32v[1], 2.2, rel_tol=0.0, abs_tol=1e-6))
        self.assertEqual(list(root.f64v), [9.9, 8.8, 7.7, 6.6, 5.5])
        self.assertEqual(list(root.flags), [True, True, False, True, False, False, False])

        objects = root.objectv
        self.assertEqual(len(objects), 3)
        self.assertEqual(objects[0].i32, 1)
        self.assertTrue(objects[1].flag)
        self.assertEqual(objects[2].str, "good luck!")

    def test_maps_and_aliases(self):
        root = _load_root()

        index = root.index
        self.assertIsInstance(index, pc.Map)
        self.assertIsInstance(index, dict)
        self.assertEqual(len(index), 6)
        self.assertEqual(index["abc-1"], 1)
        self.assertEqual(index["abc-2"], 2)
        self.assertIsNone(index.get("abc-3"))
        with self.assertRaises(KeyError):
            _ = index["abc-4"]

        objects = root.objects
        self.assertEqual(len(objects), 4)
        self.assertEqual(objects[1].i32, 1)
        self.assertIsNone(objects.get(5))
        for key, value in objects.items():
            self.assertNotEqual(key, 0)
            self.assertEqual(value.i32, key)

        self.assertEqual([list(row) for row in root.matrix], [[1.0, 2.0, 3.0],
                                                              [4.0, 5.0, 6.0],
                                                              [7.0, 8.0, 9.0]])
        self.assertEqual(root.matrix[2][2], 9.0)
        self.assertIsInstance(root.matrix, pc.Array)

        vector = root.vector
        self.assertIsInstance(vector, pc.Array)
        self.assertEqual(len(vector), 2)
        self.assertEqual(len(vector[0]), 2)
        self.assertEqual(list(vector[0]["lv2"]), [21.0, 22.0])

        arrays = root.arrays
        self.assertIsInstance(arrays, pc.Map)
        self.assertEqual(list(arrays["lv5"]), [51.0, 52.0])

    def test_full_serialize_round_trip(self):
        root = _load_root()
        data = root.Serialize()
        self.assertEqual(len(data), len((Path(__file__).resolve().parent / "test.pc").read_bytes()))
        mirror = test_pc.Main.Deserialize(data)

        self.assertEqual(mirror.i32, -999)
        self.assertEqual(mirror.str, "Hello World!")
        self.assertEqual(mirror.object.i32, 88)
        self.assertEqual(list(mirror.i32v), [1, 2])
        self.assertEqual(list(mirror.flags), [True, True, False, True, False, False, False])
        self.assertEqual(mirror.index["abc-2"], 2)
        self.assertEqual(list(mirror.arrays["lv5"]), [51.0, 52.0])

        built = test_pc.Main(
            i32=-1,
            str="x",
            object=test_pc.Small(i32=7),
            i32v=[3, 4],
            index={"a": 9},
            matrix=test_pc.Vec2D([test_pc.Vec2D_Vec1D([1.0])]),
        )
        self.assertIsInstance(built.i32v, list)
        self.assertIsInstance(built.index, dict)
        loaded = test_pc.Main.Deserialize(built.Serialize())
        self.assertEqual(loaded.i32, -1)
        self.assertEqual(loaded.str, "x")
        self.assertEqual(loaded.object.i32, 7)
        self.assertEqual(list(loaded.i32v), [3, 4])
        self.assertEqual(loaded.index, {"a": 9})
        self.assertEqual([list(row) for row in loaded.matrix], [[1.0]])

    def test_deserialize_accepts_memoryview_slice(self):
        data = test_pc.Main(i32=42, str="slice").Serialize()
        view = memoryview(b"x" + data)[1:]
        loaded = test_pc.Main.Deserialize(view)

        self.assertEqual(loaded.i32, 42)
        self.assertEqual(loaded.str, "slice")

    def test_deserialize_materializes_before_return(self):
        source = bytearray(test_pc.Main(i32=7, str="stable", data=b"payload").Serialize())
        loaded = test_pc.Main.Deserialize(memoryview(source))
        source[:] = b"\0" * len(source)

        self.assertEqual(loaded.i32, 7)
        self.assertEqual(loaded.str, "stable")
        self.assertEqual(loaded.data, b"payload")

    def test_schema_named_field(self):
        class Weird(pc.Message):
            _field_schema = 0

        Weird._schema = (("schema", Weird._field_schema, False, pc.NONE, pc.I32, None),)

        obj = Weird(schema=7)
        self.assertTrue(obj.HasField(Weird._field_schema))
        self.assertEqual(Weird.Deserialize(obj.Serialize()).schema, 7)

    def test_schema_contract(self):
        self.assertEqual(pc.NONE, 0)
        self.assertIsInstance(test_pc.Main._schema, tuple)

        schema = test_pc.Main._schema
        self.assertIsInstance(schema, tuple)
        self.assertTrue(all(len(field) == 6 for field in schema))

        fields = {field[0]: field for field in schema}
        self.assertEqual(fields["i32"], ("i32", test_pc.Main._field_i32, False, pc.NONE, pc.I32, None))
        self.assertEqual(fields["object"][2:], (False, pc.NONE, pc.MESSAGE, test_pc.Small))

        i32v = fields["i32v"]
        self.assertEqual(i32v[2:], (True, pc.NONE, pc.I32, None))

        matrix = fields["matrix"]
        self.assertEqual(matrix[2:], (False, pc.NONE, pc.ARRAY, test_pc.Vec2D))

        index = fields["index"]
        self.assertEqual(index[2:], (True, pc.STRING, pc.I32, None))

        arrays = fields["arrays"]
        self.assertEqual(arrays[2:], (False, pc.NONE, pc.MAP, test_pc.ArrMap))

    def test_schema_rejects_wrong_tuple_size(self):
        obj = test_pc.Main(i32=1)
        with self.assertRaises(ValueError):
            pc._protocache.serialize_model(obj, (("i32", test_pc.Main._field_i32, pc.I32),))

    def test_alias_type_contract(self):
        self.assertEqual(test_pc.Vec2D_Vec1D.TYPE, (pc.NONE, pc.F32, None))
        self.assertEqual(test_pc.Vec2D.TYPE, (pc.NONE, pc.ARRAY, test_pc.Vec2D_Vec1D))
        self.assertEqual(test_pc.ArrMap.TYPE, (pc.STRING, pc.ARRAY, test_pc.ArrMap_Array))

        class BrokenArray(pc.Array):
            TYPE = (pc.STRING, pc.I32, None)

        class BrokenMap(pc.Map):
            TYPE = (pc.NONE, pc.I32, None)

        class BrokenComplex(pc.Array):
            TYPE = (pc.NONE, pc.ARRAY, object())

        with self.assertRaises(TypeError):
            BrokenArray([1]).Serialize()
        with self.assertRaises(TypeError):
            BrokenMap({"a": 1}).Serialize()
        with self.assertRaises(TypeError):
            BrokenComplex([]).Serialize()

        obj = test_pc.Main(matrix=test_pc.Vec2D())
        with self.assertRaises(TypeError):
            pc._protocache.serialize_model(
                obj,
                (("matrix", test_pc.Main._field_matrix, False, pc.NONE, pc.ARRAY, object()),),
            )

    def test_only_alias_array_map_serialize_standalone(self):
        row = test_pc.Vec2D_Vec1D([1.0, 2.0])
        self.assertEqual(list(test_pc.Vec2D_Vec1D.Deserialize(row.Serialize())), [1.0, 2.0])

        arrays = test_pc.ArrMap({"a": test_pc.ArrMap_Array([7.0])})
        loaded = test_pc.ArrMap.Deserialize(arrays.Serialize())
        self.assertEqual(list(loaded["a"]), [7.0])

        with self.assertRaises(TypeError):
            pc.Array([1]).Serialize()

        with self.assertRaises(TypeError):
            pc.Map({"a": 1}).Serialize()

    def test_serialize_type_conversion_boundaries(self):
        loaded = test_pc.Main.Deserialize(test_pc.Main(f32=1, f64=2).Serialize())
        self.assertEqual(loaded.f32, 1.0)
        self.assertEqual(loaded.f64, 2.0)

        with self.assertRaises(TypeError):
            test_pc.Main(flag="yes").Serialize()


if __name__ == "__main__":
    unittest.main()
