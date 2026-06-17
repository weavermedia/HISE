# Persisting per-plugin UI state across instances (hide/show keyboard)

**Date:** 2026-06-16

## Goal

Persist a per-plugin UI preference - the motivating case is hide/show keyboard -
so that it survives across all instances of the exported plugin, but is NOT part
of any user preset. Toggling it in one place should become the default the next
time the plugin loads, independent of whichever preset is recalled.

This was a design discussion, not yet implemented. Recorded so the "where does
global, non-preset state live" question is not re-derived later.

## Why not reuse `General Settings.xml`

The obvious idea - drop a custom key into the existing `General Settings.xml` in
AppData - does not work safely. That file is owned by `GlobalSettingManager` and
is rewritten wholesale on every save by `saveSettingsAsXml()`
(`hi_core/hi_core/StandaloneProcessor.cpp:591`). It builds a fresh
`GLOBAL_SETTINGS` XML element from a fixed set of attributes and writes it over the
file:

- `DISK_MODE`, `SCALE_FACTOR`, `VOICE_AMOUNT_MULTIPLIER`, `MIDI_CHANNELS`, `OPEN_GL`
- `SAMPLES_FOUND` (frontend only, guarded by `#if USE_FRONTEND`)
- `GLOBAL_BPM` (standalone app only, guarded by `#if IS_STANDALONE_APP`)

There is no merge/round-trip of unknown keys. Any custom attribute injected by hand
is dropped the next time HISE writes the file - which happens on any zoom/disk/voice
change. So `General Settings.xml` is the wrong container for app-defined state.

What it does usefully confirm: this whole AppData-settings mechanism is live in
exported frontends. `GlobalSettingManager::restoreGlobalSettings()` is called during
frontend init (`hi_frontend/frontend/FrontEndProcessor.cpp:74`), and the settings
file sits in the per-product AppData folder. That folder is the right place; we just
want our own file in it, not HISE's.

## What to use instead: own JSON file in the AppData folder

Write a dedicated JSON file via the scripting `FileSystem` API. In a frontend build
`FileSystem.AppData` (`hi_scripting/scripting/api/ScriptingApi.h:1773`) resolves to
the same per-product folder HISE itself uses,
`FrontendHandler::getAppDataDirectory()` -> `[AppDataRoot]/[Company]/[Product]/`
(`hi_core/hi_core/PresetHandler.cpp:1354`):

- macOS: `~/Library/Application Support/[Company]/[Product]/`
- Windows: `%APPDATA%\[Company]\[Product]\`
- (Global-AppData project flags relocate the root, but the structure holds.)

```javascript
const var configFile = FileSystem.getFolder(FileSystem.AppData).getChildFile("UIState.json");

// Save (from the hide/show keyboard control callback):
inline function saveUIState(keyboardVisible)
{
    configFile.writeObject({ "keyboardVisible": keyboardVisible });
}

// Restore on load (onInit):
if (configFile.isFile())
{
    const var state = configFile.loadAsObject();
    // apply state.keyboardVisible to the keyboard floating tile's visibility
}
```

Because this is a plain file write and not a `saveInPreset` component, it is
automatically excluded from presets - which is exactly the requirement. Do NOT back
the toggle with a `saveInPreset` control, or the state leaks into presets.

## Side effects / caveats

- **Global, not live-synced.** The file is shared by every instance, but instances
  only read it at load. With two instances open at once, instance B will not see a
  change instance A just wrote until B reloads (or explicitly re-reads the file).
  For a "default on next launch" preference this is fine; it is persistence, not
  cross-instance live sync.
- **Concurrent writes.** If multiple instances can write the same file, last-writer
  wins. Acceptable for a single low-frequency UI flag; revisit if the file grows to
  hold many independent settings written by different instances.
- **Keep it out of the preset deliberately.** The whole point is that this preference
  is orthogonal to preset content. Confirm the toggle control is not flagged
  `saveInPreset`.

## Related

- Preset/global storage split also discussed in
  [preset-save-workflow.md](preset-save-workflow.md) (factory vs user presets in the
  `User Presets` AppData tree). This file covers the non-preset, global-default case.
