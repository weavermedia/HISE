# MIDI automation is restored AFTER onInit from project/instance state – add companion flag `HISE_MIDI_AUTOMATION_IN_PLUGIN_STATE`

**Date:** 2026-07-10 (follow-up to `HISE_MIDI_AUTOMATION_IN_USER_PRESETS`, fork commit `5b09675f2`)
**Target commit:** `67fa79271` (branch `meatbeats`)
**Status:** DONE - landed in meatbeats `422dfa104` (2026-07-11), verified across all four flag combinations (editor + exported plugins); pushed to [christophhart/HISE#995](https://github.com/christophhart/HISE/pull/995) as companion commit `70bd24ef3`, PR retitled to cover the pair. Applied as proposed, plus: the `HISE_MIDI_AUTOMATION_IN_USER_PRESETS` doc comment and database entry were qualified to reference this flag (their "still saved in instance state" wording is only true when this flag is enabled), and the database entry sits alphabetically before the USER_PRESETS entry.
**Affects:** projects managing MIDI CC assignments themselves (e.g. Sublime's global `MidiMappings.json` via `Engine.createMidiAutomationHandler()`)

## Symptom

With `HISE_MIDI_AUTOMATION_IN_USER_PRESETS=0`, presets no longer touch CC
assignments – but a project that applies its own mappings in `onInit`
(script-side global persistence) still gets them wiped, because HISE restores
`MidiAutomation` from *project/instance state* **after** script compilation:

- **Editor project load** (`MainController::loadPresetInternal`): the project
  XML's `<MidiAutomation/>` is stashed via `setUnloadedData()` *before*
  `compileAllScripts()` (`MainController.cpp:562`) and applied *after*
  compilation via `loadUnloadedData()` (`MainController.cpp:577` – postponed
  deliberately to resolve attribute indexes). Sublime's project XML carries an
  empty `<MidiAutomation/>`, so every project load clears whatever `onInit`
  applied.
- **Exported plugin instantiation** (`FrontendProcessor::restorePlugin`):
  the embedded project state's `MidiAutomation` node is restored after
  `compileAllScripts()` (`FrontEndProcessor.cpp:458`) – same wipe.
- **DAW session load** (`FrontendProcessor::setStateInformation`,
  `FrontEndProcessor.cpp:576`): the per-instance session copy overwrites the
  onInit-applied global mappings.

Worse than the wipe itself: each restore fires the handler's change message,
so a script `setUpdateCallback` persistence scheme (write-on-change) then
**writes the wiped/stale state back into its global file**. Script-side code
cannot distinguish these restores from real user edits – the opt-out has to
live in HISE.

There is also a save-side counterpart: `ModulatorSynthChain::exportAsValueTree`
(`ModulatorSynthChain.cpp:210`) writes the `MidiAutomation` node into the
project XML / `.hip` / embedded state, and `MainController::savePluginState`
(`MainController.cpp:2378`) writes it into DAW session state – dead/lying data
once the project owns persistence.

## Fix

New dynamic preprocessor `HISE_MIDI_AUTOMATION_IN_PLUGIN_STATE`, default **1**
(current behaviour, fully backwards compatible). Orthogonal companion to
`HISE_MIDI_AUTOMATION_IN_USER_PRESETS`: that one covers *user presets*, this
one covers *project/instance/session state*. Both at 0 = MIDI automation is
never persisted natively; the project script is the single owner. `MPEData`
untouched throughout.

Backend sites use `HISE_GET_PREPROCESSOR` (runtime read from Extra
Definitions, no HISE rebuild); frontend-only sites use plain `#if` (the macro
is baked into the export by the generated Projucer project).

### 1. Default + doc – `hi_core/hi_core.h` (directly below `HISE_MIDI_AUTOMATION_IN_USER_PRESETS`)

```cpp
/** Config: HISE_MIDI_AUTOMATION_IN_PLUGIN_STATE

If enabled (default), MIDI CC assignments are stored in the project/instance
state (project file, embedded plugin data, DAW session chunk) and restored
from it after script compilation. Disable this together with
HISE_MIDI_AUTOMATION_IN_USER_PRESETS to make the project script the single
owner of MIDI CC assignments (e.g. persisting them to a global file in the
app data folder through the MidiAutomationHandler scripting object) – the
post-compilation restore would otherwise overwrite the assignments the script
applied in onInit.

Note that this is a dynamic preprocessor so you don't need to recompile HISE
to use this functionality, but just add
HISE_MIDI_AUTOMATION_IN_PLUGIN_STATE=0 to your ExtraDefinitions.
*/
#ifndef HISE_MIDI_AUTOMATION_IN_PLUGIN_STATE
#define HISE_MIDI_AUTOMATION_IN_PLUGIN_STATE 1
#endif
```

### 2. Editor project load, deferred restore – `hi_core/hi_core/MainController.cpp:562` (`loadPresetInternal`)

```diff
-				getMacroManager().getMidiControlAutomationHandler()->setUnloadedData(v.getChildWithName("MidiAutomation"));
+				if (HISE_GET_PREPROCESSOR(this, HISE_MIDI_AUTOMATION_IN_PLUGIN_STATE))
+					getMacroManager().getMidiControlAutomationHandler()->setUnloadedData(v.getChildWithName("MidiAutomation"));
```

(`loadUnloadedData()` at line 577 then no-ops on the invalid tree – no second
gate needed.)

### 3. Chain restore, non-deferred path – `hi_core/hi_dsp/modules/ModulatorSynthChain.cpp:415` (`restoreFromValueTree`)

```diff
-	if (!getMainController()->shouldSkipCompiling())
+	if (!getMainController()->shouldSkipCompiling()
+		&& HISE_GET_PREPROCESSOR(getMainController(), HISE_MIDI_AUTOMATION_IN_PLUGIN_STATE))
 	{
 		ValueTree autoData = v.getChildWithName("MidiAutomation");
```

### 4. Save side, project/chain export – `hi_core/hi_dsp/modules/ModulatorSynthChain.cpp:210` (`exportAsValueTree`)

```diff
-		v.addChild(getMainController()->getMacroManager().getMidiControlAutomationHandler()->exportAsValueTree(), -1, nullptr);
+		if (HISE_GET_PREPROCESSOR(getMainController(), HISE_MIDI_AUTOMATION_IN_PLUGIN_STATE))
+			v.addChild(getMainController()->getMacroManager().getMidiControlAutomationHandler()->exportAsValueTree(), -1, nullptr);
```

(Removes the `<MidiAutomation/>` node from the project XML on the next editor
save – intended: no lying data at rest.)

### 5. Save side, DAW session chunk – `hi_core/hi_core/MainController.cpp:2378` (`savePluginState`)

```diff
-    getUserPresetHandler().saveStateManager(v, UserPresetIds::MidiAutomation);
+    if (HISE_GET_PREPROCESSOR(this, HISE_MIDI_AUTOMATION_IN_PLUGIN_STATE))
+        getUserPresetHandler().saveStateManager(v, UserPresetIds::MidiAutomation);
```

### 6. Exported plugin, embedded-state restore – `hi_frontend/frontend/FrontEndProcessor.cpp:458` (`restorePlugin`)

```diff
+#if HISE_MIDI_AUTOMATION_IN_PLUGIN_STATE
 	ValueTree autoData = synthData.getChildWithName("MidiAutomation");
 
 	if (autoData.isValid())
 		getMacroManager().getMidiControlAutomationHandler()->restoreFromValueTree(autoData);
+#endif
```

### 7. Exported plugin, DAW session restore – `hi_frontend/frontend/FrontEndProcessor.cpp:576` (`setStateInformation`)

```diff
+#if HISE_MIDI_AUTOMATION_IN_PLUGIN_STATE
     getUserPresetHandler().restoreStateManager(v, UserPresetIds::MidiAutomation);
+#endif
```

### 8. Register in the preprocessor database – `hi_backend/backend/PreprocessorDatabase.cpp` (next to the `HISE_MIDI_AUTOMATION_IN_USER_PRESETS` entry)

```cpp
data["HISE_MIDI_AUTOMATION_IN_PLUGIN_STATE"] = Entry()
	.withCategory(Category::AutomationAndMacros)
	.withBrief("Stores MIDI learn CC assignments in the project and plugin instance state and restores them after script compilation.")
	.withDescriptionLine("When enabled (the default), MIDI CC assignments live in the project file, the embedded plugin data and the DAW session chunk, and are restored from there after every project load, plugin instantiation and session load – including after the interface script's onInit has run. Disable this together with HISE_MIDI_AUTOMATION_IN_USER_PRESETS when the project script owns the assignments itself (for example persisting them to a global file through the MidiAutomationHandler scripting object), because the post-compilation restore would otherwise overwrite the script-applied assignments and re-trigger the update callback with the overwritten state.")
	.withDescriptionLine("> Read at runtime from the Extra Definitions, so no HISE rebuild is required. With this disabled the project XML no longer contains a MidiAutomation node after the next save.")
	.withDefault(1)
	.withValue(HISE_MIDI_AUTOMATION_IN_PLUGIN_STATE)
	.withHotReload()
	.withCrossReference(LinkType::ScriptingApi, "MidiAutomationHandler", "the scripting object that owns the assignments when this is disabled")
	.withCrossReference(LinkType::Preprocessor, "HISE_MIDI_AUTOMATION_IN_USER_PRESETS", "companion flag covering user presets; set both to 0 for fully script-owned MIDI automation");
```

## Semantics when disabled (`=0`)

- Project XML / `.hip` / embedded plugin data: node neither written nor
  restored (editor project loads no longer clear onInit-applied mappings).
- DAW session chunk: node neither written nor restored (session load no
  longer overwrites the global mappings).
- User presets: unaffected by this flag – governed by
  `HISE_MIDI_AUTOMATION_IN_USER_PRESETS`.
- In-memory behaviour (learn, popup, table UI, update callback) unchanged.

## Verification

1. Both flags unset: byte-identical behaviour to today (node in project XML,
   session persistence, post-compile restore).
2. Both flags `=0` in ExtraDefinitions, editor: MIDI-learn a knob → reload
   the project → mapping survives (script re-applies from
   `MidiMappings.json`, nothing wipes it post-compile); saving the project
   drops `<MidiAutomation/>` from the project XML.
3. Both flags `=0`, exported plugin: map a CC in instance A → open a new
   instance B → mapping present; reopen an old DAW session → current global
   mappings shown, not the session's stale copy; `MidiMappings.json` never
   gets overwritten by loads.
4. `PLUGIN_STATE=0` with `USER_PRESETS=1` (odd but legal): presets still
   store/restore; instance state doesn't.

### Verification notes (2026-07-11)

All four flag combinations passed, editor + exported plugins (script layer
disabled for the flag tests; config `00` additionally passed the
`MidiMappings.js` integration tests). One finding from the default-config
regression pass, stock behaviour, not caused by these flags:

- **Saving an XML backup silently rewrites MIDI mappings** in any project
  with a DefaultUserPreset: `saveFileXml`
  (`BackendApplicationCommands.cpp:1938`, upstream `fb9472ca5`, 2023) calls
  `initDefaultPresetManager({})` before exporting the chain, which runs a
  full internal load of the default preset - with `USER_PRESETS=1` that
  load replaces the live assignments before they are serialized.
  `loadPresetInternal` does the same after `loadUnloadedData()`
  (`MainController.cpp:601` vs `:577`), so project-state mappings are
  stomped by the default preset on every project load too. With
  `USER_PRESETS=0` this entire class of wipe disappears. Third face of the
  same problem; worth citing if the PR needs defending.

## Upstream

Propose together with `HISE_MIDI_AUTOMATION_IN_USER_PRESETS` as a pair:
"presets" and "instance state" are independent axes of where MIDI learn
persists, both defaulting to current behaviour. The Sublime use case (global
per-machine mapping file, the Serum/Vital convention) needs both at 0.
