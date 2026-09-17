# Ladder filter: add feedback saturation, driven by the ignored Gain parameter

**Date:** 2026-07-15 (designed during Sublime Filter Res toggle session)
**Target commit:** `60b38611d` (branch `meatbeats`)
**Status:** IMPLEMENTED 2026-07-15, then REVERTED 2026-07-20 (Dan undecided on the sound for now).

- v1 was applied exactly as proposed below: feedback tanh in `LadderSubType` (`MultiChannelFilters.h`/`.cpp`), gain slider enabled for `LadderFourPoleLP` in `FilterEditor.h`. The display curve already ignored gain, so no change was needed there. Docs item never done (filter docs live in the separate hise_documentation repo).
- The exact applied diff is saved as `ai/todo/ladder-filter-feedback-saturation.patch`. To re-apply: `git apply ai/todo/ladder-filter-feedback-saturation.patch` (verified clean against commit `60b38611d`-era working tree).
- A Debug build WITH the saturation (built 2026-07-15 16:28) is preserved at `projects/standalone/Builds/MacOSX/build/Debug/HISE with Ladder Saturation.app` for A/B against a clean build.
- Verified before reverting: both the Filter module (`Filters.cpp:223`) and scriptnode `filters.ladder` (`FilterNode.cpp:67`) convert Gain from dB to linear before `updateCoefficients`, and both default to 0 dB, so the `drive > 1.004` gate keeps old sessions bit-exact.
**Affects:** `LadderSubType` (`hi_dsp_library/dsp_basics/MultiChannelFilters.cpp:905`) — Filter modules in `LadderFourPoleLP` mode and the scriptnode `filters.ladder` node (same subtype, registered `HiseNodeFactory.cpp:1722`)

## Motivation

HISE's ladder is a clean linear 4×1-pole cascade with global feedback. It has the ladder's slope and the classic resonance bass-suck, but none of the analog ladder's saturation character — and two loose ends in the source suggest it was left half-finished:

- The feedback variable is literally named `resoclip` (`MultiChannelFilters.cpp:909`) but is never clipped.
- `updateCoefficients` receives `gain` and ignores it (`double /*gain*/`, `:895`), even though the `MultiChannelFilter` wrapper already smooths a gain value (`.h:182`) and passes it down every block.

Clipping the feedback is where the analog ladder's musical behaviour lives: the resonant peak compresses instead of growing linearly, odd harmonics appear around the cutoff, resonance stops eating the passband so aggressively (feedback can't exceed ±1), and self-oscillation stays bounded. For bass instruments (Sublime runs this filter at Q up to 4) it's the difference between "digital resonance" and "vintage filter".

