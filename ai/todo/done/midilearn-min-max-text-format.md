# MIDI Learn panel Min/Max cells: initial text bypasses the ValueToTextConverter

**Date:** 2026-07-04
**Target commit:** `bada887e9` (branch `meatbeats`)
**Status:** Done - landed in meatbeats `598670c6d`, verified in a Debug build; upstreamed as [christophhart/HISE#994](https://github.com/christophhart/HISE/pull/994)
**Affects:** `MidiLearnPanel` and `FrontendMacroPanel` (both derive from `TableFloatingTileBase`)

## Symptom

Assign a MIDI CC (e.g. mod wheel) to a knob whose `Mode` is `NormalizedPercentage`, then open the MIDI Learn floating tile:

1. The Min and Max cells display the raw normalized values `0.0` and `1.0`.
2. Type `0.5` RETURN into Min, expecting "half". The cell now shows **`1%`**.

The field looks like it switches format on first edit. It doesn't — the typed input was always parsed in the knob's *display* units (percent here), so `0.5` meant 0.5% → 0.005, rendered rounded as `1%`. Only the **initial** paint is wrong: it shows the raw value instead of `0%` / `100%`, which is what misleads the user into typing normalized values.

The same applies to any converter mode: a Decibel knob's cells parse typed text as dB, Frequency as Hz/kHz, etc. Only the first paint is raw.

## Root cause

`TableFloatingTileBase::refreshComponentForCell`,
`hi_core/hi_components/floating_layout/FrontendPanelTypes.cpp:2106-2111`:

```cpp
slider->setRowAndColumn(rowNumber, (ColumnId)columnId, value, fullRange);   // 2106: setValue() happens here

ValueToTextConverter vtc = getValueToTextConverter(rowNumber);

slider->slider->textFromValueFunction = vtc;                                // 2110: converter assigned AFTER
slider->slider->valueFromTextFunction = vtc;
```

- `setRowAndColumn` (`FrontendPanelTypes.cpp:1756`) calls `slider->setValue(value, dontSendNotification)` while `textFromValueFunction` is still null on a freshly created cell component, so the text box renders with JUCE's default numeric formatting (raw `0.0` / `1.0`).
- Assigning `Slider::textFromValueFunction` afterwards does **not** refresh the text box.
- The first user edit goes through `sliderValueChanged` → `setValue` with the converter now in place, so from then on the cell shows converted text (`1%`), which reads as a format switch.

The converter itself is correct: it's the automated component's converter, captured at learn time via `MacroControlledObject::addMidiControlledParameter` (`hi_core/hi_core/MacroControlledComponents.cpp:293` / `:331`) and returned per-row by `MidiLearnPanel::getValueToTextConverter` (`FrontendPanelTypes.h:883`). For `NormalizedPercentage`, `getValueForText("0.5")` = `0.5 * 0.01` (`hi_tools/hi_tools/MiscToolClasses.h:3179`) and `getTextForValue(0.005)` = `"1%"` (`MiscToolClasses.h:3111`).

## Fix

Refresh the text box after the converter functions are assigned:

```diff
 		slider->setRowAndColumn(rowNumber, (ColumnId)columnId, value, fullRange);

 		ValueToTextConverter vtc = getValueToTextConverter(rowNumber);

 		slider->slider->textFromValueFunction = vtc;
 		slider->slider->valueFromTextFunction = vtc;
+
+		slider->slider->updateText();
```

`Slider::updateText()` is public (`JUCE/modules/juce_gui_basics/widgets/juce_Slider.h:843`).

Note: merely moving the converter assignment above `setRowAndColumn` is **not** sufficient — `Slider::setValue` skips the text refresh when the new value equals the current one (e.g. Min = 0.0 on a freshly constructed slider), so the explicit `updateText()` is the robust fix.

Unused rows are unaffected: their `ValueToTextConverter` is default-constructed (`active == false`), so `getTextForValue` falls back to plain numbers (`hi_tools/hi_tools/MiscToolClasses.cpp:3958`).

## Verification

1. Load a project with a `NormalizedPercentage` knob exposed to MIDI learn; assign CC1.
2. Open the MIDI Learn panel: Min/Max should now read `0%` / `100%` immediately (before: `0.0` / `1.0`).
3. Type `50` RETURN into Min → `50%`.
4. Check the FrontendMacroPanel the same way with a macro-assigned parameter.
5. Regression: rows without an assignment should still show plain numeric text.
