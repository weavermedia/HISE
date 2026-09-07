# MIDI Learn / macro table: centre the Inverted, Min and Max column headers

**Date:** 2026-08-26 (noticed in Sublime, MIDI Control panel)
**Target commit:** `ca472e27b` (branch `meatbeats`)
**Status:** DONE. Committed on meatbeats as `3c32e253b` (2026-08-29): opt-in `centredColumnIds` list on TableHeaderLookAndFeel, empty by default, with TableFloatingTileBase opting in the three widget columns; the simple_css header path untouched. Upstreamed as PR #1031 (https://github.com/christophhart/HISE/pull/1031, branch `fix/midilearn-header-centring`, cherry-pick `f3d96f50b` on upstream `5d34685fe`). Moved to done 2026-09-07.
**Affects:** `MidiLearnPanel` and `FrontendMacroPanel` (both `TableFloatingTileBase`), stock-header path only

## Symptom

In the MIDI Learn table the Inverted column holds a centred toggle button and the Min/Max
columns hold centred value sliders, but their headers are drawn left-aligned like the text
columns (CC #, Parameter). "Inverted" in particular sits visibly off to the left of the
`Normal`/`Inverted` buttons beneath it.

## Why it can't be fixed from the project side

`TableHeaderLookAndFeel::drawTableHeaderColumn` (`hi_tools/hi_tools/HI_LookAndFeels.cpp`
~1770) hard-codes `Justification::centredLeft` with a 3 px inset for every column, and
JUCE's `TableHeaderComponent` carries no per-column justification a project could set.

The only project-side lever is a `th` rule in the tile's stylesheet, which switches the
whole header to the simple_css renderer (`TableFloatingTileBase::refreshComponentForCell` /
`resized()`). That route was tried in Sublime (2026-08-26) and rejected:

- simple_css header columns are only distinguishable by `:first-child` / `:last-child`
  (`CssParser.cpp` ~1461); there is no `nth-child`, so `text-align: center` can't target
  Inverted/Min/Max without also centring Parameter.
- With a `th` rule present, `resized()` fixes every column from Inverted onwards to its
  header's text width, so the table's proportions change as well (recoverable with
  `button { width }` / `.range-slider { width }`, but it is a lot of CSS to restore the
  stock look).
- The `th:first-child` rule also drew the first header ~4 px lower than the others.

A `td` rule on its own (no `th`) is fine - Sublime uses one to make the CC column
content-width - because the header stays on the stock LAF.

## Fix

Opt-in list of centred columns on `TableHeaderLookAndFeel`. Empty by default, so the other
users (`BackendPanelTypes.h`, `BackendComponents.h`, `SamplePoolTable`, the sampler's
`SampleEditorComponents.h`, `ScriptingPanelTypes.cpp`) keep their left-aligned headers.
`TableFloatingTileBase` opts in the three widget columns.

`hi_tools/hi_tools/HI_LookAndFeels.h` (class `TableHeaderLookAndFeel`, ~line 196):

```diff
 	Font f;
 	Colour bgColour;
 	Colour textColour;
+
+	/** Column ids whose header text is centred instead of left-aligned. Empty by
+	    default so existing tables are unchanged; opt in for columns whose cells hold
+	    centred widgets (toggle buttons, value sliders) so the header lines up with
+	    its content. */
+	Array<int> centredColumnIds;
 };
```

`hi_tools/hi_tools/HI_LookAndFeels.cpp` (`TableHeaderLookAndFeel::drawTableHeaderColumn`,
~line 1770 - the column id parameter is currently the unnamed `int i`):

```diff
 void TableHeaderLookAndFeel::drawTableHeaderColumn(Graphics& g, TableHeaderComponent& tableHeaderComponent,
-	const String& columnName, int i, int width, int height, bool cond, bool cond1, int i1)
+	const String& columnName, int columnId, int width, int height, bool cond, bool cond1, int i1)
 {
 	if (width > 0)
 	{
 		g.setColour(bgColour);

 		g.fillRect(0.0f, 0.0f, (float)width - 1.0f, (float)height);

 		g.setFont(f);
 		g.setColour(textColour);

-		g.drawText(columnName, 3, 0, width - 3, height, Justification::centredLeft, true);
+		if (centredColumnIds.contains(columnId))
+			g.drawText(columnName, 0, 0, width - 1, height, Justification::centred, true);
+		else
+			g.drawText(columnName, 3, 0, width - 3, height, Justification::centredLeft, true);
 	}
 }
```

`hi_core/hi_components/floating_layout/FrontendPanelTypes.cpp` (`TableFloatingTileBase`
constructor, ~line 1857):

```diff
 	laf = new TableHeaderLookAndFeel();
+
+	// Inverted holds a centred toggle, Min/Max centred value sliders - centre their
+	// headers over them. CC # / Channel / Parameter stay left like their text cells.
+	laf->centredColumnIds = { Inverted, Minimum, Maximum };

 	table.getHeader().setLookAndFeel(laf);
```

`width - 1` in the centred branch matches the background fill, which already stops one
pixel short of the column edge.

## Verification

1. Rebuild HISE, open Sublime, open the MIDI Control panel (`btnIconShowMidiControlPanel`).
2. Empty table: "Inverted", "Min" and "Max" sit centred over their 70 px columns; "CC #"
   and "Parameter" are unchanged (left, 3 px inset).
3. Assign a CC (right-click a knob > MIDI Learn) so rows exist: each header is centred
   over its toggle / slider. Row painting is unaffected (this touches the header only).
4. Sanity-check one other table - e.g. the backend's sample pool table - still draws
   left-aligned headers.

The simple_css path (`th` rule present) is untouched; it never calls this LAF.

## Upstream

Generic and behaviour-preserving for existing tables - worth a PR once verified. A
`th:nth-child(n)` implementation in simple_css would be the fuller fix but is a parser
feature, not a bug fix.
