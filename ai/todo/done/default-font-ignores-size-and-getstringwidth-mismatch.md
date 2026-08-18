# "Default" font ignores the size argument; g.getStringWidth measures a table at the requested size, not the drawn font

**Date:** 2026-08-18 (noticed in Sublime while chasing off-centre knob labels)
**Target commit:** `c94fc43ea` (branch `meatbeats`)
**Status:** APPLIED on `meatbeats` (2026-08-18, build + Sublime repro verified: `+nudge` and `+measure` rows centred for both "Default" and "ReproFont"; uncommitted). Bug 1 fixed as proposed. Bug 2 fixed the minimal way: the width table was introduced in `8744dddb9` to dodge a Windows threading crash in `Font::getStringWidthFloat` on the scripting thread, so it stays; instead `setGlobalFont` now rebuilds `defaultFont` from the global font's typeface (so the "Default" fallback measures what is drawn), and the index clamp is fixed (chars > 126 use an average-width slot at index 127 instead of reading past the table). Still open: non-`loadFontAs` names (system fonts, `"Default Bold"`) measure the global font, and kerning pairs are ignored – both pre-existing.
**Affects:** any script that draws with font name `"Default"` and a size; any script relying on `g.getStringWidth()` / `Engine.getStringWidth()` for layout

Two separate bugs that compound. Repro script + measurements: Sublime `ai/experiments/text-centering-repro.js` (draws `MMMMM` "centred" under a centred bar at increasing letter-spacing, then tries two corrections – both came out wrong with `"Default"`, both because of the bugs below).

## Bug 1 – `getFontFromString("Default", size)` drops the size

`hi_core/hi_core/MainController.cpp` ~2033:

```cpp
Font MainController::getFontFromString(const String& fontName, float fontSize) const
{
	if (fontName == "Default")
		return globalFont;          // <- fontSize ignored
	...
	// every other branch:
	return Font(typeface).withHeight(fontSize);
```

`globalFont` is `GLOBAL_FONT()` = `Font(oxygenTypeFace).withHeight(13.0f)` (`hi_tools/Macros.h:165`; the typeface is Lato Regular because `USE_LATO_AS_DEFAULT` defaults to 1, `hi_tools/hi_tools.h:211`), or the project's global font at 14 px if one is set (`setGlobalFont`, ~2073).

So `g.setFont("Default", 30)` and `g.setFontWithSpacing("Default", 30, x)` (`ScriptingGraphics.cpp` 2102 / 2113) silently draw at 13 px. Knock-on: `setExtraKerningFactor` scales by the *actual* height, so letter-spacing is `spacing × 13`, not `spacing × 30`. No error, no warning. Every other caller of `getFontFromString` (label wrappers, keyboard octave text, markdown style data, `Content.createPath`… ~12 sites) inherits the same behaviour for `"Default"`.

### Fix

```diff
 	if (fontName == "Default")
-		return globalFont;
+		return globalFont.withHeight(fontSize);
```

Callers that genuinely want the global font at its own size already pass 13/14 (`ScriptComponentWrappers.cpp:848` passes 14, `ScriptingGraphics.cpp:1586` passes 14). Worth a quick scan of the callers list (`grep -rn "getFontFromString(" hi_core hi_scripting`) for anything passing 0 / a bogus size that was previously masked – `SANITIZED(fontSize)` guards the two Graphics entry points.

## Bug 2 – `getStringWidth` measures a per-character table, and for non-embedded fonts measures the wrong size

`ScriptingGraphics.cpp` ~2414 and `ScriptingApi.cpp` ~3645 (`Engine.getStringWidth`) both go through:

```cpp
// MainController.cpp ~2019
float MainController::getStringWidthFromEmbeddedFont(text, fontName, fontSize, kerningFactor)
{
	for(auto& tf: customTypeFaces)          // loadFontAs fonts
		if (name matches) return tf.getStringWidthFloat(text, fontSize, kerningFactor);

	return defaultFont.getStringWidthFloat(text, fontSize, kerningFactor);   // <- fallback
}
```

