# Embedded LICENSE.txt to AppData (design + re-implementation guide)

Status: NOT IMPLEMENTED (shelved). This documents a working implementation that was built
and verified, then reverted. Use this to re-implement later if desired.

## Goal

Mirror HISE's "embed user presets, extract to AppData on first run" mechanism for a single
human-readable `LICENSE.txt`:

- Each HISE *project* provides its own `LICENSE.txt` at the project root (picked up at
  export, baked into the binary).
- On first run of the exported plugin/standalone, the text is written to the AppData folder
  next to the `User Presets` folder: `{AppDataRoot}/{Company}/{Product}/LICENSE.txt`.
- Version detection (always on): keep an existing `LICENSE.txt` only if it was written by
  the current project version; otherwise (re)write it, overwriting any existing file. This
  is independent of the `HISE_OVERWRITE_OLD_USER_PRESETS` macro (which only governs presets).

This was confirmed working: backend builds clean, export emits the symbol + wiring, first
run writes the file, and a version bump overwrites it.

## How the existing user-preset mechanism works (the pattern being mirrored)

Export side (`hi_backend/backend/CompileExporter.cpp`):

- `CompileExporter::exportInternal()` (~line 621-627) collects presets into a ValueTree and
  calls `compressValueTree<UserPresetDictionaryProvider>(tree, directoryPath, "userPresets")`,
  which writes a temp file named `userPresets` into the export temp dir.
- `CppBuilder::exportValueTreeAsCpp()` (line 2697) scans that temp dir for ALL files
  (`findChildFiles(..., "*")`) and, via `CppBuilder::addFile()` (line 2641), emits each as
  `extern const char* PresetData::<filename>;` + `const int <filename>Size;` in the
  generated `PresetData.h`/`.cpp`. `addFile` appends `,0,0}` (line 2678) so the byte array is
  null-terminated -- a text file becomes a usable C string. Note: `CppBuilder::isHiddenFile`
  (line 2686) skips zero-byte files, so an empty source file produces no symbol.
- `HeaderHelpers::addBasicIncludeLines()` (line 3080-3112) emits the generated plugin's
  `getEmbeddedData()` switch via the `BEGIN_EMBEDDED_DATA` / `DEFINE_EMBEDDED_DATA` macros,
  mapping each `PresetData::xxx` symbol to a `FileHandlerBase::SubDirectories` enum case.

Frontend side:

- `FrontendFactory::createPluginWithAudioFiles()`
  (`hi_frontend/frontend/FrontEndProcessor.cpp` line 36-86) calls
  `getEmbeddedData(FileHandlerBase::UserPresets)` (line 63) and passes the blob to
  `UserPresetHelpers::extractUserPresets()` (line 65).
- `UserPresetHelpers::extractUserPresets()` (`hi_core/hi_core/PresetHandler.cpp` line 381)
  is guarded by `#if USE_FRONTEND && !DONT_CREATE_USER_PRESET_FOLDER`, resolves
  `FrontendHandler::getUserPresetDirectory()`, decompresses, writes files. Its version
  detection (under `#if HISE_OVERWRITE_OLD_USER_PRESETS`) reads/writes an `info.json`
  holding Name/Version/Date and compares `FrontendHandler::getVersionString()`.
- AppData root is `FrontendHandler::getAppDataDirectory()`
  (`hi_core/hi_core/PresetHandler.cpp` line 1354): `{AppDataRoot}/{Company}/{ProjectName}/`.

## Key design decisions

1. Do NOT add a `License` value to `FileHandlerBase::SubDirectories` / route through
   `getEmbeddedData`. That enum is enumerated by many loops/switches (folder creation, UI
   listings, `getSubDirectoryPath`, etc.); a new value would make HISE treat "License" as a
   real project subfolder. Too invasive for one text file.

2. Instead, add a tiny dedicated getter `FrontendFactory::getEmbeddedLicenseText()` that
   follows the same "declared in the hi_frontend module, defined in generated code via a
   macro" indirection as `getEmbeddedData`. Leaves the enum untouched.

3. Reuse the `CppBuilder` auto-scan: drop a temp file literally named `license` into the
   export temp dir and `PresetData::license` + `PresetData::licenseSize` are generated for
   free. No `CppBuilder` change.

