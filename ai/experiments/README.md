# ai/experiments

A log of design, refactor, or feature explorations that were investigated with AI assistance and either abandoned, deferred, or merged with notable caveats.

Each file captures one experiment so the same ground is not re-walked later.

## When to add a file here

- An idea was explored in enough depth that a design or sketch exists, but the idea was not (or not yet) merged.
- A change was attempted and reverted, and the reasons are not obvious from git history alone.
- A line of investigation hit a structural blocker worth recording, so future work knows the constraint up front.

Trivial bug fixes, routine refactors, and anything fully captured by a commit message do not belong here.

## File convention

- One experiment per file.
- Filename: `kebab-case-topic.md` (e.g. `shapefx-gain-modulation.md`). The topic should name the subsystem or feature, not the conclusion.
- Suggested sections: Goal, What was sketched / tried, Why we did not proceed (or: what we changed), What to use instead, Side effects.
- Include the date at the top.

## Index

- [shapefx-gain-modulation.md](shapefx-gain-modulation.md) - Why ShapeFX's Gain cannot be envelope-modulated, and why PolyshapeFX is the right tool for that use case.
