We truly appreciate your contributions. While ThorVG does not expect patches to be perfect, we value changes that move the project forward. This document outlines our contribution guidelines and formatting requirements.<br />
<br />

## Coding Convention
Please read the [ThorVG Coding Convention Guidance](https://github.com/thorvg/thorvg/wiki/Coding-Convention) before writing your code.

## Code Formatting
ThorVG uses `.clang-format` (version 18) at the repository root to enforce a consistent C/C++ coding style. All contributions must be properly formatted before submitting a Pull Request.

### Install clang-format
**Ubuntu**
```
sudo apt install clang-format
```
**macOS**
```
brew install clang-format
```
**Windows**
```
Download and install LLVM from: https://llvm.org/builds/
Make sure clang-format is added to your system PATH
```

### Recommended Workflow
To keep the Git history clean, we recommend formatting your code after creating a commit and then amending it. You can use the ThorVG helper script (`tvg-format.sh`):
```
$ ./tvg-format.sh <git-sha>    # Run in the ThorVG root directory
$ git commit --amend           # Amend the formatting changes after self-review
```

## Reviewers
ThorVG uses GitHub infrastructure to automatically assign code reviewers for your changes. To see the full list of reviewers, please refer to the [CODEOWNERS](https://github.com/thorvg/thorvg/blob/main/CODEOWNERS) file.
<br />

## Self Test & Verification
After updating the ThorVG codebase, please ensure that your changes do not break the library. We recommend running unit tests using the following commands:
```
$ meson setup build -Dtests=true -Dloaders="all" -Dsavers="all" -Dbindings="capi" -Dlog=true -Db_sanitize="address,undefined"
$ ninja -C build test
```
Please make sure that all tests pass without failures:
```
Expected Fail:      0
Fail:               0
Unexpected Pass:    0
Skipped:            0
Timeout:            0
```
## Commit Message

Keep each line of the commit message within 72 characters, except for API declarations, URLs, code snippets, or other references.

### Format

```text
module[/feature]: Short description

[Detailed description]

[issue: #123]
```

- **Module** identifies the primary sub-module affected by the change. Use lowercase names, preferably matching the corresponding source directory (e.g., `build`, `doc`, `infra`, `common`, `cpu_engine`, `gl`, `svg`, `wasm`). For changes spanning multiple modules, choose the most relevant one or use a suitable general category.
- **Feature** optionally identifies a specific component or functionality within the module (e.g., `canvas`, `shape`, `paint`, `scene`, `picture`, `loader`, `task-scheduler`).
- **Title** briefly summarizes the change in a single line. Use the imperative mood (e.g., `fix`, `add`, `update`, `refactor`, `optimize`) and avoid unnecessary details.
- **Description** explains what changed and why. It is optional for trivial changes but recommended for bug fixes, new features, performance improvements, and significant refactoring.

### Description Guidelines

For **bug fixes**, describe the following when relevant:

- The incorrect behavior and steps to reproduce it, or a link to the relevant issue for details.
- The root cause of the problem.
- How the change resolves the issue.

For **new features or API changes**, include:

- The functionality introduced and its purpose.
- Any added, modified, or removed public API declarations.
- Relevant limitations, restrictions, or compatibility concerns.
- Usage examples or references, when helpful.

For **performance improvements or refactoring**, explain the motivation, significant implementation changes, and their impact. Include benchmark results when applicable.

Reference related GitHub issues using issue: #123, when applicable.

For **API changes**, prefix each API declaration with a symbol indicating the type of change:

- `+` Added API
- `*` Modified API
- `-` Removed API

Example:

```cpp
+Result Paint::composite(std::unique_ptr<Paint> target, CompositeMethod method);
*Result Paint::opacity(uint8_t opacity) noexcept;
-Result Paint::deprecated() noexcept;
```

### Example

```text
renderer/paint: add path clipping support

Introduce Paint::composite() to support compositing between paint
objects. Initially, the API supports ClipPath for path-based clipping.

The source paint defines the clipping region applied to the target.

API:
 +Result Paint::composite(std::unique_ptr<Paint> target, CompositeMethod method)

Note: The clipping source must contain valid path geometry.

issue: #123
```

## Pull Request

Once you submit a Pull Request, please ensure the following:
- Reviewers are automatically assigned.
- You are listed as an assignee.
- Appropriate labels are applied.
- All automated integration tests pass.
<p align="center"><img width="1000" height="auto" src="https://github.com/thorvg/thorvg.site/blob/main/readme/contribution.png"></p>
