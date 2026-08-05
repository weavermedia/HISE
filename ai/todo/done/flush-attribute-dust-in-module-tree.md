# Module tree serializes near-zero float dust as e-notation attributes

**Date:** 2026-07-29 (follow-up to `float-dust-in-content-properties.md`)
**Target commit:** `b58eef722` (branch `meatbeats`)
**Status:** DONE — applied, build + Sublime save verified by Dan, committed on `meatbeats` as `d903eb22b` (2026-08-05). Not yet upstreamed; candidate follow-up to PR #1011. Companion patch: `flush-attribute-dust-in-module-tree.patch`
**Affects:** every processor attribute saved via `saveAttribute` / `saveID`

## Symptom

After the Content-side float-cast fix (`b58eef722`), the project XML module tree still carries scientific-notation attributes:

```xml
Balance="2.081668171172169e-17"
<Processor Type="PolyphonicFilter" ID="Filter1" Bypassed="0" Gain="3.747002708109903e-16"
BipolarIntensity="2.081668171172169e-17"
```

These are NOT the stepSize-grid dust from the previous fix — magnitudes of 1e-16/1e-17 are double-precision rounding residue from HISE's internal math (note `Balance` and `BipolarIntensity` hold the *identical* value, pointing at a shared bipolar/centre conversion producing almost-zero instead of zero). They are computed values, so they reappear regardless of knob activity, and the ScriptSlider export rounding never sees them: the module tree stores raw processor attributes via its own path.

## Root cause

`hi_core/hi_dsp/Processor.h`:

```cpp
#define saveAttribute(name, nameAsString) (v.setProperty(nameAsString, getAttribute(name), nullptr))
#define saveID(name) v.setProperty(#name, getAttribute(name), nullptr);
```

Both write `getAttribute()` verbatim. `FloatSanitizers::sanitizeFloatNumber` can't help — it only flushes NaN/Inf and true denormals (below ~1e-38); 2e-17 is a perfectly normal float.

All three observed attributes flow through `saveAttribute` (`ModulatorSynth.cpp:158-159` for Gain/Balance, `Filters.cpp:317` for BipolarIntensity), so patching the two macros covers every processor.

## Fix

One helper + two macro edits in `Processor.h` — see the companion patch:

```cpp
static inline float flushAttributeDustToZero(float value)
{
	return (value < 1.0e-9f && value > -1.0e-9f) ? 0.0f : value;
}

#define saveAttribute(name, nameAsString) (v.setProperty(nameAsString, hise::flushAttributeDustToZero(getAttribute(name)), nullptr))
#define saveID(name) v.setProperty(#name, hise::flushAttributeDustToZero(getAttribute(name)), nullptr);
```

Threshold rationale: 1e-9 as a gain factor is -180 dB; as a frequency, time, or intensity it is indistinguishable from zero. No HISE parameter has a meaningful non-zero value below it. Load paths are untouched — flushing happens only at serialization, so runtime DSP behaviour is bit-identical (the difference is below audibility by ~140 dB in the worst case).

The comparison is written without `std::abs` so the header gains no include dependency.

## Verification

- Build, load Sublime, save: `Balance`, `BipolarIntensity` (both `2.08e-17`) and `Filter1 Gain` (`3.75e-16`) must serialize as `0.0`.
- Grep the saved project XML for `e-` — zero hits expected in the module tree.
- Confirm no behaviour change: attributes at legitimate small values (e.g. an Intensity of 0.001) must pass through untouched.

## Upstream

Same category as PR #1011 (Content-side float casts) — could ride along as a follow-up. The `Balance`/`BipolarIntensity` identical-value fingerprint (`2.081668171172169e-17`) is the demonstrative example: an internal bipolar conversion at centre produces almost-zero, and the serializer faithfully immortalises it in every saved project.