4. Only embed/wire when the project has a non-empty `LICENSE.txt` (zero-byte files are
   dropped by `isHiddenFile`, which would leave the symbol undefined and fail to link). When
   absent, emit `NO_EMBEDDED_LICENSE()` so the getter still resolves (returns nullptr).

5. Version detection uses a DEDICATED `license_info.json` in the AppData root, NOT the
   presets' `info.json`. Reason: the license helper runs AFTER `extractUserPresets`, which
   (in overwrite mode) has already bumped the presets' stored version -- sharing it would
   make the license comparison always read "unchanged".

6. The license version detection is ALWAYS ON, deliberately decoupled from
   `HISE_OVERWRITE_OLD_USER_PRESETS` (which still governs preset behavior only).

## Gotchas

- `CompileExporter.cpp` and `FrontEndProcessor.cpp` contain MIXED line endings (mostly CRLF
  with some LF-only regions). The Edit tool normalized whole files to CRLF, producing
  unrelated whitespace churn (e.g. in `addStaticDspFactoryRegistration`). When
  re-implementing, insert edits without reflowing those files, or reconstruct from HEAD so
  the diff only shows intended additions.
- ASCII only (no em-dashes / smart quotes), per repo rules.
- Like `getEmbeddedData`, `getEmbeddedLicenseText()` is DEFINED ONLY in generated code, so
  existing projects must be RE-EXPORTED (normal after an engine update). Rebuilding an old
  export without re-exporting fails to link. This matches the existing `BEGIN_EMBEDDED_DATA`
  contract.
- The runtime write path lives under `#if USE_FRONTEND`, so it is NOT compiled by the HISE
  IDE / backend build (`build_meatbeats.sh`). The backend build only verifies the export
  side and that the helper compiles; the write path is only exercised by running an export.

## Exact changes (verbatim, as built and verified)

### 1. `hi_frontend/hi_frontend.h`

Add the getter declaration to `struct FrontendFactory` (after `getEmbeddedData`):

```cpp
	/** Returns the embedded LICENSE.txt text baked in at export time, or nullptr if the
	    project had no LICENSE.txt. Defined in the generated plugin source via the
	    SET_EMBEDDED_LICENSE / NO_EMBEDDED_LICENSE macros below. */
	static const char* getEmbeddedLicenseText();
```

Add the macros after `END_EMBEDDED_DATA()`:

```cpp
#define SET_EMBEDDED_LICENSE(textData) const char* hise::FrontendFactory::getEmbeddedLicenseText() { return textData; }
#define NO_EMBEDDED_LICENSE() const char* hise::FrontendFactory::getEmbeddedLicenseText() { return nullptr; }
```

### 2. `hi_frontend/frontend/FrontEndProcessor.cpp`

In `createPluginWithAudioFiles()`, right after the `extractUserPresets(...)` call (line 65):

```cpp
	FrontendHandler::extractEmbeddedLicense(getEmbeddedLicenseText());
```

### 3. `hi_core/hi_core/PresetHandler.h`

Declare the helper on `FrontendHandler` (after `getAppDataDirectory`):

```cpp
	/** Writes the embedded LICENSE.txt text (baked in at export time) into the AppData
	    directory. The file is (re)written whenever the project version changes (tracked via
	    license_info.json); an existing license written by the current version is left
	    untouched. This always uses version detection, independent of
	    HISE_OVERWRITE_OLD_USER_PRESETS. Does nothing if the text is null. */
	static void extractEmbeddedLicense(const char* licenseText);
```

### 4. `hi_core/hi_core/PresetHandler.cpp`

Implement after `FrontendHandler::getAppDataDirectory()` (~line 1364):

```cpp
void FrontendHandler::extractEmbeddedLicense(const char* licenseText)
{
#if USE_FRONTEND
	if (licenseText == nullptr)
		return;

	auto licenseFile = getAppDataDirectory().getChildFile("LICENSE.txt");

	// Use the same version-detection idea as the user presets, but always on (independent of
	// HISE_OVERWRITE_OLD_USER_PRESETS): keep an existing license only if it was written by
	// the current version, otherwise (re)write it, overwriting the old file. A dedicated
	// marker is used (not the presets' info.json) because this runs after the presets have
	// already bumped their version.
	auto infoFile = getAppDataDirectory().getChildFile("license_info.json");
	auto infoObj = JSON::parse(infoFile);

	if (licenseFile.existsAsFile() &&
		infoObj.getProperty(ExpansionIds::Version, "").toString() == getVersionString())
		return;

	licenseFile.replaceWithText(String(licenseText));

	infoObj = var(new DynamicObject());
	infoObj.getDynamicObject()->setProperty(ExpansionIds::Name, getProjectName());
	infoObj.getDynamicObject()->setProperty(ExpansionIds::Version, getVersionString());
	infoObj.getDynamicObject()->setProperty("Date", Time::getCurrentTime().toISO8601(true));
	infoFile.replaceWithText(JSON::toString(infoObj));

#else
	ignoreUnused(licenseText);
#endif
}
```

