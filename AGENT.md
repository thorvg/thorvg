# ThorVG Code Review Guidelines

Follow [CONTRIBUTING.md](CONTRIBUTING.md) and the [Coding Convention](https://github.com/thorvg/thorvg/wiki/Coding-Convention).

## Review Priorities

- **Correctness and compatibility** — Compare behavior before and after the change. Check edge cases, error handling, rendering correctness, public APIs, supported platforms and builds, and CPU/GPU consistency. Trace changes to shared code through affected callers.
- **Memory and thread safety** — Check ownership, lifetimes, invalid memory access, leaks, undefined behavior, and synchronization of shared state.
- **Performance and reuse** — Look for avoidable allocations, copies, and computation in hot paths. Suggest existing helpers or shared logic when they improve correctness, consistency, or maintainability within performance and dependency constraints.
- **Binary size** — Suggest behavior-preserving alternatives that save at least 100 bytes. Compare equivalent Meson builds with `-Doptimization=s` and report the measured artifact or section and byte savings. Mark unmeasured candidates as unverified.

## Findings

- Review surrounding code and existing comments. Skip unrelated pre-existing issues and root causes already reported, regardless of wording or line number. Omit speculation, subjective preferences, and refactors without concrete benefit. Omit findings based solely on coding conventions.
- Order actionable findings by severity. Give the code location, trigger, impact, and a focused fix when practical. Label issues introduced by the change as regressions.
- For regressions, provide a minimal runnable C++ sample in `thorvg.example` style when feasible. If an asset alone reproduces the issue, create and attach a minimal Lottie or SVG file. Include how to run or load the reproduction and what to observe before and after the change.
- Say when no actionable findings remain. State what source review, builds, or tests verified, what was run, and what remains unverified.
