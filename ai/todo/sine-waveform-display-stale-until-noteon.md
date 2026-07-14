# Sine synth Waveform display stale until note-on when saturation chain has active mods

**Date:** 2026-07-14 (noticed in Sublime, Shape knob)
**Target commit:** `29dc7eb00` (branch `meatbeats`, "Make Saturation param of Sinewave Generator modulatable")
**Status:** Applied 2026-07-14 (pending build + manual verification)
**Affects:** any `SineSynth` whose Saturation Modulation chain contains an unbypassed modulator

## Symptom

With a modulator sitting unbypassed in the Sine synth's Saturation Modulation chain, changing `SaturationAmount` (UI knob / `setAttribute`) while **no voice is rendering** does not update the Waveform display — the redraw fires but re-draws the old shape. The display only catches up on the next note-on. Audio is unaffected; the attribute is applied correctly.

Surfaced in Sublime because its velocity-routing convention keeps the `VelSourceShape` modulator permanently unbypassed (toggling Intensity 0/1 instead of Bypassed, Sublime commit `007a14c`), so the chain always counts as active and every silent Shape-knob turn hits this.

## Root cause

The modulatable-Saturation feature (`29dc7eb00`) tracks the render-time modulated value for the display:

- `SineSynth::getSaturatedTableValues()` (`hi_core/hi_modules/synthesisers/synths/SineSynth.cpp` ~126):

  ```cpp
  if (mb.getChain()->shouldBeProcessedAtAll())
      currentSaturation = lastSaturationModValue;   // last value tracked during rendering
  else
      currentSaturation = saturationAmount;
  ```

- `lastSaturationModValue` is only refreshed in `handlePeakDisplay()` (`SineSynth.cpp` ~261), i.e. **during voice rendering** (every ~1323 samples from the chain's output value).
- `setInternalAttribute(SaturationAmount)` (`SineSynth.h` ~200) updates `saturationAmount`, resets the chain's initial value, and calls `triggerWaveformUpdate()` — but never touches `lastSaturationModValue`. The triggered redraw therefore repaints the stale value.
- `ModulatorChain::shouldBeProcessedAtAll()` = `!isBypassed() && handler.hasActiveMods()` (`ModulatorChain.cpp:1350`) — an unbypassed modulator keeps the chain "active" even at Intensity 0 and with no voice playing.

## Fix

One line in `SineSynth.h` `setInternalAttribute`, `case SaturationAmount:` (~line 200):

```diff
 case SaturationAmount:		saturationAmount = newValue;
 							saturator.setSaturationAmount(newValue);
 							if (saturationChain != nullptr)
 							{
 								saturationChain->setInitialValue(newValue);
 								// Force update of constant value when parameter changes
 								modChains[ChainIndex::SaturationChain].clear();
 							}
+							lastSaturationModValue = newValue;
 							triggerWaveformUpdate();
 							return; // skip the calculation of the pitch ratio
```

Matches the constructor's existing convention (`lastSaturationModValue = saturationAmount`). While a note sustains, `handlePeakDisplay()` keeps overwriting it with the real modulated chain output every ~30 ms, so the velocity-modulated display behaviour is unchanged — this only fixes the idle case.

### Follow-up (same day)

The one-liner alone was not enough: `handlePeakDisplay()` is called unconditionally from `ModulatorSynth::renderNextBlockWithModulators()` every audio block (`ModulatorSynth.cpp` ~684), even with zero active voices — the root-cause note above ("only refreshed during voice rendering") was wrong. With no voices the chain's `getOutputValue()` is 0, so every ~30 ms the tracker overwrote the knob value and the display flickered saturated/plain-sine, then settled on plain sine.

Second fix in `SineSynth::handlePeakDisplay()` (`SineSynth.cpp` ~250): add `&& getNumActiveVoices() > 0` to the tracking condition, so the chain output is only mirrored into `lastSaturationModValue` while voices are actually rendering.

## Verification

1. Load Sublime, no note held: turning Shape should morph the `fltSourceShapeGraph` waveform sine→square live.
2. Hold a note with velocity→Shape ON: display still follows the *modulated* saturation during the note (unchanged).
3. Regression: velocity→Shape OFF + note held; preset switches; empty saturation chain (stock Sine synth) still live.

## Upstreaming

The whole modulatable-Saturation feature is fork-only, so this rides along with any future upstream PR of `29dc7eb00` rather than needing its own forum post.
