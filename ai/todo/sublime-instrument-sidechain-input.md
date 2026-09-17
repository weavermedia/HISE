# Instrument sidechain input for Sublime (external pump source)

**Date:** 2026-08-02 (feasibility research only, no code written)
**Target commit:** `b58eef722` (branch `meatbeats`)
**Status:** TODO. Estimated at roughly a day of focused work including manual DAW testing. The bus/copy code is trivial; the channel-count bookkeeping at export time is the fiddly part.

**Goal:** Exported Sublime instrument plugin exposes a stereo "Sidechain" input bus so a kick track can be routed in and drive an internal pump (envelope follower -> gain/filter modulator), designed and preset-savable inside the instrument.

**Affects:** `PluginParameterAudioProcessor::getHiseBusProperties()` / `isBusesLayoutSupported()` (`hi_core/hi_dsp/plugin_parameter/PluginParameterProcessor.cpp`), `MainController::processBlockCommon` instrument path (`hi_core/hi_core/MainController.cpp`), plus a new config define. Instrument (`USE_FRONTEND`, non-`FRONTEND_IS_PLUGIN`) builds only.

## What exists today (research findings)

- FX plugins already have this exact feature behind `HISE_SIDECHAIN_CHANNEL_LAYOUT` (`PluginParameterProcessor.cpp:610`): main stereo Input + named stereo "Sidechain" input + stereo Output. Instruments never got it - their branch (`:622-647`) declares outputs only, plus a dummy stereo input for Pro Tools / `FORCE_INPUT_CHANNELS`.
- `FORCE_INPUT_CHANNELS` proves the instrument input path works end-to-end: JUCE input buffer -> `multiChannelBuffer` channels 1/2 (`MainController.cpp:1518`) -> root synth chain `internalBuffer` (`ModulatorSynthChain.cpp:323-337`). But it MIXES the input into the audible output (pass-through simulation, see doc at `hi_core.h:247`). Not sidechain.
- For sidechain the external audio must land on a HIDDEN stereo pair (channels 3/4) that never reaches the output. The FX build already documents this "hidden internal stereo pairs" pattern (`hi_core.h:215-232`, `HISE_NUM_FX_PLUGIN_CHANNELS` vs. matrix channel count). Instruments have no equivalent override.
- `multiChannelBuffer` width comes from the master container routing matrix source-channel count (`MainController::updateMultiChannelBuffer`, `MainController.cpp:2454`); destination channels are pinned to `HISE_NUM_PLUGIN_CHANNELS` (`:1819`).
- Global cables work in compiled plugins, so the consumption path (scriptnode send -> modulator receive) needs no C++.

## Plan

### 1. Bus layout (~10 lines)

In the instrument branch of `getHiseBusProperties()` add, behind a new define (suggest `HISE_INSTRUMENT_SIDECHAIN` rather than overloading the FX flag, since the FX flag also REPLACES the multichannel layout):

```cpp
busProp = busProp.withInput("Sidechain", AudioChannelSet::stereo());
```

Keep the Pro Tools dummy-input logic intact. `isBusesLayoutSupported` (`:677-679`) already accepts `inputs == 2 || inputs == 0` for stereo instruments; with a named non-main sidechain bus `getMainInputChannels()` may report 0, so verify and at most add a one-line check on the sidechain bus.

### 2. Input copy (~10 lines)

In the instrument path of `processBlockCommon` (`MainController.cpp:1507-1531`): fetch the sidechain bus with `getBusBuffer(buffer, true, sidechainBusIndex)` and copy it into `multiChannelBuffer` channels 3/4 instead of leaving them cleared. Mirror of the existing `FORCE_INPUT_CHANNELS` block at `:1518`, targeting channels 3/4 (indices 2/3). Guard against the host not connecting the bus (0 channels) - fall back to clearing.

### 3. Channel bookkeeping (the fiddly bit)

The master routing matrix must have 4 source channels (so `multiChannelBuffer` is 4 wide) while the plugin exposes only stereo out. Problem: the exporter auto-derives `HISE_NUM_PLUGIN_CHANNELS` from the matrix, so a 4-channel matrix currently exports a 4-output plugin (two stereo output buses at `:641`). Options:

- ExtraDefinitions override `HISE_NUM_PLUGIN_CHANNELS=2` if the exporter's auto-derivation respects a manual define (verify; possible collision if it writes its own define unconditionally).
- Or add an instrument analog of `HISE_NUM_FX_PLUGIN_CHANNELS` (e.g. `HISE_NUM_INSTRUMENT_PLUGIN_CHANNELS`) used for the output-bus loop only, leaving matrix/`multiChannelBuffer` at 4. Cleaner, mirrors the documented FX pattern.

Also check nothing else assumes `multiChannelBuffer` channels == output channels in instrument builds (routing matrix back-copy after render, standalone path is separate via `HISE_NUM_STANDALONE_OUTPUTS`).

## Sublime project side (no C++)

1. Master container routing matrix: 4 source channels. Sources 1/2 -> dest 1/2; sources 3/4 unrouted (hidden pair).
2. Master FX routed to channels 3/4: scriptnode network with `core.peak` (envelope follower) -> `routing.global_cable` send. Attack/release smoothing in the network defines the pump shape.
3. Receive side: GlobalModulatorContainer child modulator connected via `connectToGlobalCable` (`hi_core/hi_modules/synthesisers/synths/GlobalModulatorContainer.cpp:431`), driving Sublime's gain (and optionally filter cutoff for filter ducking).
4. Product decision: tempo-synced LFO fallback when no sidechain is connected, so the pump feature works out of the box without routing.

## DAW support (instrument sidechain)

- Logic (AU): yes - AU instrument with an aux input bus gets the standard "Side Chain" dropdown in the plugin header.
- Live (VST3/AU): yes since 10.1 - routing chooser in the device's expanded panel.
- Reaper / Bitwig / FL Studio: yes via flexible routing.
- Cubase/Nuendo: NO - VST3 instruments cannot receive audio input (FX only). Long-standing limitation; document it for users.
- Pro Tools (AAX): treat as unsupported until proven otherwise (key inputs are processor-oriented, typically mono).

## Why bother (vs. a DAW-side sidechain compressor)

A compressor after Sublime achieves plain volume pumping with zero work. The built-in route is worth it only for the designed-instrument angle: preset-savable pump amount/shape, filter ducking instead of just gain, tempo-synced fallback, one-knob macro. Decide that before starting.

## Testing

1. Export Sublime AU + VST3 with the flag; verify Logic shows the Side Chain menu and Live shows the routing chooser, and that a routed kick produces the pump.
2. No sidechain connected: plugin must behave exactly as today (silence on channels 3/4, no denormals, fallback LFO if implemented).
3. Verify only ONE stereo output appears in the DAW (the bookkeeping fix worked) and audio output is bit-identical to the current build when the sidechain is silent.
4. Pro Tools dummy-input path still validates (AAX layout check), standalone build unaffected.
5. Host automation/latency unaffected (no latency is added by the copy).
