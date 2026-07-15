#!/usr/bin/env python3
import argparse
import gc
import importlib
import statistics
import subprocess
import sys
import time
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
GEN = Path("/tmp/protocache-py-benchmark-gen")
sys.path.insert(0, str(ROOT / "python"))
sys.path.insert(0, str(ROOT / "test"))

try:
    import protocache as pc
    import test_pc
except ImportError as exc:
    raise SystemExit(
        "build the Python extension first: cd python && python3 setup.py build_ext --inplace"
    ) from exc


def _generate_python_bindings():
    pb_dir = GEN / "protobuf"
    fb_dir = GEN / "flatbuffers"
    pb_file = pb_dir / "test_pb2.py"
    fb_file = fb_dir / "test" / "Main.py"
    pb_dir.mkdir(parents=True, exist_ok=True)
    fb_dir.mkdir(parents=True, exist_ok=True)
    if not pb_file.exists():
        subprocess.run(
            [
                "protoc",
                f"--python_out={pb_dir}",
                "-I",
                str(ROOT / "test" / "benchmark"),
                str(ROOT / "test" / "benchmark" / "test.proto"),
            ],
            check=True,
        )
    if not fb_file.exists():
        subprocess.run(
            [
                "flatc",
                "--python",
                "-o",
                str(fb_dir),
                str(ROOT / "test" / "benchmark" / "test.fbs"),
            ],
            check=True,
        )
    sys.path.insert(0, str(pb_dir))
    sys.path.insert(0, str(fb_dir))
    return importlib.import_module("test_pb2"), importlib.import_module("test.Main")


def _small(i):
    return test_pc.Small(i32=i, flag=(i % 2) == 0, str=f"small-{i}")


