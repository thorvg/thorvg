This page guides a format for your contribution.

## Commit Message
[Module][Feature] - [Title]

[Description]
##

- [Module] is a sub module majorly affected by your change. Most of times this indicates a sub folder name.
This indicates whom need to review your patch as well.
If your change don't belonged to any sub modules, you can replace with proper name or skip it.
The name must be written in all lower alphabet characters.
  - ex) build / doc / infra / common / sw_engine / gl_engine / svg_loader / examples / wasm / svg2png  ...

- [Feature] is what major function/feature you changed. Normally this indicates a representive file name. 
You can keep the file name, but don't please contain any prefix(tvg) nor suffix(Impl) here.
  - ex) Canvas / TaskScehduler / SvgLoader / SvgBuilder / SwRle / GlRenderer / ...

- [Title] is a brief description of your change. It must be described in one sentence.
  - ex) "Fixed a typo"
  - ex) "Fixed compile warnings"
  - ex) "Code refactoring"
  - ex) "Fixed a rendering bug that overlapped shapes inproper way."
  
- [Description] There is no any strict formats, but it must describe what you did in this patch as far as possible you can describe in detail.

  If you fixed any bugs, it must contain below:
  - what type of bug  
  - conditions to reproduce it
  - root cause
  - solution 
  
  Or if you add a new feature or function, it must contain below:
  - what sort of features
  - api full specification (if any api additions)
  - any necessity
  - condition / restriction  
  - reference or sample
  
  Lastly, please append any issue ticket numbers in this section if any.
  
  - ex) common composite: newly added path clipping feature

    We introduced new method Paint::composite() to support composite behaviors. </br>
    This allows paints to composite with other paints instances. </br>
    Composite behaviors depends on its compoite method type. </br>
    Here we firstly introduced "ClipPath" method to support clipping by path unit of paint.</br>    
    
    tagetPaint->composite(srcPaint, CompoisiteMethod::ClipPath);</br>
    
    Beaware if the source paint doesn't contain any path info, clipping won't be applied as you expected.
    
    @API Additions:</br>
    enum CompositeMethod {None = 0, ClipPath}; </br>
    Result Paint::composite(std::unique_ptr<Paint> target, CompMethod method) const noexcept;</br>

    @Examples: added ClipPath</br>

    @Issues: 49
  
  
