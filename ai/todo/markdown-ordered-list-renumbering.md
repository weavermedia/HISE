# Markdown ordered lists ignore literal numbers and renumber from 1

**Date:** 2026-07-04 (diagnosed 2026-06, Sublime license panel)
**Target commit:** `bada887e9` (branch `meatbeats`)
**Status:** TODO — not yet applied (this is the *remaining half*; the digit-eating half was fixed in `fc8da09a7`)
**Affects:** `MarkdownPanel` floating tile / `Content.createMarkdownRenderer` — any ordered list whose numbering matters

## Symptom

An ordered list written as `3.` / `4.` / `5.` renders as `1.` / `2.` / `3.`. Worse, blank-line-separated numbered lines are each parsed as their **own single-item list**, so section headers written `1.` / `2.` / `3.` (with paragraphs between) ALL render as `1.`.

(Historical context: this used to be compounded by any digit-leading line — `1) Definitions`, `1 osc` — being routed to the enumeration parser and having its digit silently consumed. That half is **fixed**: `isEnumeration()` in `hi_tools/hi_markdown/MarkdownParser.cpp:783` now peeks for the full `<digits>.` pattern before committing, commit `fc8da09a7`.)

## Root cause

Two spots:

1. `MarkdownParser::parseEnumeration` (`hi_tools/hi_markdown/MarkdownParser.cpp:174`) consumes the digit run without capturing its value:
   ```cpp
   while (CharacterFunctions::isDigit(it.peek()))
       skipTagAndTrailingSpace();
   ```
2. `EnumerationList` (`hi_tools/hi_markdown/MarkdownElements.cpp:572`, `:581`) synthesizes numbering unconditionally:
   ```cpp
   int rowIndex = 1;
   ...
   bp << rowIndex++ << ".";
   ```

Since a blank line ends the `while (isEnumeration())` loop, each separated numbered line becomes a fresh `EnumerationList` restarting at 1.

## Fix

Capture the parsed digit run in `parseEnumeration` (at minimum for the first item) and pass it to `EnumerationList` as the starting index; seed `rowIndex` from it instead of hard-coding 1. That fixes both the literal-number case and the blank-line-separated-sections case in one move (each single-item list starts at its own literal number). Full CommonMark fidelity (per-item literal numbers) would mean passing the whole number array, but start-index seeding covers the practical cases.

Sketch:
- `parseEnumeration`: accumulate the digit characters into an `int firstNumber` for the first iteration; pass to the `EnumerationList` constructor.
- `EnumerationList`: store `startIndex`, replace `int rowIndex = 1;` with `int rowIndex = startIndex;`.

## Project workaround (shipped in Sublime `Scripts/LicensePanel.js`, keep until fixed)

Never start a display line with a digit when the literal number matters: bold `**headers**` instead of `1.` headers, en-dash `–` (U+2013) bullets instead of `- `. No backslash escaping exists in this parser.

## Verification

1. `3. three\n4. four\n5. five` → renders `3.` `4.` `5.`.
2. `1. one\n\ntext\n\n2. two` → renders `1.` and `2.` (not `1.` twice).
3. Regression on `fc8da09a7`: `1) Definitions` and `1 osc` still render verbatim (not routed to enumeration).
4. Plain `1. a\n2. b\n3. c` unchanged.
