# Float casts dust preset/UI XML with e-notation values

**Date:** 2026-07-29 (diagnosed in Sublime; present since Sublime's first commit, 2025-07-27)
**Target commit:** `a1cb6a524` (branch `meatbeats`)
**Status:** DONE — applied and committed on `meatbeats` as `b58eef722` (all three hunks, build verified 2026-07-29). Hunks 1+3 upstreamed as PR #1011 (https://github.com/christophhart/HISE/pull/1011, branch `fix/content-float-cast-precision`, commit `638981ea5` off upstream `7d4faae2f`); hunk 2 (exportAsValueTree rounding) deliberately held back as a possible follow-up PR. Companion patch: `float-dust-in-content-properties.patch`. Follow-up: the module tree's own attribute dust (a separate source, double-precision residue rather than float casts) is fixed in `flush-attribute-dust-in-module-tree.md` (meatbeats `d903eb22b`).
**Affects:** every ScriptSlider stored value and every numeric UI property, in all projects

## Symptom

Stored control values in the project XML `<Content>` block and in `.preset` files show scientific-notation dust instead of clean decimals:

```xml
<Control type="ScriptSlider" id="knbOutputLevel" value="1.490116119384766e-6"/>
<Control type="ScriptSlider" id="knbOutputLowEQ" value="8.940696716308594e-8"/>
```

Both knobs are sitting at exactly 0.0 dB. The dust also hides in plain-decimal form on non-zero values (`3.299999952316284` for 3.3); it only turns into e-notation when the true value is 0 on a knob with a negative minimum, because then the dust IS the entire number.

Hand-cleaning the XML does not stick: HISE rewrites the dust on the next load/save cycle. Double-clicking the knob to reset it to 0 doesn't stick either — the reset value gets re-snapped onto the dusty grid.

## Root cause

Three float casts in `hi_scripting/scripting/api/ScriptingApiContent.cpp`, one primary and two accomplices:

**1. `Helpers::sanitizeNumberProperties` (~line 10111) — the primary.** Called on EVERY project load (`ScriptProcessor.cpp` ~1875/1887), it walks the whole ContentProperties tree and casts every numeric property through `float`:

```cpp
float valueAsNumber = (float)copy.getProperty(id);
```

So `stepSize="0.1"` in the XML becomes `0.10000000149011612` in memory (and `0.1000000014901161` on the next save). This warps the snap grid: `NormalisableRange::snapToLegalValue` computes `min + n × step`, so for a Decibel knob with `min="-100"` the legal value nearest 0 dB is

```
-100 + 0.10000000149011612 × 1000 = 1.4901161193847656e-6
```

— exactly the stored value. Clean 0.0 is not ON the grid; no amount of snapping or resetting can store it. (With a clean double 0.1 the same arithmetic lands on exactly 0.0: `0.1 × 1000` rounds to `100.0` in double precision.) The `knbOutputLowEQ` value is the same formula with `min="-6"`: `-6 + 0.1f × 60 = 8.940696716308594e-8`.

**2. `ScriptSlider::setScriptObjectPropertyWithChangeMessage`, `defaultValue` case (~line 2209).** Same `(float)` cast pattern; re-dusts the `defaultValue` property when it is set on load, undoing fix 1's cleanup for that property.

**3. Serialization writes the raw double.** `ScriptComponent::exportAsValueTree` stores the component value verbatim, so dust from any source (including float32 processor attributes, e.g. `2.70000147819519` for 2.7) lands in the file as-is — while the UI value display already rounds to the step's precision and shows a clean "0.0 dB".

(There is a fourth minor cast in `ScriptSlider::resetValueToDefault` (~line 2296); the serialization fix below makes its effect invisible in stored data, so it is deliberately left out to keep the patch minimal.)

## Fix

Three hunks — see `float-dust-in-content-properties.patch`:

1. `sanitizeNumberProperties`: sanitize in `double` via `FloatSanitizers::sanitizeDoubleNumber` (already exists in `hi_tools/hi_tools/UpdateMerger.h`). Stops the per-load contamination; NaN/Inf protection unchanged.
2. `ScriptSlider::exportAsValueTree`: round the stored value to the step size's decimal precision plus one guard digit (step 0.1 → 2 decimals, step 1.0 → 1, step 0 / continuous → untouched). Mirrors what the value display already does. This is the belt-and-braces layer: it also self-heals values that are already dusty (a stored `1.49e-6` rounds to `0.0`, `3.299999952316284` becomes `3.3`) and covers dust entering through float32 processor attributes, which hunk 1 can't reach. Guarded on `value.isDouble()` so JSON/string/int values pass through untouched.
3. `defaultValue` property case: `jlimit` + sanitize in `double`.

This path is shared: `PresetHandler.cpp:127` saves user presets through the same `Content::exportAsValueTree`, so one fix covers the project XML `<Content>` block AND `.preset` files.

## Verification

- Build the fork, load a project whose XML has `stepSize="0.1"` on a Decibel slider (min -100), save: the stepSize must survive as `0.1` (previously rewritten to `0.1000000014901161`).
- Set the slider to 0 dB, save: stored value must be `0.0`, not `1.49e-6`.
- Load + resave an existing project with dusty stored values: they must come out clean (self-heal) with no audible/behavioural change — the correction is below half a step everywhere.
- Sublime is the live test case: `XmlPresetBackups/Sublime.xml` currently shows both e-values after any save; its `.preset` files clean up progressively as each is resaved.

## Upstream

Straightforwardly upstreamable — the float casts look like habit rather than intent (the sanitizers exist in double form already). Worth a forum post to Christoph with the `1.49e-6 = -100 + float32(0.1) × 1000` fingerprint, since every HISE project with dB sliders ships this dust in its presets.
