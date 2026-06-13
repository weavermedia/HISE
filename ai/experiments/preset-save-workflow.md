# Preset save workflow (factory presets + later user saving)

**Date:** 2026-06-12

## Goal

Ship a plugin with ~50 factory presets the user cannot overwrite, embedded in the
binary and extracted to AppData on first launch, refreshed on plugin update. Phase
the work so release 1 is read-only (no Save button) and a later release adds the
ability for users to save their own presets without ever touching the factory set.

This was a design discussion, not yet implemented. Release 1 (read-only browser) is
the immediate target; user saving is deferred to a later release. Recorded so the
folder-layout decision and the save-routing rules are not re-derived later.

## Project settings

Three settings govern factory preset shipping and updates:

- **Embed User Presets** (default Yes): collects the project `UserPresets/` folder
  into the binary at export and auto-extracts to AppData on first launch. Leave on.
- **Overwrite Old User Presets** (default No): on launch, compares an `info.json`
  version string in the AppData preset folder against the running plugin version.
  Off -> if the preset dir exists, never re-extract. On -> re-extract (overwrite by
  filename) whenever the version string differs. Turn this ON so plugin updates
  refresh shipped presets.
- **Read Only Factory Presets** (default No): advisory only. It flags extracted
  factory paths as read-only so UI can grey/hide a Save button and so
  `Engine.isUserPresetReadOnly(file)` reports correctly. It does NOT enforce at the
  save path - `saveUserPreset` will still overwrite a factory file if pointed at
  one. Turn ON once saving is added (harmless before then).

### Critical operational rules

- **Bump the project Version string on every release that changes presets.** The
  Overwrite-Old trigger is version-string inequality (`!=`), not a date or real
  "is older" check. Same version -> `info.json` matches -> re-extraction is skipped
  and the update silently ships stale presets.
  See `hi_core/hi_core/PresetHandler.cpp:386-420` (`extractUserPresets`).
- **Re-extraction adds and overwrites by filename but never prunes.** New presets
  are added, changed ones overwritten, but presets removed/renamed in a later
  version leave orphaned files behind in AppData. So do not reorganize the on-disk
  layout across versions (see folder layout below).

## Where presets live and how the browser shows them

- AppData root: `getAppDataDirectory()/User Presets`
  (`hi_core/hi_core/PresetHandler.cpp:1542`). Factory and user presets share one
  tree. Subfolders become browser category columns.
- Factory-vs-user is tracked by an extracted-paths set, not by folder. `isReadOnly(f)`
  is true only for files in that set (`hi_core/hi_core/UserPresetHandler.cpp:831`).
- Stock `PresetBrowser` `numColumns` (1-3) maps to folder depth
  (`hi_core/hi_components/plugin_components/PresetBrowser.cpp`):
  - 1 column: recursive scan, shows every `.preset` flat, **ignores subfolders**
    (`rebuildAllPresets`, line 765, recurse=true).
  - 2 columns: column 1 = top-level folders ("banks"), column 2 = presets inside.
  - 3 columns: bank / category / preset.

## Folder layout decision (do this in release 1)

Put the 50 factory presets under a **`Factory/` subfolder** of the project's
`UserPresets/` from release 1, even though release 1 uses a 1-column browser:

```
<project>/UserPresets/
    Factory/          <- 50 factory presets here from day one
    (User/ created lazily on first user save, later release)
```

Rationale: a 1-column browser flattens and ignores folders, so release 1 still shows
just the 50. Later, switching to a 2-column browser renders the same on-disk data as
`Factory` / `User` columns with **zero migration**. If instead release 1 ships
presets flat at the root and a later version moves them into `Factory/`, the
no-prune re-extraction leaves the old root files in place -> 50 duplicates plus
orphaned loose files the 2-column browser cannot categorize.

## User saving (later release)

### Routing and guard

Save to a dedicated `User/` subfolder using the explicit-file form of
`Engine.saveUserPreset`, never the string form:

```javascript
const var presetRoot = FileSystem.getFolder(FileSystem.UserPresets);
// create User/ lazily on first save: presetRoot.createDirectory("User");
local userFolder = presetRoot.getChildFile("User");
local target = userFolder.getChildFile(chosenName + ".preset");

if (Engine.isUserPresetReadOnly(target))   // belt-and-suspenders; never Factory
    return;

Engine.saveUserPreset(target);             // ScriptFile form -> writes exactly here
```

- The **string form** `Engine.saveUserPreset("name")` saves a SIBLING of the
  currently-loaded preset (`hi_core/hi_core/UserPresetHandler.cpp:579-582`). If a
  factory preset is loaded, that lands in the Factory folder and a name collision
  could overwrite a factory file (save path has no read-only guard). Do not use it.
- The save captures current (tweaked) plugin state via `createUserPreset`
  (`hi_core/hi_core/PresetHandler.cpp:107`).
- The browser **auto-refreshes**: saving with notification sets `currentlyLoadedFile`
  to the new preset and calls `sendRebuildMessage()`
  (`hi_core/hi_core/PresetHandler.cpp:95-100`); `PresetBrowser` listens
  (`PresetBrowser.cpp:513`) and rebuilds (`PresetBrowser.cpp:403`). No manual
  rebuild needed. New preset shows in the User column, selected.

### Save button: leave it always on

Decision: leave Save enabled at all times rather than dirty-gating it. The name
prompt already makes clear the user is creating a named copy, not overwriting the
factory preset, and the factory files are never a save target. Always-on also
supports a legitimate case - copying an unmodified factory preset into the User
collection under a chosen name - which dirty-gating would block.

Rejected alternative (dirty tracking): enable Save only after a parameter changes,
via `Engine.createBroadcaster` + `attachToComponentValue` to set a dirty flag, reset
in `createUserPresetHandler().setPostCallback`, with an `isCurrentlyLoadingPreset()`
guard so the value flood during a preset load does not falsely mark dirty. This
works but is more code and the guard is easy to get wrong; the protection it adds is
already covered by the name-prompt + User-folder routing.

### Collision handling

`saveUserPreset` silently deletes-and-rewrites an existing same-name file
(`hi_core/hi_core/PresetHandler.cpp:70-76`). With always-on Save, add a same-name
check within the User folder (`target.isFile()`) and show a "replace existing?"
confirm so a user does not silently overwrite their own earlier save. This is
User-vs-User data only; factory presets are never reachable.

## Summary of settings per phase

- Release 1 (read-only): Embed User Presets = Yes, Overwrite Old User Presets = Yes,
  factory presets under `UserPresets/Factory/`, 1-column browser, no Save button.
- Later (saving): set browser to 2 columns, turn Read Only Factory Presets = Yes,
  route saves to `UserPresets/User/` via the explicit ScriptFile form + guard,
  always-on Save, User-folder collision confirm.
</content>
</invoke>
