# ai/todo/done/

Completed todos from `ai/todo/`. A todo moves here once its fix or feature has been applied, built, and verified - the diagnosis and design reasoning stay useful after the fact (why the fix took the shape it did, traps like re-entrancy, verification recipes), which is why these are kept instead of deleted.

Reading caveats:

- **`file:line` anchors and the "Target commit" header refer to the pre-fix tree.** They locate where the problem *was*; the current code has moved by at least the fix itself.
- The **Status** line of each doc records where the change landed (commit on `meatbeats`, plus the upstream PR if one was opened) and any deviation from the doc's proposed diff.

## Index

- [midilearn-min-max-text-format.md](midilearn-min-max-text-format.md) - MIDI Learn / macro panel Min-Max cells painted raw normalized values until first edit because the ValueToTextConverter was assigned after setValue(). Fixed with Slider::updateText() in meatbeats `598670c6d`; upstreamed as [christophhart/HISE#994](https://github.com/christophhart/HISE/pull/994).
- [midilearn-remove-assignment-button.md](midilearn-remove-assignment-button.md) - Per-row X button to remove a MIDI Learn / macro assignment, with the callAsync re-entrancy trap. Landed in meatbeats `2b615d9cb` (with a ShapeButton/closeIcon deviation from the proposed diff); deliberately kept fork-only, not upstreamed.
- [lfo-ignorenoteon-skips-fadein.md](lfo-ignorenoteon-skips-fadein.md) - LFO IgnoreNoteOn also killed FadeIn (fade reset only lived inside resetPhase, which note-on skipped), so free-running LFOs never faded per note. Decoupled the resets in meatbeats `3576607bc`; fork decision was option 1 (fade always retriggers, no new params, accept the IgnoreNoteOn name drift), with the design discussion and rejected alternatives kept in the doc. Fork-only for now; the doc's upstream options list stands if it ever goes to Christoph.
