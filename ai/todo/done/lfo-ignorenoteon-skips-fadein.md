# LFO IgnoreNoteOn also disables FadeIn — fade should still retrigger per note

**Date:** 2026-07-04 (diagnosed in Sublime — "LFO retrigger" settings toggle)
**Target commit:** `598670c6d` (branch `meatbeats`)
**Status:** Done - landed in meatbeats `3576607bc` (fork decision: option 1, no new params), fade verified working; upstream options list retained in case it ever goes to Christoph
**Affects:** `LfoModulator` with `IgnoreNoteOn=1` and `FadeIn > 0`

## Symptom

With `IgnoreNoteOn` enabled (free-running LFO), the `FadeIn` parameter appears
dead: the fade ramp runs exactly once — at the moment `IgnoreNoteOn` is set to 1
(its setter calls `resetPhase()` as a side effect) — and every note after that
plays with the LFO at full depth immediately. Users who want a free-running
phase but still expect the fade to ease the LFO in on each note get no fade at
all, with no hint in the UI that the two parameters are mutually exclusive.

## Root cause

The fade-in reset is coupled to the phase reset — `resetFadeIn()` is only
called from inside `resetPhase()`:

`hi_core/hi_modules/modulators/mods/LFOModulator.cpp:765`:

```cpp
void LfoModulator::resetPhase()
{
    uptime = phaseOffset * (double)SAMPLE_LOOKUP_TABLE_SIZE;
    ...
    resetFadeIn();   // attackValue = 0.0f — the ONLY place the ramp restarts
}
```

and on note-on, `resetPhase()` is guarded by `ignoreNoteOn`
(`LFOModulator.cpp:794`):

```cpp
if(m.isNoteOn())
{
    if((legato == false || keysPressed == 0) && !ignoreNoteOn)
    {
        resetPhase();

        for (auto& mb : modChains)
            mb.startVoice(0);

        frequencyModulationValue = modChains[FrequencyChain].getConstantModulationValue();
        calcAngleDelta();
    }

    keysPressed++;
}
```

Once `attackValue` ramps to 1.0 it holds there (`calculateNewValue`,
`LFOModulator.cpp:567`) until the next `resetFadeIn()` — which never comes.

Note the same guard also skips `mb.startVoice(0)` for the LFO's internal
intensity/frequency chains, so the usual workaround — an envelope inside the
LFO's Intensity chain to fake the fade — is also dead when `IgnoreNoteOn` is
on. There is no project-side escape hatch.

## Proposed fix

Decouple the two resets: when note-on is ignored for *phase*, still restart the
*fade* ramp (respecting the existing legato guard):

```diff
 	if(m.isNoteOn())
 	{
-		if((legato == false || keysPressed == 0) && !ignoreNoteOn)
+		if(legato == false || keysPressed == 0)
 		{
-            resetPhase();
-            
-			for (auto& mb : modChains)
-				mb.startVoice(0);
-
-			frequencyModulationValue = modChains[FrequencyChain].getConstantModulationValue();
-			calcAngleDelta();
+			if(!ignoreNoteOn)
+			{
+				resetPhase();
+
+				for (auto& mb : modChains)
+					mb.startVoice(0);
+
+				frequencyModulationValue = modChains[FrequencyChain].getConstantModulationValue();
+				calcAngleDelta();
+			}
+			else
+				resetFadeIn();
 		}
 
 		keysPressed++;
 	}
```

## Open question for Christoph

This changes existing behaviour: anyone relying on `IgnoreNoteOn` making the
LFO *fully* inert to note-ons (e.g. a constant free-running wobble that must
not dip on new notes) would now hear the fade re-run per note whenever
`FadeIn > 0`. Options, in order of preference:

