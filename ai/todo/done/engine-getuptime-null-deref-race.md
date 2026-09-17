# Engine.getUptime(): null-deref race on the scripting thread

**Date:** 2026-09-07 (from a HISE Debug 4.9.4 crash report, Sublime open ~51 minutes)
**Target commit:** `8886cb5f8` (branch `meatbeats`)
**Status:** Landed in meatbeats `9fa518b62` (2026-09-17); upstreamed as [christophhart/HISE#1045](https://github.com/christophhart/HISE/pull/1045). Applied as proposed below minus the explanatory comment (dropped: nothing contradictory, no comments at the neighbouring call sites). Verified with a temporary spin-wait between the two loads: unfixed crashed 43 s after launch on Logic MIDI, fixed ran 5 minutes clean. Companion .patch alongside carries the original commented form.
**Affects:** `ScriptingApi::Engine::getUptime()`; every build target (IDE, exported plugin, DLL)
**Upstream:** present unchanged on `develop` (= `christophhart/HISE`), traced to `245690eea` (Nov 2020). Single-fix upstream PR candidate.

## Symptom

`EXC_BAD_ACCESS (SIGSEGV)`, `KERN_INVALID_ADDRESS at 0x000000000000000c`, crashed thread
"Javascript Thread":

```
0  hise::HiseEvent::getTimeStamp() const + 20      (HiseEventBuffer.cpp:271)
1  hise::ScriptingApi::Engine::getUptime() const + 92   (ScriptingApi.cpp:1697)
2  hise::ScriptingApi::Engine::Wrapper::getUptime(hise::ApiClass*)
3  hise::ApiClass::callFunction(int, juce::var*, int)
4  ...RootObject::ApiCall::getResult(...)
5  ...RootObject::VarStatement::perform(...)
...
14 hise::WeakCallbackHolder::operator()(hise::JavascriptProcessor*)
22 hise::JavascriptThreadPool::Task::callWithResult()
23 hise::SuspendHelpers::Suspended<..., hise::SuspendHelpers::FreeTicket>::call()
25 hise::JavascriptThreadPool::run()
```

`this` is null inside `getTimeStamp()`: `x0 = 0`, `far = 0xc`, and `timestamp` is at
offset 12 in `HiseEvent` (`hi_tools/hi_tools/HiseEventBuffer.h:475`, after
type/channel/number/value, transpose/gain/semitones/cents, eventId/startOffset).

Thread 6 (`com.apple.audio.IOThread.client`) was inside `processBlockCommon` at the moment
of the crash.

## Cause

`ScriptingApi.cpp:1695-1697` calls the accessor twice:

```cpp
if (parentMidiProcessor != nullptr && parentMidiProcessor->getCurrentHiseEvent() != nullptr)
{
    return parentMidiProcessor->getMainController()->getUptime() + parentMidiProcessor->getCurrentHiseEvent()->getTimeStamp() / getSampleRate();
}
```

`ScriptBaseMidiProcessor::currentEvent` (`hi_scripting/scripting/ScriptProcessor.h:258`) is
a plain non-atomic `HiseEvent*`. For a non-deferred `JavascriptMidiProcessor` it is written
and cleared by the **audio thread** via `ScopedValueSetter` in `processHiseEvent`
(`hi_scripting/scripting/ScriptProcessorModules.cpp:227`).

The crashing callback ran on the **scripting thread** as
`JavascriptThreadPool::Task::HiPriorityCallbackExecution` under
`Suspended<..., FreeTicket>`, i.e. without suspending audio. So the two loads straddle a
concurrent write: the first sees a live event, the audio thread's `ScopedValueSetter`
restores the member to `nullptr`, the second returns null, and the dereference faults.

Textbook TOCTOU. Every other `getCurrentHiseEvent()` call site in `ScriptingApi.cpp`
(5515, 5626, 5649, 5688, 5781, 5874, 6385, 6442, 6506) already loads once into a local;
`getUptime()` is the only outlier.

## Trigger in Sublime

`/Users/dan/Code/hise-sublime/Scripts/Overs.js:34`, the clip-ring timer, `include`d by
`Interface.js:51`:

```js
const var timer = Engine.createTimerObject();
timer.setTimerCallback(function()
{
    var now = Engine.getUptime();   // line 34
    ...
});
timer.startTimer(30);
```

It is the only `Engine.getUptime()` call in the project (`Interface.js:377` is a comment).
`TimerObject` is a plain `juce::Timer` on the message thread
(`ScriptingApiObjects.h`, `InternalTimer`) whose `timerCallback`
(`ScriptingApiObjects.cpp:5513`) fires a `WeakCallbackHolder`, which dispatches to the
scripting thread pool (`ScriptingBaseObjects.cpp:802`, `:832`). Frame 5 of the trace is a
`VarStatement`, i.e. literally `var now = Engine.getUptime();`.

At `startTimer(30)` that is ~33 attempts a second at a few-instruction window, only while
the audio thread happens to be inside `processHiseEvent`. Hence "crashes on its own after
a while" - the report was ~51 minutes in, roughly 100k attempts.

## Not backend-only

Nothing on the path is guarded:

| Path element | Guard |
|---|---|
| `Engine::getUptime()` (`ScriptingApi.cpp:1693`) | none |
| `ScriptBaseMidiProcessor::currentEvent` (`ScriptProcessor.h:258`) | none (outside the `#if USE_BACKEND` block at 117-127) |
| `JavascriptThreadPool` (`ScriptProcessor.h:807`) | none; owned unconditionally by `MainController` (`MainController.h:2281`) |
| `TimerObject` + `WeakCallbackHolder::call` (`ScriptingBaseObjects.cpp:802`) | none |

So exported Sublime plugins carry it too, and arguably hit it harder: DAW sessions run for
hours and real playing widens the aggregate window.

Caveat on symptom, not on cause: the crash report is a Debug build. In Release,
`getCurrentHiseEvent()` is a one-line header accessor and the optimiser may fold the two
loads into one, masking the bug. A Release plugin that has never crashed is not evidence
the bug is absent.

## Proposed change

`hi_scripting/scripting/api/ScriptingApi.cpp` (~1693), see the `.patch` alongside.

Load the pointer once into a local and dereference that, matching the idiom used by every
other call site in the file:

```cpp
if (parentMidiProcessor != nullptr)
{
    if (auto currentEvent = parentMidiProcessor->getCurrentHiseEvent())
        return parentMidiProcessor->getMainController()->getUptime() + currentEvent->getTimeStamp() / getSampleRate();
}

return getProcessor()->getMainController()->getUptime();
```

One file, one hunk, no behaviour change on the non-racing path.

## Deliberately out of scope

The single load removes the crash but not the whole race. `currentEvent` points at a
`HiseEvent` living on the **audio thread's stack** (`ScopedValueSetter<HiseEvent*> svs(currentEvent, &m)`),
so a stale non-null read still yields a garbage timestamp - a wrong uptime rather than a
segfault, and the stack page stays mapped, so it will not fault.

Closing that properly means making `currentEvent` a `std::atomic<HiseEvent*>` (or routing
event access through a copy the way `DeferredExecutioner` already does at
`ScriptProcessorModules.cpp:148`). That touches the audio-thread hot path and several
modules, so it is not part of this fix.

Related unchecked dereferences noticed while reading, not investigated:
`hi_scripting/scripting/HardcodedScriptProcessor.h:394` and
`hi_scripting/scripting/hardcoded_modules/Arpeggiator.cpp:579,832` all do
`*getCurrentHiseEvent()` with no null check. Presumed synchronous-path-only.

## Project-side mitigation (only if running an unpatched binary)

`Overs.js` does not need transport-accurate time; the timer object's own
`getMilliSecondsSinceCounterReset()` would avoid `Engine.getUptime()` entirely. Not needed
once HISE is rebuilt with the patch, so leave the script alone.

## Verification

- Not built. Cause established from the crash report plus source reading only.
- After applying: rebuild Debug standalone, open Sublime, leave it running with MIDI input
  for an hour or so and confirm no repeat. Note that absence of a crash is weak evidence
  for a race this narrow - the source-level argument is the stronger one.
