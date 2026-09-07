# Markdown renderer forces Lato for bold text and headlines when no bold font is set

**Date:** 2026-08-30 (noticed in Sublime, `Scripts/LicensePanel.js`)
**Target commit:** `2769f674d` (branch `meatbeats`)
**Status:** DONE. Committed on meatbeats as `2b5a98391` (2026-08-30): fromDynamicObject() no longer forces useSpecialBoldFont on for the "default" bold font, the explicit-name branch sets it instead, and toDynamicObject() writes BoldFont as "default" while the flag is false. Visually confirmed by Dan in Sublime. Fork-only, not upstreamed. Moved to done 2026-09-07.
**Affects:** `MarkdownLayout::StyleData` (JSON path) and the `MarkdownPanel` floating tile

## Symptom

A `MarkdownPanel` configured with `"Font": "Barlow"` and `"BoldFontName": "Default"`
renders its body in Barlow but every headline and every `**bold**` span in Lato - a
different typeface, not just a heavier weight. Headlines are drawn in the bold font
(`MarkdownParser.cpp:111`, `currentFont = styleData.getBoldFont()...`), so the whole
heading hierarchy switches face.

## Cause

`StyleData::getBoldFont()` (`MarkdownLayout.h` ~119) already has the right fallback: when
`useSpecialBoldFont` is false it returns `FontHelpers::getFontBoldened(getFont())`, i.e.
the current typeface with a synthesised bold weight. Two callers forced the flag on for
the "no bold font" case:

1. `StyleData::fromDynamicObject()` (`MarkdownLayout.cpp` ~300) - the `bName == "default"`
   branch set `boldFont = GLOBAL_BOLD_FONT()` *and* `useSpecialBoldFont = true`, which also
   overrode the `UseSpecialBoldFont` value read from the JSON a few lines earlier.
2. `MarkdownPreviewPanel::fromDynamicObject()` (`ScriptingContentComponent.h` ~585) - the
   floating tile path, which is the one Sublime hits. `BoldFontName` defaults to `""`, but
   an explicit `"Default"` is non-empty, so it set `useSpecialBoldFont = true` with
   `getFontFromString("Default")`, i.e. the global font.

## Fix

`hi_tools/hi_markdown/MarkdownLayout.cpp`:

- `bName == "default"` no longer sets `useSpecialBoldFont`; it keeps whatever the JSON's
  `UseSpecialBoldFont` said (default false in the `StyleData` declaration), so bold falls
  back to the boldened current font.
- The `else` branch (an explicitly named bold font) now sets `useSpecialBoldFont = true`,
  which the `"default"` branch used to do for it. Semantics for those callers are unchanged.
- `toDynamicObject()` writes `BoldFont` as `"default"` when `useSpecialBoldFont` is false,
  so a save/load round-trip no longer resurrects the flag via the `else` branch.

`hi_scripting/scripting/components/ScriptingContentComponent.h`:

- Treat `BoldFontName == "Default"` like an empty name, so the panel falls back to
  boldening its own font instead of switching to the global typeface.

## Other consumers checked

- `GLOBAL_BOLD_FONT()` elsewhere in `hi_tools/hi_markdown/` and
  `hi_core/hi_components/markdown_components/` is renderer chrome only (error text, search
  result headers, the "Empty" placeholder image, TOC items) - not the body/headline path.
- `simple_css` (`StyleSheet.cpp` ~947) sets `boldFont = f.boldened()` with the flag on,
  which is equivalent to the new fallback.
- `MessageWithIcon` (`PresetHandler.cpp` ~3179) assigns `boldFont` but never sets the flag,
  so alert windows already used the boldened font. Unchanged.
- `ScriptedLookAndFeel::getAlertWindowMarkdownStyleData` likewise assigns `boldFont`
  without the flag - pre-existing, untouched here.

Behaviour change for anyone who was relying on the old forcing: the multipage dialogs
(`MultiPageDialog.cpp` 1041/1751) and `MarkdownObject.setStyleData` now bolden their body
font instead of using Lato Bold. Where the body font is the global font this is a
synthesised-bold Lato vs real Lato Bold difference (slightly different weight/metrics,
same face); where the body font was custom it is the actual fix. The HISE editor's own
documentation viewer falls in the first group.

Not touched in this pass: headline size multipliers, margins, underline rules.

## Verification

1. Rebuild the Debug standalone.
2. Open Sublime, open the license panel: headlines and `**bold**` spans render in Barlow
   (heavier weight), matching the body text.
3. Check a panel with an explicit `BoldFontName` (a real font name) still uses that font.
4. Check the HISE editor's documentation viewer and an alert window still look right.
