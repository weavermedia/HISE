# HISE patch handoff — expose user presets as host programs (AU Presets menu / VST3 program list)

**Status:** proposal / handoff for the HISE repo. Not yet implemented or tested.
**Date:** 2026-07-03.
**Target:** `~/Code/HISE` @ `fc8da09a7` (`hi_frontend/frontend/FrontEndProcessor.h` / `FrontendProcessor.cpp`).
**Class:** `FrontendProcessor`.

---

## Problem

Every exported HISE plugin presents an **empty preset list to the host**:

- **Logic (AU):** the plugin header's "AU Presets" submenu is empty, and Logic's native
  `[` / `]` (Previous/Next Setting) commands have nothing to walk.
- **Cubase (VST3):** no program list in the program selector.
- **Reaper (VST3/AU):** the FX-window preset dropdown shows no factory presets.

Competing plugins (e.g. Maize Sampler exports) populate this list, so HISE exports look
broken by comparison.

The host-facing motivation that surfaced this (Sublime project): the user shows the
plugin window with a hotkey and expects Logic's `[` / `]` to step presets immediately.
Logic's native keys work at **window** focus; a script-level
`Content.setKeyPressCallback` only fires once the **plugin UI component** has grabbed
keyboard focus (i.e. after a click into the UI — `ScriptContentComponent::keyPressed`,
`ScriptingContentComponent.cpp:750`, is focus-routed by JUCE). Populating the host
program list makes Logic's own window-level keys do the job; no focus hack needed.

## Root cause

`FrontendProcessor` stubs out the JUCE program API — including a commented-out
half-start, so this was clearly intended once:

```cpp
// hi_frontend/frontend/FrontEndProcessor.h:192
int getNumPrograms() override
{
    return 1;// presets.getNumChildren() + 1;
}

const String getProgramName(int /*index*/) override { return "Default"; }

int getCurrentProgram() override { return 0; }

// FrontendProcessor.cpp:624
void FrontendProcessor::setCurrentProgram(int /*index*/) { return; }
```

JUCE builds the host-facing preset list *entirely* from these four methods:

- **AU:** `GetPresets` / `NewFactoryPresetSet` (`juce_AU_Wrapper.mm:1418` / `:1448`)
  map `getNumPrograms`/`getProgramName` to the AU factory-preset array and route a menu
  selection to `setCurrentProgram`.
- **VST3:** `IUnitInfo::getProgramListInfo` etc. (`juce_VST3_Wrapper.cpp:373-403`) plus
  an automatable "Program" parameter (`ProgramChangeParameter`, `:804`, flagged
  `kIsProgramChange | kCanAutomate`) — both only created when `getNumPrograms() > 1`.

With the stubs, both wrappers see one program ("Default") and publish nothing useful.

This is **not reachable from a project**: no script API, XML attribute, or export flag
touches these overrides (`ReadOnlyFactoryPresets` is unrelated — it only write-protects
factory preset files).

## Sketched fix

Expose the same flat, sorted preset list that `UserPresetHandler::incPreset()`
(`hi_core/hi_core/UserPresetHandler.cpp:725`) already uses for prev/next navigation, so
the host menu order matches the in-plugin browser and script `loadNext/PreviousUserPreset`
exactly.

**`hi_frontend/frontend/FrontEndProcessor.h`** — replace the four stubs (lines 192-209):

```cpp
int getNumPrograms() override
{
    return jmax(1, getFactoryPresetList().size());
}

const String getProgramName(int index) override
{
    auto& list = getFactoryPresetList();
    if (isPositiveAndBelow(index, list.size()))
        return list[index].getFileNameWithoutExtension();
    return "Init";
}

int getCurrentProgram() override
{
    return jmax(0, getFactoryPresetList().indexOf(getUserPresetHandler().getCurrentlyLoadedFile()));
}

void setCurrentProgram(int index) override;
```

Add to the `private:` section (near line 218):

```cpp
// Flat, sorted list of every *.preset under the frontend UserPreset dir,
// exposing presets to the host as "factory presets" (AU Presets menu / VST3
// program list). Built lazily; call refreshFactoryPresetList() to rebuild.
Array<File> factoryPresetList;
const Array<File>& getFactoryPresetList();
void refreshFactoryPresetList();
```

**`hi_frontend/frontend/FrontendProcessor.cpp`** — replace the no-op (line 624):

