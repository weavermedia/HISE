# ai/

AI-assisted design and reference material for this fork. Two kinds of document live here:

- **`specs/`** - reference docs that explain how an existing subsystem works, anchored to `file:line`. Durable; kept accurate as the code moves.
- **`experiments/`** - a log of design, refactor, or feature explorations that were investigated with AI assistance and either abandoned, deferred, or merged with notable caveats. Each file captures one experiment so the same ground is not re-walked later.

## File convention

- One topic per file.
- Filename: `kebab-case-topic.md` (e.g. `shapefx-gain-modulation.md`). The topic should name the subsystem or feature, not the conclusion.
- Include the date at the top.
- Specs: anchor claims to `file:line` and note the target commit.
- Experiments: suggested sections are Goal, What was sketched / tried, Why we did not proceed (or: what we changed), What to use instead, Side effects.

## When to add an experiment

- An idea was explored in enough depth that a design or sketch exists, but the idea was not (or not yet) merged.
- A change was attempted and reverted, and the reasons are not obvious from git history alone.
- A line of investigation hit a structural blocker worth recording, so future work knows the constraint up front.

Trivial bug fixes, routine refactors, and anything fully captured by a commit message do not belong here.

## Index

### specs/

- [specs/svg-to-path-converter-output-formats.md](specs/svg-to-path-converter-output-formats.md) - The four output formats of the SVG To Path Converter (Base64 Path, HiseScript number array, C++ Path String, Base64 SVG): what each encodes, expected input, downstream loading, and the Base64-vs-number-array performance note.

### experiments/

- [experiments/shapefx-gain-modulation.md](experiments/shapefx-gain-modulation.md) - Why ShapeFX's Gain cannot be envelope-modulated, and why PolyshapeFX is the right tool for that use case.
- [experiments/preset-save-workflow.md](experiments/preset-save-workflow.md) - Shipping read-only factory presets now and adding user saving later: folder layout, project settings, save routing, and the always-on Save decision.
- [experiments/persistent-ui-state-appdata.md](experiments/persistent-ui-state-appdata.md) - Persisting a per-plugin UI preference (hide/show keyboard) globally across instances without it entering presets: why not General Settings.xml, and the own-JSON-in-AppData approach.
- [experiments/frontend-au-preset-list.md](experiments/frontend-au-preset-list.md) - Exposing user presets as host programs (AU Presets menu / VST3 program list) via FrontendProcessor's stubbed getNumPrograms/setCurrentProgram; recall-integrity analysis and known gaps.
