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
$ meson setup build -Dtests=true -Dloaders="all" -Dsavers="all" -Dbindings="capi" -Dtools="all" -Dlog=true -Db_sanitize="address,undefined"
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
[Module][Feature]: [Title]

[Description]

- [Module] refers to the sub-module primarily affected by your change. Most of the time, this indicates the name of a sub-folder. This helps identify the appropriate reviewers for your change. If your change doesn't belong to any sub-modules, you can either replace this with a suitable name or skip it. The name should be written entirely in lowercase letters.
  - e.g., build, doc, infra, common, cpu_engine, gl, svg, wasm, svg2png...

- [Feature] indicates the primary function or feature you modified. This field is optional.
  - e.g., canvas, shape, paint, scene, picture, task-scheduler, loader, builder, ...

- [Title] provides a brief description of your change and should be encapsulated in a single sentence.
  - e.g., "Fixed a typo."
  - e.g., "Addressed compile warnings."
  - e.g., "Refactored code."
  - e.g., "Resolved a rendering bug causing overlapped shapes to display improperly."

- [Description] doesn't have a strict format. However, it should detail what you accomplished in this patch as comprehensively as possible.

 If you've resolved bugs, include the following details:
  - The type of bug.
  - Steps to reproduce it.
  - The root cause.
  - The solution applied.

  For new features or functions, cover:
  - The nature of the feature.
  - Full API specifications (if there are any API additions).
  - Its necessity or rationale.
  - Any conditions or restrictions.
  - Relevant examples or references.

 Finally, include any related issue ticket numbers in this section, if applicable.


Here's a sample commit message for clarity:

- renderer/paint: Introduced path clipping feature

  We've added a new method, Paint::composite(), to support various composite behaviors. This allows paints to interact with other paint instances based on the chosen composite method. Initially, we're introducing the "ClipPath" method for path-based clipping.

  targetPaint->composite(srcPaint, CompositeMethod::ClipPath);

  Note: If the source paint lacks path information, clipping may not produce the desired effect.

  API Additions:
  enum CompositeMethod {None = 0, ClipPath};
  Result Paint::composite(std::unique_ptr<Paint> target, CompositeMethod method) const noexcept;

  Examples: Introduced ClipPath

  References: [Provide any relevant links, such as screenshots.]

  Issues: [Link to the issue]

## Pull Request

Once you submit a Pull Request, please ensure the following:
- Reviewers are automatically assigned.
- You are listed as an assignee.
- Appropriate labels are applied.
- All automated integration tests pass.
<p align="center"><img width="1000" height="auto" src="https://github.com/thorvg/thorvg.site/blob/main/readme/contribution.png"></p>
