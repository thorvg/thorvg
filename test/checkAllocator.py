#!/usr/bin/env python3
"""Reject new/delete of public plain-data types in the active ThorVG build.

Uses canonical Clang types, so aliases, macros, one-dimensional arrays and
instantiated templates are checked too. This is a build-configuration check, not pointer provenance
analysis: casts to unrelated types and inactive preprocessor branches are outside
its scope. Placement construction in existing storage is allowed.
"""

import argparse
import json
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


#Keep in sync with the compile-time checks in testAllocatorData.cpp.
DATA_TYPES = (
    "::tvg::Matrix", "::tvg::Point", "::tvg::TextMetrics",
    "::tvg::GlyphMetrics", "::tvg::Fill::ColorStop",
)
ROOT = Path(__file__).resolve().parents[1]
RECORD = "cxxRecordDecl(hasAnyName(" + ", ".join(map(json.dumps, DATA_TYPES)) + "))"
POINTER = f"qualType(hasCanonicalType(pointerType(pointee(recordType(hasDeclaration({RECORD}))))))"
#The standard placement overload takes (size_t, void*). Unlike nothrow new,
#it only constructs an object in storage which has already been allocated.
PLACEMENT = 'hasDeclaration(functionDecl(parameterCountIs(2), hasParameter(1, hasType(pointerType(pointee(voidType()))))))'
QUERIES = (
    f"match stmt(anyOf(cxxNewExpr(hasType({POINTER}), unless({PLACEMENT})), "
    f"cxxDeleteExpr(has(expr(hasType({POINTER}))))))",
    f'match cxxRecordDecl({RECORD}, isDefinition(), isDerivedFrom("::tvg::Allocator"))',
)


def check(clang_query, sources, build=None, flags=None):
    command = [clang_query]
    if build:
        command += ["-p", str(build)]
    for query in QUERIES:
        command += ["-c", query]
    command += [str(source) for source in sources]
    if flags is not None:
        command += ["--", *flags]
    result = subprocess.run(command, text=True, capture_output=True)
    counts = re.findall(r"^(\d+) match(?:es)?\.$", result.stdout, re.MULTILINE)
    if result.returncode or len(counts) != len(QUERIES):
        raise RuntimeError(result.stdout + result.stderr or "clang-query produced no results")
    return sum(map(int, counts)), result.stdout + result.stderr


def self_test(clang_query):
    prefix = '#include <thorvg.h>\n#include <memory>\n#include <new>\n'
    cases = {
        "malloc/free and stack": ("void f() { tvg::Matrix m{}; auto p = tvg::malloc<tvg::Matrix>(sizeof(m)); *p = m; tvg::free(p); }", 0),
        "unrelated names": ("namespace other { struct Matrix {}; } void f() { delete new other::Matrix; }", 0),
        "placement construction": ("void f() { alignas(tvg::Matrix) char storage[sizeof(tvg::Matrix)]; new (storage) tvg::Matrix{}; }", 0),
        "scalar": ("void f() { delete new tvg::Matrix; }", 2),
        "alias array": ("using M = tvg::Matrix; void f() { auto p = new M[2]; delete[] p; }", 2),
        "nothrow": ("void f() { auto p = new (std::nothrow) tvg::Matrix; tvg::free(p); }", 1),
        "macro": ("#define MAKE(T) new T\nvoid f() { tvg::free(MAKE(tvg::Point)); }", 1),
        "template": ("template<class T> T* make() { return new T; } void f() { tvg::free(make<tvg::Matrix>()); }", 1),
        "member delete": ("struct Owner { tvg::Matrix* p; ~Owner() { delete p; } };", 1),
        "default deleter": ("void f(tvg::Matrix* p) { std::default_delete<tvg::Matrix>{}(p); }", 1),
    }
    with tempfile.TemporaryDirectory(prefix="thorvg-allocator-check-") as directory:
        source = Path(directory) / "probe.cpp"
        flags = ["-std=c++14", "-I" + str(ROOT / "inc")]
        for name, (code, expected) in cases.items():
            source.write_text(prefix + code)
            count, output = check(clang_query, [source], flags=flags)
            if count != expected:
                raise RuntimeError(f"Self-test {name}: expected {expected}, got {count}\n{output}")
        #Independent fixture for accidental direct or indirect inheritance.
        source.write_text('namespace tvg { struct Allocator {}; struct Intermediate : Allocator {}; struct Matrix : Intermediate {}; }')
        count, output = check(clang_query, [source], flags=["-std=c++14"])
        if count != 1:
            raise RuntimeError("Self-test inheritance failed\n" + output)
    print(f"Allocator checker: {len(cases) + 1} self-tests passed.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path, help="Meson build directory containing compile_commands.json")
    parser.add_argument("--clang-query", default="clang-query")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    clang_query = shutil.which(args.clang_query)
    if not clang_query:
        parser.error("clang-query is required (install clang-tools or use --clang-query)")
    if args.self_test:
        self_test(clang_query)
    database = json.loads((args.build / "compile_commands.json").read_text())
    sources = set()
    for entry in database:
        source = (Path(entry["directory"]) / entry["file"]).resolve()
        if ROOT / "src" in source.parents:
            sources.add(source)
    if not sources:
        parser.error("No ThorVG source files found in the compilation database")
    #Bound AST memory consumption by checking one translation unit at a time.
    violations = 0
    for source in sorted(sources):
        count, output = check(clang_query, [source], build=args.build.resolve())
        if count:
            print(output)
            violations += count
    if violations:
        print("Plain-data types must not inherit Allocator. Use tvg::malloc/calloc/realloc and tvg::free instead of new/delete.")
        return 1
    print(f"Allocator policy passed for {len(sources)} translation units.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