`CustomTypeFace::getStringWidthFloat` (`MainController.h` ~2318) sums a normalised per-character width table (built once in the ctor, `MainController.cpp` ~1760, from `typeface->getStringWidth(single char)`) plus `kerning` per character, then multiplies by `fontSize`.

Problems:

1. **Size mismatch for `"Default"`.** `defaultFont` is a `CustomTypeFace` over `GLOBAL_FONT().getTypefacePtr()` (`MainController.cpp:136`, id `"Oxygen"`), so it is the right *typeface* for the stock global font – but it is scaled by the *requested* `fontSize` (30) while Bug 1 means the text was *drawn* at 13. Result: `getStringWidth` returned ~2.3× the rendered width in the repro. If the project sets a global font, `defaultFont` is also the wrong typeface.
2. **Any non-`loadFontAs` name** (system font names, `"Default Bold"`, …) silently measures Lato instead of what was drawn.
3. **Kerning pairs are ignored** (single-char table), so proportional fonts with real kerning measure slightly wide. Minor, but it means `getStringWidth` can never be used for exact centring, which is the main reason anyone calls it.
4. **Off-by-one out-of-bounds read**: `characterWidths[jlimit<uint8>(31, 128, c)]` on a `float characterWidths[128]` – any char ≥ 128 (every non-ASCII glyph, e.g. `°`, `–`, `…`) reads index 128, one past the end. Should clamp to 127 (and 32..126 is what the ctor fills; index 31 and 127 are 0).

### Fix

The `Graphics` object already holds the exact `Font` that was set (`currentFont`, `ScriptingGraphics.h:780`, assigned in `setFont`/`setFontWithSpacing` with height and kerning applied). Measure that:

```diff
 float ScriptingObjects::GraphicsObject::getStringWidth(String text)
 {
-	auto mc = getScriptProcessor()->getMainController_();
-	return mc->getStringWidthFromEmbeddedFont(text, currentFontName, currentFontHeight, currentKerningFactor);
+	return currentFont.getStringWidthFloat(text);
 }
```

That honours typeface, size, kerning factor and kerning pairs, and matches `drawText` exactly (same `Font` object). If the table exists for a reason (thread-safety of `Typeface::getStringWidth` off the message thread? speed?), the minimum fix is to keep the table but make the `"Default"` fallback consistent with Bug 1's fix, and clamp the index to 127:

```diff
-	normalisedLength += characterWidths[jlimit<uint8>(31, 128, c)];
+	normalisedLength += characterWidths[jlimit<uint8>(31, 127, c)];
```

`Engine.getStringWidth(text, fontName, fontSize, spacing)` has no `Font` object to hand; it should build one via `getFontFromString(fontName, fontSize)` + `setExtraKerningFactor(spacing)` and call `getStringWidthFloat` on it, so both APIs agree with what `g.drawText` will render.

## Verification

Sublime `ai/experiments/text-centering-repro.js`, `FONT = "Default"`, `SIZE = 30`:

- Before: `MMMMM` renders at ~13 px (same size as the 12 px captions); the `+measure` row (`getStringWidth` – trailing gap, left-aligned at `w/2 – tw/2`) lands ~35 px left of centre.
- After Bug 1 fix: text renders at 30 px; letter-spacing gap is `spacing × 30`; the `+nudge` row (rect shifted by `spacing × 30 / 2`) is centred.
- After Bug 2 fix: `+measure` row is centred and coincides with `+nudge`.
- `FONT = "ReproFont"` (a `loadFontAs` font) should already be correct on both counts before the fix – that's the control.

## Related

The motivating symptom (centred text with `setFontWithSpacing` sitting `spacing × height / 2` px left of centre) is plain JUCE behaviour – `Font::getGlyphPositions` adds the extra kerning after every glyph including the last, and `Justification::centred` centres the advance box. Not a HISE bug; noted here only because Bugs 1 and 2 made it impossible to correct from script with `"Default"`.
