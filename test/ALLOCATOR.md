Public data types (`Matrix`, `Point`, `TextMetrics`, `GlyphMetrics`, and
`Fill::ColorStop`) must remain trivial, standard-layout aggregates without an
`Allocator` base. `testAllocatorData.cpp` checks this in every unit-test build,
including C++14 aggregate initialization.

Inside ThorVG, allocate dynamic storage for these types with
`tvg::malloc/calloc/realloc` and release it with `tvg::free`. Stack values and
placement construction in existing storage are allowed.

Run the source policy check against a configured Meson build:

```sh
python3 test/checkAllocator.py build --self-test
```

This requires `clang-query` (the `clang-tools` package; use `--clang-query PATH`
for a versioned installation), plus Clang's OpenMP headers (`libomp-dev`) when
the build enables OpenMP. The Ubuntu PR workflow runs it with all supported
Linux loaders and engines enabled. A missing tool, parse error, or violation
fails the check; it is never silently skipped.

The check uses canonical types to reject scalar and one-dimensional array `new` and `delete`,
including aliases, macros, and instantiated templates. Its self-tests include
deliberately invalid examples. Add new protected data types to both the Python
type list and the compile-time checks.

This is not whole-program allocation-provenance analysis: uninstantiated
templates, disabled preprocessor branches, multidimensional arrays, type-erasing casts, indirect raw
allocation calls, and platform code absent from the compilation database are
not covered. Run it against each build configuration that needs coverage.
