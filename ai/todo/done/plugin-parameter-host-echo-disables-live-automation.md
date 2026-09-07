# Plugin parameters echo non-user changes to the host; Ableton Live disables the automation lane

**Date:** 2026-09-07 (found during the Sublime 1.0 pre-freeze host round)
**Target commit:** `8886cb5f8` (branch `meatbeats`)
**Status:** DONE. Committed on `meatbeats` as `00d858939` (two layers, 3 files, +49 lines) and `16384a0ae` (memo refresh after a user send, +5 lines), both 2026-09-07. Upstreamed as PR #1037 (https://github.com/christophhart/HISE/pull/1037, branch `fix/plugin-parameter-host-echo`, cherry-picks of both onto upstream develop `5d34685fe`). Revision 2 VERIFIED in Live 12 on 2026-09-07 15:30 (Sublime 0.0.99 test export, HISE Debug built 15:18): 5/5 project opens all lanes red; 5/5 editor open+close during playback and 5/5 stopped, all lanes red; Automation Arm + record writes points on a knob drag. Revision 1 (flags only) had been built and tested at 15:00: fixed editor open and the randomness, but left 4 of 6 lanes greyed on every project open and random lanes greyed on editor CLOSE during playback; the memo (layer 2) closed both. FOLLOW-UP (same day, found by source reading, confirmed in Live): `lastHostValue` was only written by the host-facing setValue() overrides, which the recursive flag blocks during handleAsyncUpdate(), and both JUCE wrappers (VST3 setValueAndNotifyIfChanged, AU SetParameter) skip a host write equal to the current value, so a user edit never refreshed the memo and a later user change back to the last host-written value was silently dropped. Repro: Filter Resonance button, host-side OFF then ON, then plugin OFF then ON - host display stayed OFF. A project reopen does NOT plant the stale value (state restore never calls setValue()); only a host write that changed the value does (automation playback, host panel edit, host MIDI mapping). Fixed in `16384a0ae` by setting lastHostValue = parameterValueToSend right after the send, re-verified both tests in Live 12. The parked .patch was dropped in favour of the PR branch. Other hosts not re-checked after revision 2 (they never showed the symptom).
**Affects:** every exported plugin with `isPluginParameter` controls, VST3 and AU. Only Ableton Live shows a symptom; Cubase, FL Studio, Reaper, Logic, Bitwig and Maschine ignore the echo.
**Upstream:** present unchanged on `develop`. Same lineage as GitHub issue 749 (June 2025, Christoph: "the automation coming in from Cubase is written back to Cubase as if it would be moved by hand") and forum topic 15004 (August 2026, one-knob repro, no scripts). Single-fix upstream PR candidate.

## Symptom

In Live 12, with automation drawn on any Sublime parameter:

- **Project open**: a random subset of the automated lanes comes back grey (overridden), the
  "Re-Enable Automation" button lights, and the greyed parameters sit at values that are not
  the automation value at the playhead. The subset differs on every open. Three screenshots
  of the same six-lane project on three consecutive opens showed 1, 2 and 3 lanes surviving.
- **Editor open or close**: the same thing, every time, during playback or stopped.
- Playback itself is fine: lanes stay red and knobs follow (the issue 749 patch is in).

Reproduced by a forum user with a fresh HISE project, one knob, `isPluginParameter` on, no
scripts (topic 15004): "the DAW behaves exactly as if a parameter was manually adjusted by
the user". Failure rate scales with parameter count (1/30 opens with one knob, 1/3 with 60).

## Cause

`HisePluginParameterBase::onUpdate()` (`MacroControlledComponents.h`) is an attribute
listener: any change to the controlled attribute, from any origin, ends in
`refreshParameterValue()` -> `setValueNotifyingHost()` -> the wrapper's `performEdit()`.
The only origin that is filtered is the host itself (`sendToHost = false` inside
`ScriptedControlAudioParameter::setValue()`, the issue 749 fix). Three other origins are
not user edits and still notify:

1. `FrontendProcessor::setStateInformation()` restoring every control (project open).
2. `FrontendProcessorEditor` construction, when the wrappers sync the components.
3. The internal preset load that runs inside 1.

Live treats a plugin-originated `performEdit` on an automated parameter as a manual
takeover and disables the lane. Nothing else does.

The randomness: `refreshParameterValue()` defers to `triggerAsyncUpdate()` whenever the
change is off the message thread or `deferNotifyHostFlag` is set, while Live is applying
its automation values on the audio thread at the same time. Per parameter, whichever write
lands last wins, and the deferred send carries whatever `parameterValueToSend` holds by
then (the odd values on the greyed lanes).

