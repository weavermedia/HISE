# AI Experiments

A running log of design / refactor explorations that were investigated but not (or not yet) merged. The intent is to record the question, what was learned, and why the experiment was abandoned, so the same ground isn't re-walked later.

---

## Adding a Gain Modulation chain to ShapeFX

**Date:** 2026-05-29

### Goal

Make ShapeFX's `Gain` parameter modulatable, specifically by a Flex AHDSR envelope, so that the input drive into the waveshaper could be shaped over the lifetime of a note (e.g. distortion that fades in with the envelope).

### What was sketched

A patch modelled on `SaturatorEffect`, which is the closest precedent (another `MasterEffectProcessor` that exposes a modulatable parameter):

1. Add `InternalChains { GainModulation = 0, numInternalChains }` and matching `EditorStates` to `ShapeFX.h`.
2. Override `getNumInternalChains` / `getNumChildProcessors` / `getChildProcessor` to expose a `gainChain` member.
3. Add `.withModulation(Mod(GainModulation)...withMode(ScaleOnly))` to `ShapeFX::createMetadata()`.
4. In the constructor, `modChains += { this, "Gain Modulation" }` before `finaliseModChains()`, then `gainChain = modChains[...].getChain()`, set `setExpandToAudioRate(true)` and `setAllowModificationOfVoiceValues(true)`, and assign `setFactoryType(new TimeVariantModulatorFactoryType(Modulation::GainMode, this))`.
5. In `applyEffect`, branch on `modChains[GainModulation].getReadPointerForVoiceValues(...)` for an audio-rate per-sample path, falling back to the existing `gainer` smoother scaled by `getConstantModulationValue()` when nothing is connected.

### Why we did not proceed

The proposed mod chain uses `TimeVariantModulatorFactoryType`, which is the only factory available to a `MasterEffectProcessor`. That factory whitelists:

- LfoModulator
- ControlModulator (MIDI CC)
- PitchwheelModulator
- MacroModulator
- GlobalTimeVariantModulator
- JavascriptTimeVariantModulator
- HardcodedTimeVariantModulator

Envelopes (including `AhdsrEnvelope` and `FlexAhdsrEnvelope`) live in `EnvelopeModulatorFactoryType`, a separate category that is not selectable on a master effect.

This is not an arbitrary filter. It reflects a structural constraint: `ShapeFX` is a `MasterEffectProcessor`, so regardless of whether it is placed in the top-level master FX chain, a container FX chain, or a synth's own FX chain, it always processes a **post-voice-summing** stereo buffer. There is no per-voice context at that stage. Envelopes are inherently per-voice (they need to know which note triggered them and when), so they cannot drive a parameter on a master effect.

Since the entire motivation for adding the mod chain was envelope-driven gain, the proposed change would not have delivered the desired behaviour. The non-envelope modulators that would become available (LFO, CC, macro, pitch wheel) were not the use case.

### What to use instead

`PolyshapeFX` is the polyphonic sibling: a `VoiceEffectProcessor` that already exposes a `DriveModulation` chain accepting envelopes (including Flex AHDSR), because it sits in the per-voice DSP path before the voice mixer.

Tradeoffs vs ShapeFX:

- PolyshapeFX has a much smaller parameter set (Drive, Mode, Oversampling, Bias). It lacks ShapeFX's HP/LP pre-filters, bit reduction, autogain, input limiter, wet/dry mix, separate L/R bias, multi-stage oversampling, and JS scripting modes.
- PolyshapeFX has a different set of shape modes: no Tanh/Square/SquareRoot/Saturate; gains Chebichev1-3 and AsymetricalCurve.

For "distortion that follows an envelope" without those extras, PolyshapeFX driven by a Flex AHDSR on its DriveModulation chain is the correct tool.

### What would unblock the original request

The only path to envelope-modulatable gain on ShapeFX-as-it-stands would be to convert ShapeFX into a voice effect (or fork a polyphonic variant of the full feature set). That is a substantial refactor: per-voice state, per-voice oversamplers, significantly higher CPU, and a different position in the signal chain. Given that PolyshapeFX exists specifically to cover the voice-effect use case, this is almost certainly not worth pursuing.

### Side effect of the investigation

While diagnosing this, a separate bug in `ShapeFXEditor.cpp` was found and fixed: the Function dropdown was showing "(no choices)" because `ProcessorMetadata::setup(HiComboBox&, ...)` clears and re-populates the combo from `pd->vtc.itemList`, which is empty for the `Mode` parameter (no `.withValueList(...)` declared). The fix swaps `md.setup(*modeSelector, ...)` for the direct `modeSelector->setup(getProcessor(), Mode, "Mode")`, matching the working pattern in `PolyShapeFXEditor.cpp`. This is independent of the abandoned mod-chain work and should ship.
