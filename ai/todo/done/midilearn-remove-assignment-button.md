# MIDI Learn panel: per-row X button to remove an assignment

**Date:** 2026-07-04
**Target commit:** `598670c6d` (branch `meatbeats`)
**Status:** Done - landed in meatbeats `2b615d9cb`, verified in a Debug build; deliberately kept fork-only (surface area too large for a simple upstream PR). Deviation from the diff below: the cell uses a path-drawn `ShapeButton` with the standard `HiBinaryData::ProcessorEditorHeaderIcons::closeIcon` (same X icon as the floating tile close button) instead of a `TextButton("X")`, after feedback that the letter X looked wrong. Consequence: `ShapeButton` paints itself without consulting the LookAndFeel, so the CSS `button`-selector hookup was dropped for this cell; the icon takes the panel's `textColour` instead.
**Affects:** `MidiLearnPanel` and `FrontendMacroPanel` (both derive from `TableFloatingTileBase`)
**Type:** Feature / UX (not a bug)

## Motivation

The only way to remove a MIDI Learn assignment from the panel is to select the row and press DELETE (`TableFloatingTileBase::deleteKeyPressed`). That is undiscoverable for end users of an exported plugin — nothing in the UI hints that rows are deletable, and a plugin UI is not a context where users expect keyboard-driven table editing. A visible per-row remove button fixes this.

Not achievable from the project side: the panel is a hardcoded C++ `TableListBox` (`FrontendPanelTypes.h:700` `TableFloatingTileBase`, `:861` `MidiLearnPanel`); columns and cell components are compiled in. HiseScript/XML/CSS can restyle it but cannot add a column.

## Design

Add a `Delete` column as the last column, with a small `TextButton("X")` cell component mirroring the existing `InvertedButton` pattern. Clicking it removes that row's assignment via the existing `deleteKeyPressed(row)` path (which already does the `isUsed` check + `removeEntry` + `updateContent` + `repaint`).