⚠️ **License note:** Surge's very nice ladder implementations (`sst-filters`) are **GPL3** — do not port or crib from them. Implement from the published papers: [Huovilainen, DAFx 2004](https://www.dafx.de/paper-archive/2004/P_061.PDF) and [D'Angelo/Välimäki, "An improved virtual analog model of the Moog ladder filter" (ICASSP 2013)](https://www.researchgate.net/publication/261193653). Algorithms aren't copyrightable; GPL source is.

## Current code

```cpp
// MultiChannelFilters.cpp:905
float LadderSubType::processSample(float input, int channel)
{
	float* buffer = buf[channel];
	float resoclip = buffer[3];              // named "clip", never clips
	const float in = input - (resoclip * res);
	buffer[0] = ((in - buffer[0]) * cut) + buffer[0];
	buffer[1] = ((buffer[0] - buffer[1]) * cut) + buffer[1];
	buffer[2] = ((buffer[1] - buffer[2]) * cut) + buffer[2];
	buffer[3] = ((buffer[2] - buffer[3]) * cut) + buffer[3];
	return 2.0f * buffer[3];
}

// MultiChannelFilters.cpp:895
void LadderSubType::updateCoefficients(double sampleRate, double frequency, double q, double /*gain*/)
```

Gain plumbing that already exists: `MultiChannelFilter::setGain` (`:120`) → smoothed `LinearSmoothedValue<double> gain = 1.0` (linear factor, `.h:182`) → passed per-block into `updateCoefficients`. So a drive control needs **zero new parameter plumbing** — every Filter module's Gain attribute and the scriptnode node's Gain parameter reach the subtype already.

## Proposed fix (v1 — feedback-only, gated, bit-exact by default)

```diff
 // MultiChannelFilters.h, LadderSubType private section
 	float cut;
 	float res;
+	float drive = 1.0f;

 // MultiChannelFilters.cpp, LadderSubType::updateCoefficients
 	cut = jlimit<float>(0.0f, 0.8f, x);
 	res = jlimit<float>(0.3f, 4.0f, (float)q / 2.0f);
+	drive = jmax(1.0f, (float)gain);   // gain is a linear factor, 1.0 = 0 dB

 // MultiChannelFilters.cpp, LadderSubType::processSample
-	float resoclip = buffer[3];
+	float resoclip = buffer[3];
+
+	// Saturate the feedback path only when driven past 0 dB (opt-in:
+	// bit-exact with the legacy linear ladder at the default Gain).
+	// tanh(x*d)/d has slope 1 at the origin, so the res calibration and
+	// small-signal behaviour are unchanged — drive only moves the clip knee.
+	if (drive > 1.004f)
+		resoclip = std::tanh(resoclip * drive) / drive;
```

(and un-ignore the parameter: `double gain` instead of `double /*gain*/`.)

Design choices, deliberate:

- **Feedback-only tanh** (not input drive, not per-stage): silent at low res, grows with resonance, cheapest possible (one tanh per sample only when driven). Per-stage tanh is 4× the cost for a subtler difference — that's the "full model" follow-up below.
- **Gated on `drive > 1.004`** (≈ +0.03 dB): every existing project has Gain at 0 dB for ladder modes (the editor greys the slider out), so old sessions null against the patched build bit-for-bit.
- **Slope-normalised** (`/ drive`): resonance calibration doesn't shift as drive rises; only the saturation onset does.

## Also needs touching

- **`FilterEditor.h:74` region** — the per-mode `qSlider`/`gainSlider` enable switch: enable the gain slider for `LadderFourPoleLP` (it currently reads as "Gain does nothing", which is accurate today).
- **Display curve** — `FilterHelpers.cpp:317` approximates the ladder with an IIR lowpass for the graph; keep gain out of that (drive shouldn't move the drawn curve).
- **Docs** — the Filter module / `filters.ladder` docs should state Gain = drive for ladder modes.

## Aliasing (accepted in v1)

The tanh generates harmonics that alias; serious models oversample 2×. Skipped deliberately: for bass material the generated harmonics sit far below Nyquist, and the gate means clean patches are untouched. If high-cutoff/high-drive sweeps sound gritty in practice, the fix is per-stage saturation + 2× oversampling per the Huovilainen/D'Angelo papers — a much bigger patch, possibly as a *new* `FilterMode` appended before `numFilterModes` (never insert mid-enum: projects store Mode as a raw number).

## Testing

1. Null test old vs new build, ladder LP, Gain 0 dB, res sweep — must be bit-identical.
2. Drive sweep (0 → +18 dB) at Q 4–8, notes up the keyboard: listen for character vs aliasing grit. Reference: Surge XT "Vintage Ladder" vs "Legacy Ladder" A/B.
3. Self-oscillation behaviour at max res with/without drive.
4. CPU: one conditional tanh per sample when driven; confirm negligible.

## Related loose ends found in the same review (separate fixes, note here so they aren't lost)

- **`LadderFourPoleHP` is a dead mode:** exposed as a scripting constant (`ScriptingApiObjects.cpp:3796`) and has display coefficients (`FilterHelpers.cpp:318`), but `FilterBank::setMode`'s switch has no case for it (`FilterHelpers.cpp:111` ends at LP → `default: break`), so setting it from script silently keeps the previous filter type. The editor combo doesn't offer it (`FilterEditor.cpp:81` adds LP only). Either implement the HP ladder or remove the constant.
- **`FilterLimits::limitGain` units mismatch:** clamps to `[-18, 18]` (`.h:51`, values that read as dB) but is applied to the *linear* gain factor in `setGain` (`.cpp:122`, default 1.0). Harmless today because nothing uses gain in non-EQ modes; worth fixing if gain becomes drive (a linear cap of 18 ≈ +25 dB is fine in practice, but a negative *linear factor* is nonsense).
- **Most subtypes ignore gain** (`double /*gain*/` throughout): by design for non-shelf modes, but it's why the Gain knob "does nothing" on LP/HP/ladder — the editor's greyed slider is the only hint.
