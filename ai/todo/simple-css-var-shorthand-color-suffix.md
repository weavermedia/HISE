# simple_css: `background: var(--x)` drops the `-color` suffix and renders transparent

**Date:** 2026-07-04 (diagnosed 2026-06, Sublime popup styling)
**Target commit:** `bada887e9` (branch `meatbeats`)
**Status:** TODO — not yet applied
**Affects:** any stylesheet using the `background:` / `border:` shorthand with a CSS variable value — popups, scrollbars, anything the LAF styles via `.popup` / `.popup-item`

## Symptom

```css
.popup { background: var(--bg); }         /* paints WHITE (transparent + JUCE fillAll shows through) */
.popup { background-color: var(--bg); }   /* paints correctly — the workaround */
```

With a literal colour (`background: #1A1A1A;`) the shorthand works. Only the `var(--*)` form breaks.

## Root cause

The shorthand-to-longhand suffix decision is value-type-driven, and the variable type short-circuits it:

1. `Parser::findValueType` (`hi_tools/simple_css/CssParser.cpp:1726`) checks `value.startsWith("var(--")` **first** and returns `ValueType::Variable`, before the `colourPrefixes` loop that would have returned `ValueType::Colour`.
2. `Parser::getTokenSuffix` (`CssParser.cpp:1782`) only appends `-color` for `background`/`border` inside the `if(v == ValueType::Colour)` branch (`CssParser.cpp:1819-1825`). For `ValueType::Variable`, no branch matches and it returns `""`.
3. The property is therefore stored as plain `"background"` (`addOrOverwrite(...rv.property + suffix...)`, `CssParser.cpp:2235` / `:2247`).
4. At paint time, `Renderer::drawBackground` (`hi_tools/simple_css/Renderer.cpp:812`) looks up `"background-color"`, misses, and defaults to `Colours::transparentBlack`. For popups, JUCE's `MenuWindow` paint has already done `g.fillAll(Colours::white)` (`JUCE/modules/juce_gui_basics/menus/juce_PopupMenu.cpp:421`), so white shows through.

## Fix

The suffix decision only needs the **property name**, not the resolved value — a variable can't be resolved at parse time, but `background`/`border` + variable can only sensibly mean the colour longhand. Insert before the `ValueType::Colour` branch at `CssParser.cpp:1819`:

```cpp
if(v == ValueType::Variable)
{
	if(p == PropertyType::Border || keyword == "background")
		return appendColour(keyword);

	return "";
}
```

(`appendColour` already guards against a double `-color` suffix.)

## Related bug — `*`-rule custom properties invisible to popup/scrollbar selectors

Same user-facing complaint, separate root cause, worth fixing/filing together: variables declared on `*` are not visible when the LAF resolves `.popup` etc. `StyleSheet::Collection::getWithAllStates` (`hi_tools/simple_css/StyleSheet.cpp:804`) gates matches on `if(l->isAll() != wantsAll) return;` (`:823`), so when `getBestPopupStyleSheet` (`hi_tools/simple_css/CSSLookAndFeel.cpp:446`) merges sheets for a `.popup` selector, the `*` sheet is excluded — and any `--*` declared there with it. Workaround shipped in Sublime: re-declare the vars on `.popup, .popup-item` (`Palette.cssVarDecls` in `Scripts/LookAndFeel.js`). Proper fix: treat custom-property declarations as their own cascade — copy them from the `*` rule unconditionally during `getWithAllStates`, regardless of the `isAll()` filter. More invasive than the suffix fix; can land separately.

## Verification

1. In any `setInlineStyleSheet`:
   ```css
   * { --bg: #1A1A1A; }
   .popup, .popup-item { --bg: #1A1A1A; background: var(--bg); }
   ```
   Open the component's popup: should paint dark, not white.
2. Regression: `background: #1A1A1A;` (literal) and `background-color: var(--bg);` (longhand) both still work.
3. `border: 1px solid var(--bg);` — border colour resolves (exercises the `PropertyType::Border` arm).
4. If the related `*`-cascade fix is applied: remove the `.popup`-scoped re-declaration and confirm the `*`-declared var still reaches the popup.
