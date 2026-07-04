# ScriptPanel borderSize defaults to 2.0 — should be 0.0

**Date:** 2026-07-04 (diagnosed 2026-05, Sublime palette migration)
**Target commit:** `bada887e9` (branch `meatbeats`)
**Status:** TODO — not yet applied
**Affects:** every `ScriptPanel` without an explicit `borderSize` attribute

## Symptom

A ScriptPanel with no `borderSize` attribute silently renders a 2px border stroke in its `textColour`. Nobody asked for a border, but one paints anyway — and because it's the default for newly created panels, projects accumulate invisible border cruft: developers set `textColour` thinking it's a text colour, notice a stray stroke, and panic-set more attributes. During Sublime's palette migration, zeroing the XML and removing colour overrides for "no-border" panels still left borders rendering through the default.

## Root cause

`hi_scripting/scripting/api/ScriptingApiContent.cpp:4490`:

```cpp
setDefaultValue(borderSize, 2.0f);
```

## Fix

```diff
-	setDefaultValue(borderSize, 2.0f);
+	setDefaultValue(borderSize, 0.0f);
```

⚠️ **Behaviour change:** any existing project that *relies* on the implicit 2px border (i.e. panels with a visible border but no `borderSize` attribute in the XML) will lose it. Most projects set `borderSize` explicitly, but this deserves a forum post to Christoph before (or alongside) landing it in the fork. Note the related serializer wrinkle: HISE strips default-matching attributes on save, so a panel that once had `borderSize="2.0"` written explicitly may have had it stripped — such panels would silently change appearance.

## Verification

1. Create a fresh ScriptPanel with no `borderSize` attribute, non-transparent `textColour`: no border should render.
2. `borderSize="2.0"` explicitly set: 2px border still renders.
3. Load Sublime: no visual change expected (all panels set `borderSize` explicitly or use overlay-panel borders).
