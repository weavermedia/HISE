# Engine.loadUserPreset(string): documented ".preset" auto-append is a no-op

**Date:** 2026-07-04 (diagnosed 2026-06, Sublime preset navigation)
**Target commit:** `bada887e9` (branch `meatbeats`)
**Status:** TODO — not yet applied
**Affects:** `Engine.loadUserPreset` with a relative-path string argument

## Symptom

`Engine.loadUserPreset(name)` silently fails for extensionless names — exactly what `Engine.getUserPresetList()` returns (`["00 INIT", "01", ...]`, no extensions). The API docs claim the extension is auto-appended; it isn't. In the editor you get `reportScriptError("...doesn't exist")`; in an exported plugin the failure is **silent** (no console). Natural pairing of the two APIs breaks unless the script appends `".preset"` itself.

## Root cause

`hi_scripting/scripting/api/ScriptingApi.cpp:3187-3190` (inside `Engine::loadUserPreset`, `:3169`):

```cpp
userPresetToLoad = userPresetRoot.getChildFile(file.toString());

if(userPresetToLoad.hasFileExtension(".preset"))
    userPresetToLoad = userPresetToLoad.withFileExtension(".preset");
```

The condition is inverted relative to the intent: the extension is only "appended" when it's **already there** (a no-op). Extensionless input keeps its bare path, `existsAsFile()` is false, and the call bails.

## Fix

Minimal (negate the condition):

```diff
-        if(userPresetToLoad.hasFileExtension(".preset"))
+        if(!userPresetToLoad.hasFileExtension(".preset"))
             userPresetToLoad = userPresetToLoad.withFileExtension(".preset");
```

⚠️ Caveat: `File::withFileExtension` **replaces** everything after the last dot, so a preset named `v1.2 Lead` would become `v1.preset`. Preset names with dots are legal, so the robust form is a plain string append:

```cpp
if(!userPresetToLoad.hasFileExtension(".preset"))
    userPresetToLoad = File(userPresetToLoad.getFullPathName() + ".preset");
```

Prefer the robust form.

## Verification

1. `Engine.loadUserPreset(Engine.getUserPresetList()[0])` — loads without the manual `+ ".preset"`.
2. `Engine.loadUserPreset("01.preset")` — explicit extension still works (no double extension).
3. Preset with a dot in its name (`v1.2 Lead`) — loads correctly with the robust form.
4. Sublime side, once fixed: the `+ ".preset"` appends in `Interface.js` become redundant (harmless to keep — path already has the extension, both fix forms skip it).
