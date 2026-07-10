# Velocity toggle in the component right-click (MIDI learn) popup

**Date:** 2026-07-08
**Target commit:** `86680e63d`
**Status:** NOT PROCEEDING – superseded the same day it was written. Kept as an experiment record.

## Why we did not proceed (read this first)

While anchoring this proposal to source, we found the feature **already exists
natively for Matrix Modulation targets** – no HISE patch needed:

- Any component registered as a matrix target gets a "Modulation for <targetId>"
  section in the right-click popup, with an **Assign** submenu listing every
  `GlobalModulatorContainer` source and per-source **Remove** items
  (`GlobalContainerMatrixModulationPopupData`,
  `hi_core/hi_modules/synthesisers/synths/GlobalModulatorContainer.cpp:251`,
  attached at `hi_scripting/scripting/api/ScriptComponentWrappers.cpp:474`).
- `matrix.setEditCallback(itemNames, fn)` injects **custom script-defined items**
  into that popup; the callback receives `(itemIndex, targetId)`
  (`hi_scripting/scripting/api/ScriptModulationMatrix.cpp:825`).
- A velocity source in the container makes "Assign → Velocity" appear on every
  target knob, with native ring display data via `getModulationDisplayData`.

**What to use instead:** put a velocity modulator in a `GlobalModulatorContainer`
and make the velocity-capable knobs matrix targets (`Engine.createModulationMatrix()`).
This is also the architecture the planned Plus-version mod matrix uses, so the
free-version velocity feature becomes a subset of it rather than a parallel
system. The patch below would only ever make sense for a project that cannot
adopt the matrix and must keep plain per-chain velocity modulators – and even
then, the likely upstream response is "use the matrix".

The original proposal follows, unchanged, in case the non-matrix case ever
becomes real.

---

## Motivation

A compiled plugin often wants a per-knob "velocity controls this parameter" toggle
(velocity → volume, velocity → filter freq, …) without spending panel space on a
dedicated button per knob. The natural home is the right-click popup that users
already open to MIDI-learn a control. The desired UX:

- Right-click a knob → a **"Velocity Control"** item (ticked when active) above
  the Learn/Assign CC items.
- Toggling it enables/disables a velocity modulator that the project author has
  prepared for that control.
- Visible state feedback (e.g. a modulation ring) stays project-side.

Concrete driving case: the Sublime project currently ships zero-visual overlay
`ScriptButton`s wired to velocity modulators' `Bypass` parameters – clutter that
duplicates what the popup + a painted ring could do.

## Current state (source anchors)

`MacroControlledObject::enableMidiLearnWithPopup()`
(`hi_core/hi_core/MacroControlledComponents.cpp:103`) builds a **hardcoded**
`PopupMenu`:

- Learn / Assign CC (`Commands` enum at `:123`)
- MPE gesture add/remove (`:203`–`:217`)
- Macro connect/remove
- One extension point: `modulationData->addToPopupMenu(this, m)` (`:282`) –
  only populated for **matrix modulation targets** (see Alternative below).

There is no script hook for a non-matrix component, so a plain "velocity
modulator on a gain chain" architecture cannot surface a popup item today.

## Precedent: the MPE gesture item

The MPE item is exactly the requested pattern with a different source:

- Convention-named modulator lookup: `dynamic_cast<Component*>(this)->getName() + "MPE"`
  resolved via `ProcessorHelpers::getFirstProcessorWithName(...)` (`:203`–`:205`).
- Menu item appears only when the modulator exists; label switches on connection
  state (`:209`–`:216`).
- Result handling at `:314`–`:318` (`data.addConnection` / `removeConnection`).

## Proposed change

Mirror the MPE pattern for a convention-named velocity modulator
`<ComponentID>Velocity` (any `Modulator`, matched by id anywhere in the synth
chain), toggling its `Bypass` state.

```cpp
// in enum Commands (MacroControlledComponents.cpp:123) – add after RemoveMPE:
    ToggleVelocity,

// after the MPE block (around :218):
const String velName = dynamic_cast<Component*>(this)->getName() + "Velocity";

auto velMod = dynamic_cast<Modulator*>(ProcessorHelpers::getFirstProcessorWithName(
    getProcessor()->getMainController()->getMainSynthChain(), velName));

if (velMod != nullptr)
    m.addItem(ToggleVelocity, "Velocity Control", true, !velMod->isBypassed());

// in the result handler (near :314):
else if (result == ToggleVelocity)
{
    velMod->setBypassed(!velMod->isBypassed(), sendNotification);
}
```

Project-side integration (no HISE change needed for these):

- **Ring/indicator:** script attaches a broadcaster to the modulator's bypass
  state (`Broadcaster.attachToProcessorBypass`) and repaints the knob's LAF ring.
- **User-preset persistence:** bypass state of a modulator is module-tree state,
  not component state – a project that persists velocity toggles in user presets
  should mirror the bypass into a hidden saved component (or a
  `UserPresetHandler` custom value) from that same broadcaster.

## Open design questions (for Christoph)

1. **Convention naming vs explicit registration.** `<ComponentID>Velocity`
   matches the MPE precedent but bakes a second magic suffix into component ids.
   Alternative: a script property (e.g. `slider.set("velocityModulatorId", "VelLevel")`)
   – more explicit, slightly larger patch (new component property + wrapper
   plumbing).
2. **Naming**: the item toggles an arbitrary modulator's bypass, not literally
   "velocity" – a generic name like `<ComponentID>Toggle` with the modulator's
   own name as the label would generalize (aftertouch, mod-wheel, …) at the cost
   of a fuzzier contract.
3. **Persistence**: should the toggle write into user-preset state natively
   (like MPE connections persist via `MPEData`) instead of leaving it to the
   project script?

## Alternative that already exists: matrix modulation popup

If the project uses **Matrix Modulation** (`Engine.createModulationMatrix()` +
`GlobalModulatorContainer` + matrix targets), the popup support is already
built in and this patch is unnecessary:

- Matrix targets automatically get a "Modulation for <targetId>" section with an
  **Assign** submenu of all container sources and per-source **Remove** items –
  `GlobalContainerMatrixModulationPopupData`
  (`hi_core/hi_modules/synthesisers/synths/GlobalModulatorContainer.cpp:251`),
  attached to components at
  `hi_scripting/scripting/api/ScriptComponentWrappers.cpp:474`.
- `matrix.setEditCallback(itemNames, fn)` injects custom script-defined items;
  the callback receives `(itemIndex, targetId)`
  (`hi_scripting/scripting/api/ScriptModulationMatrix.cpp:825`).
- A velocity source in the container (GlobalVoiceStart velocity modulator) makes
  "Assign → Velocity" appear on every target knob, with native ring display data
  (`getModulationDisplayData`).

So the honest framing of this proposal upstream is: *"the MPE-style toggle for
plain (non-matrix) modulator architectures"*. Projects able to adopt the matrix
should do that instead; the likely upstream response is "use the matrix", and
that response is reasonable.

## Verification recipe

1. In a test project, add a Velocity Modulator named `knbTestVelocity` to any
   gain chain; add a `ScriptSlider` with id `knbTest` and `enableMidiLearn=1`.
2. Build backend, right-click the slider: "Velocity Control" appears, untucked.
3. Toggle it: modulator bypass flips in the module tree; tick state follows on
   reopening the menu.
4. Right-click a slider with no matching modulator: no item (regression check
   that Learn/Assign CC, macro, and MPE items are unchanged).
5. Compile a frontend export and repeat 2–3 (the popup path differs via
   `isOnHiseModuleUI`).
