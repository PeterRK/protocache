from pathlib import Path

from setuptools import Extension, setup

ROOT = Path(__file__).resolve().parents[1]

setup(
    name="protocache",
    version="0.1.0",
    packages=["protocache"],
    ext_modules=[
        Extension(
            "protocache._protocache",
            sources=[
                "protocache/_protocache.cc",
                str(ROOT / "src" / "access.cc"),
                str(ROOT / "src" / "hash.cc"),
                str(ROOT / "src" / "perfect_hash.cc"),
                str(ROOT / "src" / "serialize.cc"),
                str(ROOT / "src" / "utils.cc"),
            ],
            include_dirs=[str(ROOT / "include")],
            language="c++",
            extra_compile_args=["-std=c++17"],
        )
    ],
)