**Re-entrancy trap (the one non-obvious part):** `removeEntry()` notifies the handler → `changeListenerCallback` → `updateContent()`, which destroys the cell component whose `buttonClicked` is still on the stack. The click handler must therefore defer the removal with `MessageManager::callAsync` and a `Component::SafePointer` on the owner. (`deleteKeyPressed` gets away with running synchronously because the KeyListener isn't owned by a table cell.)

Enum note: `Delete` is inserted before `numColumns`/`columnWidthRatio`, which are dead members of the `ColumnId` enum — nothing reads them (the `options.numColumns` hits in `FrontendPanelTypes.cpp:1055ff` belong to a different struct), so no ID shift breaks anything.

JUCE note: `TableHeaderComponent::addColumn` has no non-empty-name assertion (`juce_TableHeaderComponent.cpp:106`), so an empty header title `""` for the X column is fine.

## Diff

### `hi_core/hi_components/floating_layout/FrontendPanelTypes.h`

Enum (inside `TableFloatingTileBase`, ~line 706):

```diff
 	enum ColumnId
 	{
 		CCNumber = 1,
 		Channel,
 		ParameterName,
 		Inverted,
 		Minimum,
 		Maximum,
+		Delete,
 		numColumns,
 		columnWidthRatio
 	};
```

After the `InvertedButton` class (still `protected:`, ~line 847):

```diff
+	class DeleteButton : public Component,
+		public ButtonListener
+	{
+	public:
+
+		DeleteButton(TableFloatingTileBase &owner_);
+
+		void resized();
+		void setRow(const int newRow);
+		void buttonClicked(Button *b);
+
+		ScopedPointer<TextButton> t;
+
+	private:
+
+		TableFloatingTileBase &owner;
+
+		int row;
+		HiPropertyPanelLookAndFeel laf;
+	};
```

### `hi_core/hi_components/floating_layout/FrontendPanelTypes.cpp`

`initTable()` (~line 1825), after the `Maximum` column:

```diff
 	table.getHeader().addColumn("Min", Minimum, 70, 70, 70);
 	table.getHeader().addColumn("Max", Maximum, 70, 70, 70);
+	table.getHeader().addColumn("", Delete, 28, 28, 28);
 	table.getHeader().setStretchToFitActive(true);
```

Next to the `InvertedButton` implementations (~line 1730):

```diff
+TableFloatingTileBase::DeleteButton::DeleteButton(TableFloatingTileBase &owner_) :
+	owner(owner_)
+{
+	laf.setFontForAll(owner.font);
+
+	addAndMakeVisible(t = new TextButton("X"));
+	t->setLookAndFeel(&laf);
+	t->addListener(this);
+	t->setTooltip("Remove this assignment.");
+	t->setColour(TextButton::buttonColourId, Colour(0x88000000));
+	t->setColour(TextButton::textColourOffId, Colour(0x99ffffff));
+}
+
+void TableFloatingTileBase::DeleteButton::resized()
+{
+	t->setBounds(getLocalBounds().reduced(1));
+}
+
+void TableFloatingTileBase::DeleteButton::setRow(const int newRow)
+{
+	row = newRow;
+}
+
+void TableFloatingTileBase::DeleteButton::buttonClicked(Button *)
+{
+	// removeEntry() triggers updateContent(), which deletes this
+	// component while its click callback is still on the stack,
+	// so the removal must be deferred.
+	Component::SafePointer<TableFloatingTileBase> safeOwner(&owner);
+	auto r = row;
+
+	MessageManager::callAsync([safeOwner, r]
+	{
+		if (safeOwner != nullptr)
+			safeOwner->deleteKeyPressed(r);
+	});
+}
```

`refreshComponentForCell()` (~line 2120), new branch after the `Inverted` branch, before the fallback block that returns `nullptr`:

```diff
+	else if (columnId == Delete)
+	{
+		DeleteButton* b = dynamic_cast<DeleteButton*> (existingComponentToUpdate);
+
+		if (b == nullptr)
+			b = new DeleteButton(*this);
+
+		if (auto root = simple_css::CSSRootComponent::find(*this))
+		{
+			if (css_laf != nullptr && root->css.getWithAllStates(this, simple_css::Selector("button")))
+				b->t->setLookAndFeel(css_laf.get());
+		}
+
+		b->t->setColour(TextButton::buttonColourId, Colours::transparentBlack);
+		b->t->setColour(TextButton::textColourOffId, textColour);
+
+		b->setRow(rowNumber);
+
+		return b;
+	}
```

## Scope / caveats

- **`FrontendMacroPanel` gets the X column too** — it shares `initTable()` and has its own working `removeEntry()` (`FrontendPanelTypes.cpp:1497ff`), so the button works there for free and keeps the two tables consistent. To scope it to MIDI Learn only, add a flag parameter to `initTable()` like the existing `addChannelColumn`.
- **CSS-styled tables:** the `simple_css` branch of `TableFloatingTileBase::resized()` (~line 1952ff) fixes column widths by *index* (button column at index 2, sliders at 3–4) and doesn't know about the new last column. Default-LAF rendering is unaffected; a CSS-styled panel may want a width rule for the X column.
- The X button intentionally reuses `deleteKeyPressed()` rather than calling `removeEntry()` directly, so keyboard DELETE and the button stay one code path.

## Verification

1. Load a project with MIDI-learnable knobs; assign two or three CCs.
2. Open the MIDI Learn panel: each row should show an X button in the last column.
3. Click X on the middle row → that assignment (and only that one) disappears; no crash (this is the `callAsync` re-entrancy check — also click rapidly on several rows in succession).
4. Keyboard DELETE on a selected row still works.
5. FrontendMacroPanel: same checks with macro-assigned parameters.
6. Exported plugin: confirm the X column renders and works in a host (the panel behaves the same in frontend builds).
