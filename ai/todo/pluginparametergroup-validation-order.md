# pluginParameterGroup validated at XML restore, before the script registers group names

**Date:** 2026-07-04 (diagnosed 2026-06, Sublime parameter groups)
**Target commit:** `bada887e9` (branch `meatbeats`)
**Status:** TODO — not yet applied
**Affects:** editor only (`#if USE_BACKEND`); every project using `pluginParameterGroup` XML attributes + `setPluginParameterGroupNames`

## Symptom

Every script compile floods the console with `<group> is not a valid group name` — one warning per component carrying `pluginParameterGroup="..."` in the UI XML (Sublime: 20 per compile) — even though the script registers exactly those names via `UserPresetHandler.setPluginParameterGroupNames([...])`. Runtime behaviour is correct (DAW grouping works); only the validation is wrong. There is **no order of operations a project can choose** to avoid it: XML restore always precedes script execution.

## Root cause

- The check runs in the property setter during component construction / XML restore: `ScriptingApiContent.cpp:771-780` calls `checkPluginParameterGroupName(groupName)` and logs on failure (`:776`).
- The validator (`hi_core/hi_core/MainController.h:972`) tests against `pluginParameterGroups`, which is an empty `StringArray` until `ScriptUserPresetHandler::setPluginParameterGroupNames` (`hi_scripting/scripting/api/ScriptExpansion.cpp:653`) runs from the script body — which is always *after* XML restore.

```cpp
Result checkPluginParameterGroupName(const String& possibleName) const
{
	if(pluginParameterGroups.contains(possibleName) || possibleName.isEmpty())
		return Result::ok();

	return Result::fail(possibleName + " is not a valid group name");
}
```

## Fix options (recommend #1 for the fork)

1. **Skip the check while no groups are registered** — one-line guard at `MainController.h:974`:
   ```diff
   -		if(pluginParameterGroups.contains(possibleName) || possibleName.isEmpty())
   +		if(pluginParameterGroups.isEmpty() || pluginParameterGroups.contains(possibleName) || possibleName.isEmpty())
   ```
   Con: a project that never calls `setPluginParameterGroupNames` loses typo detection — but such projects get no meaningful validation today either (every name fails), so nothing real is lost.

2. **Defer validation until after `onInit`** — move the check into a post-init pass over all components, once `pluginParameterGroups` is populated. Cleanest semantics (also catches script-side `comp.set(...)` typos), but needs a new lifecycle hook.

3. **Register groups before XML restore** — a prelude-level API callable before component construction. Largest change.

Rejected project-side workaround: strip all `pluginParameterGroup` attributes from the XML and assign from script after registration (~20 lines of mapping + two-place edits per new parameter — too invasive for editor-only noise).

## Verification

1. Load Sublime in the HISE editor, recompile: console should show zero "is not a valid group name" warnings.
2. Typo check (with fix #1): after `setPluginParameterGroupNames` has run, script-set a bogus group on a component — warning should still fire.
3. Exported plugin: unaffected either way (check is `USE_BACKEND`-gated).
