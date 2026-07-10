# ai/

AI-assisted design and reference material for this fork. Three kinds of document live here:

- **`specs/`** - reference docs that explain how an existing subsystem works, anchored to `file:line`. Durable; kept accurate as the code moves.
- **`experiments/`** - a log of design, refactor, or feature explorations that were investigated with AI assistance and either abandoned, deferred, or merged with notable caveats. Each file captures one experiment so the same ground is not re-walked later.
- **`todo/`** - diagnosed bugs or improvements with a root cause and a proposed fix that has not been applied yet. Each file should anchor the diagnosis to `file:line`, include the proposed diff, and a verification recipe. Once applied and verified, move the file to `todo/done/` and update its Status line with the landing commit (see [todo/done/README.md](todo/done/README.md)).

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

### todo/

- [todo/scriptpanel-bordersize-default.md](todo/scriptpanel-bordersize-default.md) - ScriptPanel with no borderSize attribute silently paints a 2px border; one-line default change with a behaviour-change caveat.
- [todo/simple-css-var-shorthand-color-suffix.md](todo/simple-css-var-shorthand-color-suffix.md) - `background: var(--x)` stored without the `-color` suffix, renders transparent/white; five-line fix in getTokenSuffix, plus the related `*`-rule-vars-invisible-to-popup-selectors cascade bug.
- [todo/pluginparametergroup-validation-order.md](todo/pluginparametergroup-validation-order.md) - "is not a valid group name" console flood on every compile because XML restore validates before the script registers group names; one-line empty-list guard.
- [todo/loaduserpreset-extension-autoappend.md](todo/loaduserpreset-extension-autoappend.md) - Engine.loadUserPreset's documented ".preset" auto-append is an inverted-condition no-op, so getUserPresetList() output silently fails to load.
- [todo/markdown-ordered-list-renumbering.md](todo/markdown-ordered-list-renumbering.md) - Ordered lists ignore literal numbers and renumber from 1 per blank-line-separated block (remaining half; digit-eating half fixed in fc8da09a7).

### todo/done/

Completed todos, indexed in [todo/done/README.md](todo/done/README.md).

### experiments/

- [experiments/shapefx-gain-modulation.md](experiments/shapefx-gain-modulation.md) - Why ShapeFX's Gain cannot be envelope-modulated, and why PolyshapeFX is the right tool for that use case.
- [experiments/preset-save-workflow.md](experiments/preset-save-workflow.md) - Shipping read-only factory presets now and adding user saving later: folder layout, project settings, save routing, and the always-on Save decision.
- [experiments/persistent-ui-state-appdata.md](experiments/persistent-ui-state-appdata.md) - Persisting a per-plugin UI preference (hide/show keyboard) globally across instances without it entering presets: why not General Settings.xml, and the own-JSON-in-AppData approach.
- [experiments/frontend-au-preset-list.md](experiments/frontend-au-preset-list.md) - Exposing user presets as host programs (AU Presets menu / VST3 program list) via FrontendProcessor's stubbed getNumPrograms/setCurrentProgram; recall-integrity analysis and known gaps.
- [experiments/velocity-toggle-in-component-popup.md](experiments/velocity-toggle-in-component-popup.md) - Proposed MPE-style "Velocity Control" toggle in the component right-click popup; not proceeding – the matrix-modulation popup (Assign/Remove sources + setEditCallback custom items) already covers it natively for matrix targets.