1. **Just change it.** Patches with `FadeIn=0` are unaffected (`resetFadeIn`
   with `attack == 0.0f` snaps `attackValue` back to 1.0 on the next sample —
   inaudible). The compat risk is only patches that have `FadeIn > 0` *and*
   `IgnoreNoteOn=1` together: today they get full depth (the fade value is
   silently ignored), after the fix they get a per-note fade. Arguably those
   patches carry a stale fade value the author never heard, so honouring it is
   closer to intent than ignoring it — but it IS an audible change.
2. **Gate it behind a new parameter** (e.g. `RetriggerFadeIn`, default off) if
   1 is too risky. Only meaningful when `IgnoreNoteOn=1` (with it off, fade
   retriggers anyway), so the effective rule is
   `fadeResets = !ignoreNoteOn || retriggerFadeIn` - the new param can only add
   behaviour, never be contradicted by another write.
3. **Two orthogonal params** (`ResetPhaseOnNoteOn` / `ResetFadeOnNoteOn`, both
   default true, `IgnoreNoteOn=1` overrides both to false). Same perfect
   storage back-compat as 2, and arguably the cleaner mental model if designing
   from scratch - but rejected for now: the live override means a script that
   actively sets `ResetFadeOnNoteOn=true` has it silently swiped by an
   `IgnoreNoteOn` write elsewhere, which recreates the silently-dead-parameter
   trap this todo exists to fix, one level up. It also duplicates state
   (`ResetPhaseOnNoteOn=false` means the same as `IgnoreNoteOn=1`'s phase half)
   and needs two new editor toggles. The variant that fixes the swipe - making
   `IgnoreNoteOn` a deprecated alias that *writes* the two new params
   (last-write-wins, no runtime precedence) - brings patch-restore ordering
   problems (old patches store `IgnoreNoteOn=1` next to default-true new
   params; the result depends on attribute restore order).

## History note

The coupling is accidental, not designed: `FadeIn`/`attackValue` was in the
initial commit (`adc9cc4af`, 2016), and `IgnoreNoteOn` was added seven years
later (`bdaa004e8`, 2023-01) as a guard wrapped around the whole existing
note-on block - which contained `resetPhase()`, whose body calls
`resetFadeIn()`. The fade-kill is collateral from where the guard landed,
which supports honouring the fade value over preserving the accident.

## Fork direction (Dan, 2026-07-04)

Decision for `meatbeats`: **option 1, no new parameters.** The fade always
retriggers per note, regardless of `IgnoreNoteOn`; the param's name becoming
slightly misleading (it now means "ignore note-on for *phase* only") is
accepted.

Why no gating param: a `RetriggerFade`-style boolean is redundant with
`FadeIn` itself - `FadeIn=0` already means "no fade", so anyone who does not
want the per-note fade just sets the time to zero. A nonzero fade time the
author dialled in is a fade they want to hear; there is no state left for a
second parameter to express. (An earlier idea to replace `IgnoreNoteOn` with
orthogonal `RetriggerPhase`/`RetriggerFade` params was dropped for the same
reason plus the migration cost documented in option 3.)

So the fork change is exactly the proposed diff above. The upstream options
list stays as-is in case this ever goes to Christoph.

## Verification

1. `IgnoreNoteOn=1`, `FadeIn=2000`: each note-on should restart the depth ramp
   (LFO eases in over 2s) while the LFO *phase* keeps running — play two
   staccato notes a quarter-cycle apart and confirm the waveform position
   differs but both fade in.
2. `IgnoreNoteOn=1`, `FadeIn=0`: unchanged — full depth immediately, no dip on
   note-on.
3. `IgnoreNoteOn=0`: unchanged — phase and fade both reset per note (or per
   first note with `Legato=1`).
4. Legato: with `Legato=1` and `IgnoreNoteOn=1`, overlapping notes must NOT
   re-run the fade; only the first note of a legato group does.

## Origin

Found in Sublime: the Settings-panel "LFO retrigger" toggle writes
`IgnoreNoteOn` (`Interface.js` `onbtnLfoRetriggerControl`), and turning
retrigger off silently killed the Motion section's Fade knob.
