# SVG To Path Converter - output formats reference

**Status:** reference / documentation of existing behavior. No change proposed.
**Date:** 2026-07-03.
**Target:** `~/Code/HISE` @ `fc8da09a7`.
**Tool:** HISE IDE menu `Tools > Show SVG To Path Converter`.
**Class:** `SVGToPathDataConverter` (`hi_backend/backend/BackendApplicationCommandWindows.cpp:2356-2872`).

---

## Overview

The SVG To Path Converter takes a vector source (an SVG document, an SVG path `d`
string, or a list of points) and emits a code snippet you paste into a script or
into C++. The output format is chosen from a combo box (`:2380`):

```cpp
outputFormatSelector.addItemList(
    { "Base64 Path", "C++ Path String", "HiseScript Path number array", "Base64 SVG" }, 1);
```

These map to the `OutputFormat` enum (`:2364-2371`):

| Combo label                    | Enum value           |
|--------------------------------|----------------------|
| Base64 Path                    | `Base64Path`         |
| C++ Path String                | `CppString`          |
| HiseScript Path number array   | `HiseScriptNumbers`  |
| Base64 SVG                     | `Base64SVG`          |

Three of the four formats (`Base64Path`, `CppString`, `HiseScriptNumbers`) encode
the **same binary path geometry** - they differ only in text packaging. The fourth
(`Base64SVG`) is a different animal: it carries the full SVG document with styling.

---

## Input each format expects

The converter parses input differently depending on whether the SVG format is
selected (`:2696-2714`):

- **Base64 SVG** parses the input as a full SVG XML document
  (`XmlDocument::parse` -> `Drawable::createFromSVG`, `:2698-2700`). It keeps
  colours, strokes, gradients, groups, and transforms.
- **All three path formats** run the input through:
  1. `Drawable::parseSVGPath(text)` - an SVG path `d` string (or the `d` from an
     SVG), then
  2. `pathFromPoints(text)` as a fallback if the path came back empty - a plain
     list of point coordinates (`:2707-2710`).

  The resulting path is scaled to fit before serialization (`PathFactory::scalePath`,
  `:2712-2713`). All styling is discarded; only the outline geometry survives.

---

## The four outputs

### Base64 Path (`Base64Path`)

Binary path data from `Path::writePathToStream` (`:2732-2733`), Base64-encoded, no
compression (`:2750-2754`):

```cpp
out << "const var " << filename << " = ";
out << "\"" << mb.toBase64Encoding() << "\"";
```

Produces:

```js
const var myIcon = "....";
```

Load in HISEScript:

```js
var p = Content.createPath();
p.loadFromData(myIcon);
// in a paint routine you pick the colour:
g.fillPath(p, area);
```

### HiseScript Path number array (`HiseScriptNumbers`)

The same binary path bytes, emitted as a HISEScript array of numbers
(`:2755-2760`):

```js
const var myIcon = [110, 109, ...];
```

Loaded identically - `loadFromData` accepts either the base64 string or the number
array.

### C++ Path String (`CppString`)

The same binary path bytes as a C++ byte-array literal plus ready-to-paste load
code (`:2738-2748`):

```cpp
static const unsigned char myIcon[] = { 110,109,... };

Path path;
path.loadPathFromData (myIcon, sizeof (myIcon));
```

For C++ code - JUCE `Path`, custom LookAndFeel, `PathFactory`, module GUIs. This is
the classic Projucer "SVG to path" output format.

### Base64 SVG (`Base64SVG`)

The entire SVG XML text, ZSTD-compressed then Base64-encoded (`:2724-2729`,
`:2750-2754`):

```cpp
zstd::ZDefaultCompressor comp;
comp.compress(text, mb);
// ...
out << "const var " << filename << " = ";
out << "\"" << mb.toBase64Encoding() << "\"";
```

Produces a base64 string loaded via `Content.createSVG`:

```js
const var logoData = "....";
var svg = Content.createSVG(logoData);
// renders with its own embedded colours/styling:
g.drawSVG(svg, area, 1.0);
```

Preserves everything: multiple paths, fill/stroke colours, gradients, groups,
transforms.

---

## Downstream consumption

### Path formats -> `ApiHelpers::loadPathFromData`