def make_main(scale):
    return test_pc.Main(
        i32=-999,
        u32=1234,
        i64=-9876543210,
        u64=98765432123456789,
        flag=True,
        mode=test_pc.Mode.MODE_C,
        str="Hello World!",
        data=b"abc123!?$*&()'-=@~",
        f32=-2.1,
        f64=1.0,
        object=_small(88),
        i32v=list(range(scale)),
        u64v=[12345678987654321 + i for i in range(scale)],
        strv=[f"str-{i}" for i in range(scale)],
        datav=[f"data-{i}".encode() for i in range(scale)],
        f32v=[i + 0.25 for i in range(scale)],
        f64v=[i + 0.5 for i in range(scale)],
        flags=[(i % 3) == 0 for i in range(scale)],
        objectv=[_small(i) for i in range(scale)],
        t_u32=20,
        t_i32=-21,
        t_s32=-22,
        t_u64=23,
        t_i64=-24,
        t_s64=-25,
        index={f"k-{i}": i for i in range(scale)},
        objects={i + 1: _small(i + 1) for i in range(scale)},
        matrix=[
            [float(row * scale + col) for col in range(scale)]
            for row in range(max(1, scale // 2))
        ],
        vector=[
            {
                f"lv-{row}-{col}": [float(row + col), float(row + col + 1)]
                for col in range(max(1, scale // 4))
            }
            for row in range(max(1, scale // 2))
        ],
        arrays={
            f"arr-{i}": [float(i), float(i + 1), float(i + 2)]
            for i in range(scale)
        },
        modev=[test_pc.Mode.MODE_A, test_pc.Mode.MODE_B, test_pc.Mode.MODE_C]
        * max(1, scale // 3),
    )


def touch_small(obj):
    if obj is None:
        return 0
    return obj.i32 + int(obj.flag) + len(obj.str)


def touch_main(root):
    total = 0
    total += root.i32 + root.u32 + root.i64 + root.u64
    total += int(root.flag) + root.mode + len(root.str) + len(root.data)
    total += int(root.f32 * 1000) + int(root.f64 * 1000)
    total += touch_small(root.object)
    total += sum(root.i32v)
    total += sum(root.u64v)
    total += sum(len(x) for x in root.strv)
    total += sum(len(x) for x in root.datav)
    total += sum(int(x * 1000) for x in root.f32v)
    total += sum(int(x * 1000) for x in root.f64v)
    total += sum(int(x) for x in root.flags)
    total += sum(touch_small(x) for x in root.objectv)
    total += root.t_u32 + root.t_i32 + root.t_s32 + root.t_u64 + root.t_i64 + root.t_s64
    total += sum(len(k) + v for k, v in root.index.items())
    total += sum(k + touch_small(v) for k, v in root.objects.items())
    total += sum(int(x * 1000) for row in root.matrix for x in row)
    for one in root.vector:
        total += sum(len(k) + sum(int(x * 1000) for x in v) for k, v in one.items())
    total += sum(len(k) + sum(int(x * 1000) for x in v) for k, v in root.arrays.items())
    total += sum(root.modev)
    return total


def touch_pb_small(obj):
    return obj.i32 + int(obj.flag) + len(obj.str)


def touch_pb_main(root):
    total = 0
    total += root.i32 + root.u32 + root.i64 + root.u64
    total += int(root.flag) + root.mode + len(root.str) + len(root.data)
    total += int(root.f32 * 1000) + int(root.f64 * 1000)
    total += touch_pb_small(root.object)
    total += sum(root.i32v)
    total += sum(root.u64v)
    total += sum(len(x) for x in root.strv)
    total += sum(len(x) for x in root.datav)
    total += sum(int(x * 1000) for x in root.f32v)
    total += sum(int(x * 1000) for x in root.f64v)
    total += sum(int(x) for x in root.flags)
    total += sum(touch_pb_small(x) for x in root.objectv)
    total += root.t_u32 + root.t_i32 + root.t_s32 + root.t_u64 + root.t_i64 + root.t_s64
    total += sum(len(k) + v for k, v in root.index.items())
    total += sum(k + touch_pb_small(v) for k, v in root.objects.items())
    total += sum(int(x * 1000) for row in getattr(root.matrix, "_") for x in getattr(row, "_"))
    for one in root.vector:
        total += sum(
            len(k) + sum(int(x * 1000) for x in getattr(v, "_"))
            for k, v in getattr(one, "_").items()
        )
    total += sum(
        len(k) + sum(int(x * 1000) for x in getattr(v, "_"))
        for k, v in getattr(root.arrays, "_").items()
    )
    return total


def touch_fb_bytes(obj):
    if obj is None:
        return 0
    return obj._Length()


def touch_fb_string(value):
    return 0 if value is None else len(value)


def touch_fb_small(obj):
    if obj is None:
        return 0
    return obj.I32() + int(obj.Flag()) + touch_fb_string(obj.Str())


def touch_fb_float_array(obj):
    if obj is None:
        return 0
    return sum(int(obj._(i) * 1000) for i in range(obj._Length()))


def touch_fb_arrmap(obj):
    if obj is None:
        return 0
    total = 0
    for i in range(obj._Length()):
        entry = obj._(i)
        total += touch_fb_string(entry.Key()) + touch_fb_float_array(entry.Value())
    return total


def touch_fb_main(root):
    total = 0
    total += root.I32() + root.U32() + root.I64() + root.U64()
    total += int(root.Flag()) + root.Mode() + touch_fb_string(root.Str()) + root.DataLength()
    total += int(root.F32() * 1000) + int(root.F64() * 1000)
    total += touch_fb_small(root.Object())
    total += sum(root.I32v(i) for i in range(root.I32vLength()))
    total += sum(root.U64v(i) for i in range(root.U64vLength()))
    total += sum(touch_fb_string(root.Strv(i)) for i in range(root.StrvLength()))
    total += sum(touch_fb_bytes(root.Datav(i)) for i in range(root.DatavLength()))
    total += sum(int(root.F32v(i) * 1000) for i in range(root.F32vLength()))
    total += sum(int(root.F64v(i) * 1000) for i in range(root.F64vLength()))
    total += sum(int(root.Flags(i)) for i in range(root.FlagsLength()))
    total += sum(touch_fb_small(root.Objectv(i)) for i in range(root.ObjectvLength()))
    total += root.TU32() + root.TI32() + root.TS32() + root.TU64() + root.TI64() + root.TS64()
    for i in range(root.IndexLength()):
        entry = root.Index(i)
        total += touch_fb_string(entry.Key()) + entry.Value()
    for i in range(root.ObjectsLength()):
        entry = root.Objects(i)
        total += entry.Key() + touch_fb_small(entry.Value())
    matrix = root.Matrix()
    if matrix is not None:
        for i in range(matrix._Length()):
            total += touch_fb_float_array(matrix._(i))
    for i in range(root.VectorLength()):
        total += touch_fb_arrmap(root.Vector(i))
    total += touch_fb_arrmap(root.Arrays())
    return total


def parse_pb_main(test_pb2, payload):
    root = test_pb2.Main()
    root.ParseFromString(payload)
    return root


def time_case(fn, iterations, repeats):
    samples = []
    checksums = []
    was_enabled = gc.isenabled()
    gc.disable()
    try:
        for _ in range(repeats):
            checksum = 0
            start = time.perf_counter_ns()
            for _ in range(iterations):
                checksum = (checksum + fn()) & 0xFFFFFFFFFFFFFFFF
            elapsed = time.perf_counter_ns() - start
            samples.append(elapsed / iterations)
            checksums.append(checksum)
    finally:
        if was_enabled:
            gc.enable()
    return samples, checksums


def print_case(name, samples, checksums):
    best = min(samples)
    median = statistics.median(samples)
    print(
        f"{name:24s} best={best / 1000:10.3f} us/op  "
        f"median={median / 1000:10.3f} us/op  "
        f"best={1_000_000_000 / best:10.1f} ops/s  "
        f"checksum={checksums[-1]}"
    )


def resolve_path(path):
    return path if path.is_absolute() else ROOT / path


def main():
    parser = argparse.ArgumentParser(description="Benchmark ProtoCache Python read/write paths.")
    parser.add_argument("-n", "--iterations", type=int, default=5000)
    parser.add_argument("-r", "--repeats", type=int, default=5)
    parser.add_argument("-s", "--scale", type=int, default=16)
    parser.add_argument(
        "-i",
        "--input",
        type=Path,
        default=ROOT / "test" / "benchmark" / "test.pc",
        help="ProtoCache binary to deserialize; defaults to the C++ benchmark fixture.",
    )
    parser.add_argument(
        "--protobuf-input",
        type=Path,
        default=ROOT / "test" / "benchmark" / "test.pb",
        help="protobuf binary fixture for comparison.",
    )
    parser.add_argument(
        "--flatbuffers-input",
        type=Path,
        default=ROOT / "test" / "benchmark" / "test.fb",
        help="FlatBuffers binary fixture for comparison.",
    )
    parser.add_argument(
        "--synthetic",
        action="store_true",
        help="benchmark a generated Python object instead of the C++ benchmark fixture.",
    )
    args = parser.parse_args()
    compression_inputs = [
        ("pb", Path("test/benchmark/test.pb")),
        ("pc", Path("test/benchmark/test.pc")),
        ("fb", Path("test/benchmark/test.fb")),
    ]

    if args.synthetic:
        root = make_main(args.scale)
        source = f"synthetic scale={args.scale}"
    else:
        payload_path = resolve_path(args.input)
        payload = payload_path.read_bytes()
        root = test_pc.Main.Deserialize(payload)
        source = str(payload_path)

    payload = root.Serialize()
    baseline = touch_main(test_pc.Main.Deserialize(payload))

    print(f"source={source}")
    print(f"protocache={len(payload)} bytes  baseline={baseline}")
    if not args.synthetic:
        test_pb2, fb_main = _generate_python_bindings()
        pb_path = resolve_path(args.protobuf_input)
        fb_path = resolve_path(args.flatbuffers_input)
        pb_payload = pb_path.read_bytes()
        fb_payload = fb_path.read_bytes()
        pb_root = parse_pb_main(test_pb2, pb_payload)
        fb_root = fb_main.Main.GetRootAs(fb_payload, 0)
        fb_baseline = touch_fb_main(fb_root)
        print(f"protobuf={len(pb_payload)} bytes  baseline={touch_pb_main(pb_root)}")
        print(f"flatbuffers={len(fb_payload)} bytes  baseline={fb_baseline}")
    print(f"iterations={args.iterations}  repeats={args.repeats}")

    read_samples, read_checksums = time_case(
        lambda: touch_main(test_pc.Main.Deserialize(payload)),
        args.iterations,
        args.repeats,
    )
    print_case("ProtoCache read+walk", read_samples, read_checksums)

    if not args.synthetic:
        pb_read_samples, pb_read_checksums = time_case(
            lambda: touch_pb_main(parse_pb_main(test_pb2, pb_payload)),
            args.iterations,
            args.repeats,
        )
        print_case("protobuf read+walk", pb_read_samples, pb_read_checksums)

        fb_read_samples, fb_read_checksums = time_case(
            lambda: touch_fb_main(fb_main.Main.GetRootAs(fb_payload, 0)),
            args.iterations,
            args.repeats,
        )
        print_case("flatbuffers read+walk", fb_read_samples, fb_read_checksums)

    walk_samples, walk_checksums = time_case(
        lambda: touch_main(root), args.iterations, args.repeats
    )
    print_case("ProtoCache walk", walk_samples, walk_checksums)

    if not args.synthetic:
        pb_walk_samples, pb_walk_checksums = time_case(
            lambda: touch_pb_main(pb_root), args.iterations, args.repeats
        )
        print_case("protobuf walk", pb_walk_samples, pb_walk_checksums)

        fb_walk_samples, fb_walk_checksums = time_case(
            lambda: touch_fb_main(fb_root), args.iterations, args.repeats
        )
        print_case("flatbuffers walk", fb_walk_samples, fb_walk_checksums)

    serialize_samples, serialize_checksums = time_case(
        lambda: len(root.Serialize()), args.iterations, args.repeats
    )
    print_case("ProtoCache serialize", serialize_samples, serialize_checksums)

    if not args.synthetic:
        pb_serialize_samples, pb_serialize_checksums = time_case(
            lambda: len(pb_root.SerializeToString()), args.iterations, args.repeats
        )
        print_case("protobuf serialize", pb_serialize_samples, pb_serialize_checksums)

    print("========compress========")
    for name, path in compression_inputs:
        raw = resolve_path(path).read_bytes()
        cooked = pc.compress(raw)
        if pc.decompress(cooked) != raw:
            raise RuntimeError(f"{name} compression round trip changed benchmark data")
        print(f"{name}: raw={len(raw)} bytes  compressed={len(cooked)} bytes")

        compress_samples, compress_checksums = time_case(
            lambda raw=raw: len(pc.compress(raw)),
            args.iterations,
            args.repeats,
        )
        print_case(f"{name} compress", compress_samples, compress_checksums)

        decompress_samples, decompress_checksums = time_case(
            lambda cooked=cooked: len(pc.decompress(cooked)),
            args.iterations,
            args.repeats,
        )
        print_case(f"{name} decompress", decompress_samples, decompress_checksums)


if __name__ == "__main__":
    main()
