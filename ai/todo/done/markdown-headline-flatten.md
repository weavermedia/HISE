# Markdown renderer: flatten headline styling (sizes, margins, underline)

**Date:** 2026-08-30 (Sublime license panel styling pass)
**Target commit:** `2b5a98391` (branch `meatbeats`)
**Status:** Landed in meatbeats `8886cb5f8` (2026-08-31), applied exactly as proposed below. Fork-only, not upstreamed. The margin re-tune described under "Next pass" is still open and will get its own todo if it goes ahead.
**Affects:** headline rendering only (`MarkdownParser::Headline`), all markdown consumers

## Motivation

MarkdownPanel headlines were oversized and carried built-in decoration (an intrinsic top
margin that grew for higher levels, an extra 20 px bump for every non-first headline, and
a grey underline under H1-H3). That made it impossible to judge the plugin-side styling on
its own. This pass strips the intrinsic decoration so the styling can be re-tuned from a
neutral baseline. Body text, lists, links and tables are untouched, and headlines still
render in the bold font (`MarkdownParser.cpp:111`).

## Changes

`hi_tools/hi_markdown/MarkdownLayout.h` (~93)

- `headlineFontSize` multipliers (H1-H4, relative to `fontSize`) go from
  `{ 2.375f, 1.9375f, 1.5f, 1.2f }` to `{ 2.0f, 1.75f, 1.5f, 1.25f }`. Still four levels;
  the parser clamps to 4 so no H5/H6 entries are needed.

`hi_tools/hi_markdown/MarkdownElements.cpp`, `Headline` constructor (~260)

- Delete the `topMargin` computation (`15.0f + ((4 - level) * 5) * getZoomRatio()`) and
  pass `{ 0.0f, 0.0f }` as the default to `styleData.getMargin(...)`. The default is only
  a fallback, so a stylesheet's `h1 { margin-top: ... }` still overrides it via
  `sd.margins` (written by `StyleSheet.cpp` ~960).
- Delete the `if(!isFirst) margins.first += 20.0f;` bump. The `isFirst` member and
  constructor argument stay - they will be wanted when margins are reintroduced.

`hi_tools/hi_markdown/MarkdownElements.cpp`, `Headline::draw()` (~287)

- Delete the grey underline (`g.setColour(Colours::grey.withAlpha(0.2f))` plus the
  `if(headlineLevel <= 3) g.drawHorizontalLine(...)`). `imgOffset` is kept; it is still
  used by the image branch above.

## Dependants checked

- `headlineFontSize` is read in `MarkdownParser.cpp:109` (font size) and
  `MarkdownElements.cpp:306` (y offset); both just scale with the new values.
- `StyleSheet.cpp` 963/969/975/981 uses the array entries as the fallback when a CSS
  `h1`-`h4` rule has no `font-size`, so CSS-styled markdown picks up the new baseline.
  It writes its own margin defaults (29.1/24.4/19.1/5.0 top, 10.1 bottom) whenever the
  corresponding rule exists, so the CSS path is unaffected by the margin removal.
  Not changed.
- The other `drawHorizontalLine` calls in `MarkdownElements.cpp` (240, 1150, 1162) belong
  to the horizontal-rule and table elements, not headlines.
- `margins.first` / `margins.second` are consumed by `getTopMargin()` (365),
  `getHeightForWidth()` (323) and `draw()` (273); with both at zero a headline now
  occupies exactly its text height.

## Verification

- Debug standalone builds cleanly; no warnings in `MarkdownElements.cpp`,
  `MarkdownLayout.h` or `MarkdownParser.cpp`.
- Visual check pending: open Sublime's license panel and look at the headline hierarchy
  with no intrinsic margins, then decide what margins to reintroduce.

## Next pass

Reintroduce headline margins deliberately (probably bottom-weighted, and using `isFirst`
to suppress the leading gap) once the flattened baseline has been seen in context.