## Why not the value guard from topic 15004

The forum's proposed fix adds `if (getValue() == newValue) return;` to JUCE's
`AudioProcessorParameter::setValueNotifyingHost()`. It silences the echo because HISE
always stores the attribute BEFORE notifying, so the values are already equal. But that
ordering also holds for the user's own drags: `HiSlider::sliderValueChanged()` calls
`setAttribute()`, the listener fires, and only then does HISE call
`setValueNotifyingHost()`, with the attribute already equal. With the guard the host is
never told about a user move at all, so automation WRITING and the host's parameter
display break. (Source reading, not run. Verify before dismissing: apply it, arm a lane in
Live, drag a knob during playback, expect no points to be written.)

## Fix (this patch, two layers)

**Layer 1 - gate on origin.** New `HisePluginParameterBase::shouldSkipHostNotification()`
returns true while any of three flags that already exist is set:

- `KillStateHandler::getStateLoadFlag()` (raised around `setStateInformation()`)
- `MainController::getInterfaceCreationFlag()` (raised in the editor constructor)
- `UserPresetHandler::isInternalPresetLoad()` (the scope entered inside the state restore;
  it is NOT entered by a user's browser preset load, so those still notify)

Checked in `refreshParameterValue()` before the send/defer decision, and at the top of
`handleAsyncUpdate()`.

**Layer 2 - never echo the host's own value.** Revision 1 alone left two leaks, both the
same shape: a notification that was QUEUED while a guard was up and FIRED after it dropped,
carrying a value the host itself had supplied.

- Editor close during playback: `~ScriptContentComponent` pauses the dispatcher
  (`SUSPEND_GLOBAL_DISPATCH`) around `componentWrappers.clear()`. The dispatch queue keeps
  events while paused and flushes them on resume (`Queue.cpp`, `ENABLE_DISPATCH_QUEUE_RESUME`),
  so host automation writes made during teardown have their `onUpdate()` fire after the
  `sendToHost = false` scope in `setValue()` has ended. Random lanes = whichever parameters
  Live wrote during the pause. Stopped transport = no host writes = no symptom (observed).
- Project open, deterministic 4 of 6: async updates deferred before or during the load
  fire after it, and by then `parameterValueToSend` holds the value the host restored.

So every host-facing `setValue()` override (`ScriptedControlAudioParameter`, the custom
automation parameter and the macro parameter in `PluginParameterProcessor.cpp`) now records
`lastHostValue` via `setLastHostValue()`, converted with the SAME arithmetic `onUpdate()`
uses (`convertTo0to1(snapped)`), so stepped and skewed ranges compare exactly. `onUpdate()`
still tracks `parameterValueToSend` but returns before sending when the value equals
`lastHostValue`; `handleAsyncUpdate()` does the same. A user drag that lands exactly on the
host's last value is not reported, which is harmless: the host already has it.

User drags to any other value, script `changed()` calls, MIDI learn and macro paths are
untouched. The host loses nothing: JUCE's VST3 wrapper already calls
`restartComponent(kParamValuesChanged)` after a state restore, and the AU wrapper posts the
equivalent property change.

## Open item

If revision 2 still greys lanes on project open, the remaining source is a notification
carrying a NON-host value (plugin init, default preset load, script `onInit` writes) that
fires after Live's synchronous state restore. The fix for that is a load generation counter:
bump it in `setStateInformation()`, capture it when deferring, and drop a deferred update
whose generation is stale. Not needed if the memo covers it - test first.

## Verification plan

1. Build HISE (`meatbeats`), export Sublime `test`, install.
2. Live 12: the six-lane project (Source Shape, Source Punch, LFO Fade, Output Dirt, Filter
   Cutoff, LFO Depth, all ramped over one bar). Open the project five times: every lane
   must be red every time.
3. Open and close the editor ten times while playing and while stopped: lanes stay red.
4. Regression: arm a lane, drag a knob during playback, points are written. Load a preset
   from the browser while a lane is armed in Read mode: the host display follows.
5. Cubase, Reaper, FL, Logic: unchanged (they never showed the symptom).

## Related

- GitHub issue 749: `ScriptedControlAudioParameter::setValue()` write-back, fixed by the
  `sendToHost` scope. Same bug class, host-to-plugin direction only.
- Forum topic 15004: one-knob repro of the editor-open case, value-guard proposal.
- Fork commit `633626eeb` (2025-10-21) added the `getPluginParameterUpdateState()` gate
  for MIDI-controlled knobs: the same idea, one origin at a time. This patch adds the
  remaining three.