`Content.createPath().loadFromData(...)` routes to
`ApiHelpers::loadPathFromData` (`hi_scripting/scripting/api/ScriptingApiObjects.cpp:7292-7320`),
which handles both the base64 string and the number array and produces the same
`Path`:

```cpp
void ApiHelpers::loadPathFromData(Path& p, var data)
{
    if (data.isString())
    {
        juce::MemoryBlock mb;
        mb.fromBase64Encoding(data.toString());
        p.clear();
        p.loadPathFromData(mb.getData(), mb.getSize());
    }
    else if (data.isArray())
    {
        p.clear();
        Array<unsigned char> pathData;
        Array<var>* varData = data.getArray();
        const int numElements = varData->size();
        pathData.ensureStorageAllocated(numElements);
        for (int i = 0; i < numElements; i++)
            pathData.add(static_cast<unsigned char>((int)varData->getUnchecked(i)));
        p.loadPathFromData(pathData.getRawDataPointer(), numElements);
    }
    // ... also accepts an existing PathObject
}
```

### Base64 SVG -> `SVGObject`

`Content.createSVG(...)` builds a `SVGObject`
(`hi_scripting/scripting/api/ScriptingGraphics.cpp:882-901`), which Base64-decodes,
ZSTD-expands, and rebuilds the Drawable:

```cpp
zstd::ZDefaultCompressor comp;
MemoryBlock mb;
mb.fromBase64Encoding(b64);
String xmlText;
comp.expand(mb, xmlText);
// ... on the message thread:
if (auto xml = XmlDocument::parse(xmlText))
    obj.svg = Drawable::createFromSVG(*xml);
```

---

## Choosing a format

| Output                     | Destination  | Carries styling | Form                          |
|----------------------------|--------------|-----------------|-------------------------------|
| Base64 Path                | HISEScript   | No              | base64 string                 |
| HiseScript number array    | HISEScript   | No              | `[...]` byte array            |
| C++ Path String            | C++ / JUCE   | No              | `unsigned char[]` + load code |
| Base64 SVG                 | HISEScript   | Yes             | ZSTD + base64 string          |

Rules of thumb:

- Single-colour icon you tint from script (button states, LAF glyphs) -> **Base64 Path**.
- Same, but you want a readable/diffable source constant -> **HiseScript number array**.
- Multi-colour designed artwork/logo rendered as-is -> **Base64 SVG**.
- C++ / JUCE code (LookAndFeel, module GUI) -> **C++ Path String**.

---

## Performance: Base64 Path vs. HiseScript number array

No meaningful runtime difference, and base64 is slightly cheaper at load time.

- **Runtime:** identical. Both decode to the same `Path` and render the same way.
  Zero difference once loaded.
- **Load time:** base64 is one `MemoryBlock::fromBase64Encoding` pass
  (`ScriptingApiObjects.cpp:7297-7300`). The number array stores every byte as a
  boxed `var` and the loader loops over them casting `var -> int -> unsigned char`
  into a temporary array (`:7302-7314`) - more work per element.
- **Memory / compile time:** the number-array constant sits in memory as a large
  `Array<var>` (each `var` ~16+ bytes vs. 1 byte of real data) and parses slower at
  script-compile time than a compact quoted string.

In practice this is negligible for typical icons (a few hundred bytes to a few KB) -
microseconds at startup, zero once drawn. It only becomes noticeable with very large
paths or many loaded at once. Since it costs nothing, base64 is the sensible default;
the number array's only real advantage is readability/diffability.

---

## Source references

- `hi_backend/backend/BackendApplicationCommandWindows.cpp:2356-2872` - `SVGToPathDataConverter`
  - `:2364-2371` - `OutputFormat` enum
  - `:2380` - combo box labels
  - `:2696-2714` - input parsing (SVG vs. path vs. points)
  - `:2724-2734` - serialization (ZSTD-compress SVG vs. `writePathToStream`)
  - `:2738-2761` - the four output text formats
- `hi_scripting/scripting/api/ScriptingApiObjects.cpp:7292-7320` - `ApiHelpers::loadPathFromData`
- `hi_scripting/scripting/api/ScriptingGraphics.cpp:882-901` - `SVGObject` constructor