### 5. `hi_backend/backend/CompileExporter.cpp` (export side, two edits)

In `exportInternal()`, immediately after the user-preset embed
(`compressValueTree<UserPresetDictionaryProvider>(userPresetTree, directoryPath, "userPresets");`):

```cpp
		// Embed the project's LICENSE.txt (if present) so it can be written to AppData on
		// first run. The temp file is named "license" so CppBuilder auto-generates
		// PresetData::license. Skip empty files (CppBuilder::isHiddenFile drops zero-byte
		// files, which would leave the symbol undefined).
		{
			auto licenseSource = GET_PROJECT_HANDLER(chainToExport).getWorkDirectory().getChildFile("LICENSE.txt");

			if (licenseSource.existsAsFile() && licenseSource.getSize() > 0)
				licenseSource.copyFileTo(tempDirectory.getChildFile("license"));
		}
```

In `HeaderHelpers::addBasicIncludeLines()`, after `END_EMBEDDED_DATA()` / its trailing
`p << "\n";`:

```cpp
	// Wire the embedded LICENSE.txt (if the project has a non-empty one). Must match the
	// "license" temp file written in exportInternal() so PresetData::license exists.
	auto licenseSource = GET_PROJECT_HANDLER(exporter->chainToExport).getWorkDirectory().getChildFile("LICENSE.txt");

	if (licenseSource.existsAsFile() && licenseSource.getSize() > 0)
		p << "\nSET_EMBEDDED_LICENSE(PresetData::license);";
	else
		p << "\nNO_EMBEDDED_LICENSE();";

	p << "\n";
```

Note: the project root is `GET_PROJECT_HANDLER(chainToExport).getWorkDirectory()`
(`hi_core/hi_core/PresetHandler.h` line 303; the same accessor used at
`CompileExporter.cpp` line 296). `tempDirectory` and `directoryPath` are defined at the top
of `exportInternal()` (line 596-598; `directoryPath` is `tempDirectory.getFullPathName()`).

## Manual test steps

Setup: put a non-empty `LICENSE.txt` at the HISE project root (the folder containing
`Scripts/`, `Images/`, `UserPresets/`, `Binaries/`). Open it in a HISE build with these
changes.

1. Embed at export: export (Standalone or plugin). Confirm `Binaries/PresetData.cpp` has a
   `license` array, `PresetData.h` has `extern const char* license;` + `licenseSize`, and the
   generated plugin source contains `SET_EMBEDDED_LICENSE(PresetData::license);`. Compile.
2. First-run write: run the export once. Confirm
   `~/Library/Application Support/{Company}/{Product}/LICENSE.txt` (macOS) exists next to
   `User Presets`, with a `license_info.json` alongside, and the text matches the source.
3. Version detection:
   - Same version, relaunch: files untouched.
   - Edit the AppData LICENSE.txt, relaunch: NOT overwritten (version unchanged).
   - Bump project version, re-export, rebuild, run: LICENSE.txt overwritten, info Version bumps.
   - Delete the AppData LICENSE.txt, relaunch same build: recreated.
4. No-license case: export a project with no LICENSE.txt. Confirm the generated source emits
   `NO_EMBEDDED_LICENSE();`, it builds, and no LICENSE.txt is written to AppData.
5. Backend sanity: build the HISE IDE (`build_meatbeats.sh`) -- the `#if USE_FRONTEND` guard
   leaves it unaffected (this only verifies the export side compiles).

## Possible future variations

- Per-project toggle: add an `Embed License File` boolean to `HiseSettings::Project` (like
  `EmbedUserPresets`) and gate both export edits on it.
- Different destination/filename, or write into a subfolder rather than the AppData root.
- Compress the text (overkill for a small license; presets use zstd because they are large).
