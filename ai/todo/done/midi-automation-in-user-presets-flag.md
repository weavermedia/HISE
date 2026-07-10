# MIDI learn assignments are forced into user presets – add opt-out flag `HISE_MIDI_AUTOMATION_IN_USER_PRESETS`

**Date:** 2026-07-10 (diagnosed in Sublime, MIDI learn UX review)
**Target commit:** `86680e63d` (branch `meatbeats`)
**Status:** DONE - landed in meatbeats `bd6cfc9c7` (2026-07-10), verified in the editor; upstreamed as [christophhart/HISE#995](https://github.com/christophhart/HISE/pull/995). Applied as proposed, except the PreprocessorDatabase entry sits after `HISE_MACROS_ARE_PLUGIN_PARAMETERS` (the section is alphabetical) rather than directly next to `HISE_ENABLE_MIDI_LEARN`.
**Affects:** every project using the stock MIDI learn / `MidiLearnPanel`

## Symptom

MIDI CC assignments behave as *patch data*: every user preset save embeds the
current assignments, and every preset load **wipes and replaces** them –
including a full clear when the preset's `<MidiAutomation>` node is empty or
missing (`restoreUserPresetState` falls through to `resetUserPresetState()` →
`clear()`). The end-user experience: map your mod wheel to a knob, browse two
presets, mapping silently gone.

Most modern synths (Serum, Vital, u-he) treat MIDI CC mappings as
per-instance/global config, not patch data – users expect mappings to survive
preset browsing. There is currently **no way to opt out**: the save and the
load-time clear are unconditional in C++, and no script hook can intercept the
preset ValueTree before it hits disk.

Side effect: presets saved with no assignments still carry a dead
`<MidiAutomation/>` node (misleading data-at-rest), and presets saved *with*
assignments leak the author's controller setup to every user.

## Root cause / sites

The `MidiControllerAutomationHandler` is registered as a
`UserPresetStateManager` and the user-preset path processes it
unconditionally:

1. **Save** – `hi_core/hi_core/PresetHandler.cpp:136`
   (`UserPresetHelpers::createUserPreset`):
   ```cpp
   chain->getMainController()->getUserPresetHandler().saveStateManager(preset, UserPresetIds::MidiAutomation);
   ```
2. **Load** – `hi_core/hi_core/UserPresetHandler.cpp:665`
   (`MainController::UserPresetHandler::loadUserPresetInternal`):
   ```cpp
   restoreStateManager(userPresetToLoad, UserPresetIds::MidiAutomation);
   ```

**Do NOT touch** the per-instance plugin-state path – that one is the
desirable persistence and must keep working regardless of the flag:

- `hi_core/hi_core/MainController.cpp:2378` (`MainController::savePluginState`)
- `hi_frontend/frontend/FrontEndProcessor.cpp:576` (`FrontendProcessor::setStateInformation`)

`MPEData` is a separate state manager and is untouched by this change.

## Fix

New **dynamic preprocessor** `HISE_MIDI_AUTOMATION_IN_USER_PRESETS`,
default **1** (= current behaviour, fully backwards compatible). "Dynamic"
in the established `HISE_MACROS_ARE_PLUGIN_PARAMETERS` sense: the project
sets it in **Settings panel → Project Settings → Extra Definitions**
(per-platform), no HISE rebuild needed –

- **Backend (HISE editor):** read at runtime via
  `HISE_GET_PREPROCESSOR(mc, HISE_MIDI_AUTOMATION_IN_USER_PRESETS)`
  (`hi_core/Macros.h:183` → `MainController::getExtraDefinitionsValue`,
  which parses the platform ExtraDefinitions from project settings and
  caches). This matters: preset authoring happens in the editor, so the
  gate must work there, not only in exports.
- **Frontend (exported plugin):** ExtraDefinitions are injected as real
  preprocessor definitions into the generated Projucer project, so the
  same `HISE_GET_PREPROCESSOR` macro collapses to the compile-time value
  (`Macros.h:188`).

### 1. Default + doc – `hi_core/hi_core.h` (next to `HISE_MACROS_ARE_PLUGIN_PARAMETERS`, ~line 565)

```cpp
/** Config: HISE_MIDI_AUTOMATION_IN_USER_PRESETS

If enabled (default), MIDI CC assignments made through MIDI learn are stored
in every user preset and restored (or cleared) whenever a preset is loaded –
the assignments behave like patch data. Disable this to make MIDI learn
assignments independent of the preset system: presets no longer contain a
MidiAutomation node and loading a preset leaves the current assignments
untouched. The assignments are still saved in the plugin instance state
(DAW session) either way. Disable this if your end users expect controller
mappings to survive preset browsing (the convention in most synth plugins).

Note that this is a dynamic preprocessor so you don't need to recompile HISE
to use this functionality, but just add
HISE_MIDI_AUTOMATION_IN_USER_PRESETS=0 to your ExtraDefinitions.
*/
#ifndef HISE_MIDI_AUTOMATION_IN_USER_PRESETS
#define HISE_MIDI_AUTOMATION_IN_USER_PRESETS 1
#endif
```

### 2. Gate the save – `hi_core/hi_core/PresetHandler.cpp:136`

```diff
-	chain->getMainController()->getUserPresetHandler().saveStateManager(preset, UserPresetIds::MidiAutomation);
+	if (HISE_GET_PREPROCESSOR(chain->getMainController(), HISE_MIDI_AUTOMATION_IN_USER_PRESETS))
+		chain->getMainController()->getUserPresetHandler().saveStateManager(preset, UserPresetIds::MidiAutomation);
	chain->getMainController()->getUserPresetHandler().saveStateManager(preset, UserPresetIds::MPEData);
```

### 3. Gate the restore – `hi_core/hi_core/UserPresetHandler.cpp:665`

```diff
-		restoreStateManager(userPresetToLoad, UserPresetIds::MidiAutomation);
+		if (HISE_GET_PREPROCESSOR(mc, HISE_MIDI_AUTOMATION_IN_USER_PRESETS))
+			restoreStateManager(userPresetToLoad, UserPresetIds::MidiAutomation);
		restoreStateManager(userPresetToLoad, UserPresetIds::MPEData);
```

(`mc` is the `MainController*` member available throughout
`loadUserPresetInternal`.)

### 4. Register in the preprocessor database – `hi_backend/backend/PreprocessorDatabase.cpp` (~line 994, `Category::AutomationAndMacros`, next to `HISE_ENABLE_MIDI_LEARN`)

```cpp
data["HISE_MIDI_AUTOMATION_IN_USER_PRESETS"] = Entry()
	.withCategory(Category::AutomationAndMacros)
	.withBrief("Stores MIDI learn CC assignments inside user presets and restores them on preset load.")
	.withDescriptionLine("When enabled (the default), every user preset save embeds the current MIDI CC assignments and every preset load replaces them with whatever the preset contains, including clearing them when the preset has none – the assignments behave like patch data. Disable this to decouple MIDI learn from the preset system: presets neither store nor touch CC assignments, so a mapping made by the end user survives preset browsing, which matches the per-instance convention of most synth plugins. The assignments are still saved and restored with the plugin instance state in the DAW session regardless of this setting.")
	.withDescriptionLine("> Read at runtime from the Extra Definitions, so no HISE rebuild is required. Old presets that contain a MidiAutomation node are simply ignored on load when this is disabled.")
	.withDefault(1)
	.withValue(HISE_MIDI_AUTOMATION_IN_USER_PRESETS)
	.withHotReload()
	.withCrossReference(LinkType::ScriptingApi, "MidiAutomationHandler", "assignments can still be snapshotted/restored in script for global-file persistence")
	.withCrossReference(LinkType::Preprocessor, "HISE_ENABLE_MIDI_LEARN", "controls whether MIDI learn assignments can be created at all");
```

## Semantics when disabled (`=0`)

- Preset save writes **no** `<MidiAutomation>` node.
- Preset load leaves current assignments **untouched** (no clear, no restore).
  Old presets that still contain a node are ignored.
- DAW-session persistence (plugin state) is unaffected – assignments survive
  project save/reopen per instance.
- Cross-preset AND cross-instance persistence can then be layered on in
  script via `Engine.createMidiAutomationHandler()`
  (`getAutomationDataObject` / `setAutomationDataFromObject` /
  `setUpdateCallback`) + a JSON file in the AppData folder – without this
  flag the preset load's clear/restore fights that scheme and forces a
  post-load re-apply workaround plus lying `<MidiAutomation>` blocks on disk.

## Verification

1. Flag unset / `=1`: byte-identical behaviour to today – save a preset with
   an assignment active, node present; load a preset, assignments replaced.
2. `HISE_MIDI_AUTOMATION_IN_USER_PRESETS=0` in ExtraDefinitions (editor):
   save a preset with a CC assigned → no `<MidiAutomation>` in the `.preset`;
   MIDI-learn a knob, load any preset → mapping survives.
3. Same project exported: confirm the definition lands in the generated
   Projucer project and the exported plugin shows the same two behaviours.
4. Either flag value: assign CC, save DAW session, reopen → mapping restored
   from instance state.
5. `ASSERT_EXTRA_DEFINITION_MATCH` semantics: in exported builds the macro is
   compile-time, so mismatched editor-vs-export settings behave per-build –
   same caveat as every dynamic preprocessor.

## Upstream

Worth proposing to Christoph as-is: default preserves existing projects, and
"MIDI learn shouldn't be patch data" is the standard end-user expectation for
synths. Pairs naturally with the existing `HISE_ENABLE_MIDI_LEARN` flag.
