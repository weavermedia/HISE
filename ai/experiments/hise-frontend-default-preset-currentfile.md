# HISE patch handoff — frontend default preset should set `currentlyLoadedFile`

**Status:** proposal / handoff for the HISE repo.
**Target:** `~/Code/HISE` @ `cbf58bc28` (`hi_core/hi_core/MainController.cpp`).
**Class:** `MainController::UserPresetHandler::DefaultPresetManager`.

---

## Problem

In an **exported plugin**, the `<DefaultUserPreset>` project setting loads the default
preset's *values* but leaves `UserPresetHandler::currentlyLoadedFile` **empty**. As a
result, on first launch:

- `Engine.getCurrentUserPresetName()` returns `""`.
- A preset-name display bound to it paints blank.
- HISE's `incPreset()` takes its "no current file" branch on the first prev/next press,
  snapping to the first preset instead of advancing relative to the default.

The HISE **editor** (backend) does **not** have this problem — it sets `defaultFile`, so
`currentlyLoadedFile` is correct there. The bug is frontend-only.

In the Sublime project this forced a pile of workaround code in `Interface.js`
(`navigateFromDefault()`, painting `obj.text` instead of the live preset name, and a
hardcoded `DEFAULT_PRESET_NAME`). All of it exists solely to compensate for this missing
file association and can be **deleted** once this is fixed upstream.

## Root cause

`DefaultPresetManager::init()` resolves and stores `defaultFile` only in the `USE_BACKEND`
branch. The frontend (`#else`) branch sets `defaultPreset` from the embedded ValueTree but
never assigns `defaultFile`:

```cpp
#if USE_BACKEND

    auto userPresetRoot = mc->getCurrentFileHandler().getSubDirectory(FileHandlerBase::UserPresets);
    auto f = userPresetRoot.getChildFile(defaultValue).withFileExtension(".preset");

    if (f.existsAsFile())
    {
        // only set the default file if it's a child of the user preset directory
        // (in order to allow a "hidden" default user preset)
        if (f.isAChildOf(userPresetRoot))
            defaultFile = f;

        if (auto xml = XmlDocument::parse(f))
            defaultPreset = ValueTree::fromXml(*xml);
    }

#else

    if (v.isValid())
        defaultPreset = v;        // <-- defaultFile never set in the frontend

#endif

    resetToDefault();
```

`resetToDefault()` then passes that empty `defaultFile` straight through:

```cpp
up.loadUserPresetFromValueTree(defaultPreset, up.currentlyLoadedFile, defaultFile, false);
//                                                                     ^^^^^^^^^^^ empty in frontend
```

so `currentlyLoadedFile` is set to an empty `File`.

By the time `DefaultPresetManager::init()` runs, the embedded presets have already been
extracted to disk — `FrontEndProcessor.cpp:65` calls
`UserPresetHelpers::extractUserPresets(...)` **before** the `FrontendProcessor` is
constructed (line 69), and the processor ctor is what compiles the interface and later
inits the default preset manager. So the on-disk file is guaranteed to exist and can be
resolved here, exactly as the backend branch does.

## Proposed fix

Mirror the backend branch in the frontend `#else`: resolve the default preset file under
`FrontendHandler::getUserPresetDirectory()` and assign `defaultFile` (keeping the same
`isAChildOf` guard that preserves the "hidden default user preset" capability — a default
that isn't on disk simply leaves `defaultFile` empty and behaves as today).

```diff
--- a/hi_core/hi_core/MainController.cpp
+++ b/hi_core/hi_core/MainController.cpp
@@ void MainController::UserPresetHandler::DefaultPresetManager::init(const ValueTree& v)
 #else
 	
 	if (v.isValid())
 		defaultPreset = v;
 
+	// Mirror the USE_BACKEND branch: resolve the default preset file on disk so that
+	// resetToDefault() sets currentlyLoadedFile. Without this, the exported plugin loads
+	// the default preset's values but leaves currentlyLoadedFile empty, so
+	// Engine.getCurrentUserPresetName() returns "" on first launch (and incPreset() takes
+	// its "no current file" branch on the first prev/next). The embedded presets are
+	// already extracted to disk at this point (FrontEndProcessor.cpp calls
+	// extractUserPresets() before constructing the processor), so the file resolves here.
+	auto userPresetRoot = FrontendHandler::getUserPresetDirectory();
+	auto f = userPresetRoot.getChildFile(defaultValue).withFileExtension(".preset");
+
+	// only set the default file if it's a child of the user preset directory
+	// (in order to allow a "hidden" default user preset)
+	if (f.existsAsFile() && f.isAChildOf(userPresetRoot))
+		defaultFile = f;
+
 #endif
 
 	resetToDefault();
```

`defaultValue` is in scope (declared before the `#if`, and the function early-returns when
it's empty), and `getChildFile(defaultValue)` handles sub-folder names like `Factory/01`
the same way the backend branch already does.

### Assumptions to verify in `~/Code/HISE`

- `FrontendHandler::getUserPresetDirectory()` is reachable from `MainController.cpp` in the
  frontend build (it's used from `ScriptingApi.cpp`, `PresetHandler.cpp`, and
  `FrontEndProcessor.cpp`; confirm the include/namespace is satisfied here, otherwise add
  the include or qualify it).
- Build once with `USE_BACKEND` (editor) to confirm the new code is inside `#else` and does
  not affect the editor path.

## Testing

1. **Frontend (the fix target):** export a plugin with `<DefaultUserPreset>` set to an
   existing preset (e.g. `Factory/<name>`). On a clean first launch, confirm
   `Engine.getCurrentUserPresetName()` returns that name (not `""`), the preset-name bar
   shows it, and the **first** prev/next advances relative to the default rather than
   snapping to the first list entry.
2. **Hidden-default regression:** set `<DefaultUserPreset>` to an embedded default that is
   not present on disk under the user-preset root; confirm it still loads the values and
   leaves the name blank (unchanged behavior — `defaultFile` stays empty).
3. **Editor:** confirm the backend path is unchanged (name already correct there before and
   after).

## Downstream cleanup in hise-sublime (after this ships in our HISE build)

Once `currentlyLoadedFile` is correct in the export, remove the workarounds in
`Scripts/ScriptProcessors/Sublime/Interface.js`:

- `navigateFromDefault()` and its calls in the prev/next control callbacks.
- The `obj.text` paint fallback in the preset-bar LAF (paint
  `Engine.getCurrentUserPresetName()` directly again).
- The `DEFAULT_PRESET_NAME` constant.

and set `<DefaultUserPreset>` in `project_info.xml` to the chosen first factory preset (the
`text="00 INIT"` literal in `SublimeDesktop.xml` becomes irrelevant once the bar paints the
live name).
