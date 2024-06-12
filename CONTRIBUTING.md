We always appreciate your contribution. ThorVG doesn't expect patches to be perfect; instead, we value contributions that make ThorVG better than before. This page outlines the ThorVG contribution format.<br />
<br />
## Reviewers
ThorVG uses GitHub infrastructure to automatically assign code reviewers for your changes. To see the full list of reviewers, please refer to the [CODEOWNERS](https://github.com/thorvg/thorvg/blob/main/CODEOWNERS) file.
<br />

## Self Test & Verification
After updating the ThorVG code, please ensure your changes don't break the library. We recommend conducting unit tests. You can easily run them using the following build commands:
<br/>
`
$meson . build -Dtests=true -Dloaders="all" -Dsavers="all" -Dbindings="capi" -Dtools="all" -Dlog=true
`
<br />
`
$ninja -C build test
`
<br/>
<br/>
Please make it sure running all tests and no any fail case.<br/>
<br/>
Expected Fail:      0<br/>
Fail:               0<br/>
Unexpected Pass:    0<br/>
Skipped:            0<br/>
Timeout:            0<br/>

## Commit Message
[Module][Feature]: [Title]

[Description]

- [Module] refers to the sub-module primarily affected by your change. Most of the time, this indicates the name of a sub-folder.
This also suggests who might need to review your patch.
If your change doesn't belong to any sub-modules, you can either replace this with a suitable name or skip it.
The name should be written entirely in lowercase letters.
  - e.g., build, doc, infra, common, sw_engine, gl_engine, svg_loader, examples, wasm, svg2png...

- [Feature] indicates the primary function or feature you modified. Typically, this represents a class or file name.
This is an optional.
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

Once you've submitted a pull request (PR), please ensure the following checklist is completed:
- Reviewers: Check the Reviewers List.
- Assignees: You should be assigned.
- Labels: Mark the appropriate Patch Purpose.
- Automated Integration Test: All must pass.
<p align="center"><img width="1000" height="1072" src="https://github.com/thorvg/thorvg/blob/main/res/contribution.png"></p>