```cpp
const Array<File>& FrontendProcessor::getFactoryPresetList()
{
    if (factoryPresetList.isEmpty())
        refreshFactoryPresetList();
    return factoryPresetList;
}

void FrontendProcessor::refreshFactoryPresetList()
{
    factoryPresetList.clearQuick();
    auto presetDir = FrontendHandler::getUserPresetDirectory();
    presetDir.findChildFiles(factoryPresetList, File::findFiles, true, "*.preset");
    PresetBrowser::DataBaseHelpers::cleanFileList(this, factoryPresetList);
    factoryPresetList.sort();
}

void FrontendProcessor::setCurrentProgram(int index)
{
    auto& list = getFactoryPresetList();
    if (isPositiveAndBelow(index, list.size()))
        getUserPresetHandler().loadUserPreset(list[index], false);
}
```

(`FrontendProcessor` inherits `MainController`, so `getUserPresetHandler()` and
`this`-as-`MainController*` both resolve. Enumeration mirrors `incPreset()`:
`findChildFiles(..., true, "*.preset")` → `cleanFileList` → `sort()`.)

## Recall-integrity analysis (why by-index is safe)

Verified in the wrappers: **session reload never reconstructs sound from the program
index.**

- **AU:** `SaveState`/`RestoreState` (`juce_AU_Wrapper.mm:700`/`:732`) persist the full
  `get/setStateInformation` chunk. `AUBase::RestoreState` explicitly resets
  `mCurrentPreset.presetNumber = -1` ("custom") on restore (`AUBase.cpp:2124-2130`);
  `setCurrentProgram` is only ever called from an explicit user menu pick
  (`NewFactoryPresetSet`, `AUBase.cpp:928-931`).
- **VST3:** `setState` (`juce_VST3_Wrapper.cpp:2637`) → `setStateInformation` (full
  chunk). The controller's `setComponentState` *derives* the Program parameter's display
  from the already-restored `getCurrentProgram()` (`:947-949`) rather than re-applying a
  stored index.

So renaming/reordering/deleting/editing presets between sessions never changes what a
saved session sounds like — the state blob is authoritative in every host/format.

Remaining by-index effects are cosmetic, plus one marginal VST3 edge:

1. Menu checkmark can point at the wrong name if presets are reordered *mid-session*
   after a menu pick (AU stores the tick by number).
2. After reload the AU menu shows no tick / "custom" (number forced to -1) even though a
   real preset is loaded.
3. VST3 only: the Program parameter is automatable; written program-change automation
   re-applies by index on playback, so reordering/inserting presets between plugin
   versions changes what that automation loads. Append-only preset ordering avoids it.

## Host-by-host payoff (expected; per-host UI behavior not yet empirically verified)

| Host | Format | Result |
|---|---|---|
| Logic | AU | AU Presets submenu populates; native `[` / `]` (window-level focus) step presets |
| Cubase | VST3 | Named program selector populates |
| Reaper | VST3/AU | FX-window preset dropdown populates |
| Ableton Live | VST3 | Weak — no browsable program menu; only the step "Program" parameter |

## Known gaps in the sketch (decide before merging)

1. **Stale list after a user saves a preset in-session.** The list is built lazily and
   cached; a preset saved via the in-plugin browser won't appear in the host menu until
   reload. Fix: call `refreshFactoryPresetList()` from
   `UserPresetHandler::postPresetSave()` (needs a hook or a listener from
   `FrontendProcessor`).
2. **Host display doesn't follow in-plugin preset changes.** When the preset changes via
   the plugin's own browser / script `loadNext/PreviousUserPreset`, the host's ticked
   item goes stale. Fix: call `updateHostDisplay
   (ChangeDetails().withProgramChanged(true))` on preset load (e.g. from
   `postPresetLoad()`), which triggers AU `PropertyChanged(kAudioUnitProperty_PresentPreset)`
   / VST3 `restartComponent`.
3. **Expansions:** `incPreset()` also folds expansion preset folders into its list; the
   sketch only scans `FrontendHandler::getUserPresetDirectory()`. Mirror the expansion
   logic if host lists should include expansion presets.
4. **Threading:** hosts may call `getNumPrograms`/`getProgramName` early or off the
   message thread; the lazy build does file I/O. Consider building the list once at
   construction instead of lazily.
5. `getNumPrograms()` returning 1 with an empty dir vs. `getProgramName` returning
   "Init" — harmless, but pick a deliberate empty-state story.

## Relation to Sublime project

- The in-UI `[` / `]` shortcuts (`Content.setKeyPressCallback` in
  `Scripts/ScriptProcessors/Sublime/Interface.js`) stay: they're the only key-nav path
  in VST3 hosts and only fire when the UI has focus, so they don't double-trigger with
  Logic's native keys.
- Both paths walk the same sorted list, so host-menu order, in-plugin browser order, and
  `[` / `]` order all agree.
