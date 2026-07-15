import atexit
import os
import shutil
import sys
from pathlib import Path

from setuptools import Extension, setup


HERE = Path(__file__).resolve().parent
REPOSITORY_ROOT = HERE.parent
SDIST_NATIVE_ROOT = HERE / "native"
STAGE_MARKER = SDIST_NATIVE_ROOT / ".checkout-stage"
CORE_SOURCES = (
    "access.cc",
    "hash.cc",
    "perfect_hash.cc",
    "serialize.cc",
    "utils.cc",
)
CORE_PRIVATE_HEADERS = ("hash.h",)


def _has_native_sources(root):
    return (
        all((root / "src" / source).is_file() for source in CORE_SOURCES)
        and all((root / "src" / header).is_file() for header in CORE_PRIVATE_HEADERS)
        and (root / "include" / "protocache" / "access.h").is_file()
    )


def _remove_checkout_stage():
    if STAGE_MARKER.exists():
        shutil.rmtree(SDIST_NATIVE_ROOT, ignore_errors=True)


def _native_root():
    # A killed checkout build can leave its ignored temporary tree behind.
    _remove_checkout_stage()
    if _has_native_sources(SDIST_NATIVE_ROOT):
        return SDIST_NATIVE_ROOT
    if not _has_native_sources(REPOSITORY_ROOT):
        raise RuntimeError(
            "ProtoCache C++ sources are missing; build from a repository checkout "
            "or from the complete source distribution"
        )

    (SDIST_NATIVE_ROOT / "src").mkdir(parents=True)
    (SDIST_NATIVE_ROOT / "include" / "protocache").mkdir(parents=True)
    for source in CORE_SOURCES:
        shutil.copy2(REPOSITORY_ROOT / "src" / source, SDIST_NATIVE_ROOT / "src" / source)
    for header in CORE_PRIVATE_HEADERS:
        shutil.copy2(REPOSITORY_ROOT / "src" / header, SDIST_NATIVE_ROOT / "src" / header)
    for header in (REPOSITORY_ROOT / "include" / "protocache").glob("*.h"):
        shutil.copy2(header, SDIST_NATIVE_ROOT / "include" / "protocache" / header.name)
    STAGE_MARKER.touch()
    atexit.register(_remove_checkout_stage)
    return SDIST_NATIVE_ROOT


def _relative(path):
    return Path(os.path.relpath(path, HERE)).as_posix()


def _native_paths(root):
    sources = [_relative(HERE / "protocache" / "_protocache.cc")]
    sources.extend(_relative(root / "src" / source) for source in CORE_SOURCES)
    headers = sorted((root / "include" / "protocache").glob("*.h"))
    return sources, [_relative(root / "include")], [_relative(header) for header in headers]


def _compile_options():
    if sys.platform == "win32":
        return ["/std:c++17"], [("NOMINMAX", "1")]
    return ["-std=c++17"], []


native_root = _native_root()
sources, include_dirs, depends = _native_paths(native_root)
extra_compile_args, define_macros = _compile_options()

setup(
    ext_modules=[
        Extension(
            "protocache._protocache",
            sources=sources,
            include_dirs=include_dirs,
            depends=depends,
            language="c++",
            extra_compile_args=extra_compile_args,
            define_macros=define_macros,
        )
    ],
)
