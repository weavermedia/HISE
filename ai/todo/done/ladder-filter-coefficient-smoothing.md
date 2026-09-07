# Ladder filter: per-sample coefficient smoothing to kill modulation clicks

**Date:** 2026-07-28 (diagnosed during a Sublime session - loud clicks on short-attack sounds, "weird noises" during filter-envelope attack/release, mostly gone when the filter is bypassed; SVF clicks much less)
**Target commit:** `5c1bde3eb` (branch `meatbeats`)
**Status:** DONE. Committed on meatbeats as `a1cb6a524` (2026-07-28) after listening tests passed (tried 2 ms, settled on 1 ms). Upstreamed as PR #1010 (https://github.com/christophhart/HISE/pull/1010, branch `fix/ladder-coefficient-smoothing`, cherry-pick `992bfc659` on upstream `7d4faae2f`). The parked .patch was dropped when the doc moved to done (2026-09-07); PR #1010 carries the same diff.

**Affects:** `LadderSubType` (`hi_dsp_library/dsp_basics/MultiChannelFilters.h:510`, `.cpp:865-917`) - Filter modules in `LadderFourPoleLP` mode and the scriptnode `filters.ladder` node (same subtype). No other filter types touched.

## Root cause (two halves)

**1. Filter frequency modulation is stepped at block rate with zero interpolation.**
Mod chains compute per-sample envelope/LFO buffers, but the filter effect samples ONE value per render chunk:

- `VoiceEffectProcessor::renderVoice` chunks voice rendering at `stepSize = 64` (`hi_core/hi_dsp/modules/EffectProcessor.cpp:515`).
- `PolyFilterEffect::applyEffect` calls `modChains[FrequencyChain].getOneModulationValue(startSample)` - one value per 64-sample chunk (`hi_core/hi_modules/effects/fx/Filters.cpp:500`).
- `MultiChannelFilter::update()` applies it as an instantaneous `updateCoefficients` jump (`MultiChannelFilters.cpp:286`). The wrapper's `LinearSmoothedValue frequency` (30 ms) only smooths the BASE frequency - `applyModValue` multiplies the mod value in AFTER the smoother (`:290`), so modulation bypasses smoothing entirely.

With a 1 ms filter-envelope attack (Sublime's stored patch state) note-on is literally: first 64 samples filtered at ~20 Hz (envelope still ~ 0, `FilterLimits::lowFrequency` clamp), then cutoff teleports the entire sweep range in a single sample at the chunk boundary. Decay/release staircase at ~690 Hz update rate (44.1k/64).

**2. The ladder topology amplifies every step.**
`LadderSubType` is a naive forward-Euler one-pole cascade (`cut = jlimit(0, 0.8, 2*pi*f/sr)`, no prewarp) with global feedback whose gain never drops below 0.3 (`res = jlimit(0.3, 4.0, q/2)`). A coefficient jump instantly changes the loop gain around charged filter state -> the loop rings -> click. The SVF (`StateVariableFilterSubType`) is a Zavalishin/TPT zero-delay structure with `tan()` prewarping - the topology stays well-behaved under coefficient jumps, which is exactly why it clicks far less on identical material. So the complaint "the ladder clicks, the SVF doesn't" is structural, not imagined.

Also noticed: `cut`/`res` are uninitialized members (`.h:540`) - first use depends on `updateCoefficients` having run; the patch gives them sane defaults.

## Fix

Give `LadderSubType` per-channel smoothed copies of `cut`/`res` that glide toward the block-rate targets with a ~1 ms one-pole inside `processSample`, snapped in `reset()` so a fresh voice doesn't glide from stale state:

```cpp
// updateCoefficients - cut/res become targets, plus:
smoothAlpha = 1.0f - std::exp(-1.0f / (0.001f * (float)sampleRate)); // ~1 ms glide

// processSample - glide, then use the smoothed values:
const float c = cutS[channel] += smoothAlpha * (cut - cutS[channel]);
const float r = resS[channel] += smoothAlpha * (res - resS[channel]);
```

Design choices:

- **Per-channel smoother state** (`cutS[NUM_MAX_CHANNELS]`): `processSamples` iterates channels in the OUTER loop, so shared state would advance L before R and the channels would diverge. Per-channel copies with identical targets follow identical trajectories.
- **Subtype-local, not wrapper-level.** Interpolating the mod value in `MultiChannelFilter::update()` or shrinking the 64-sample step would touch every filter type in every project - much bigger blast radius. The ladder is where the topology hurts; fix it there.
- **~1 ms time constant** is the tuning knob: down to 0.5 ms if fast attacks sound audibly rounded, up to 2 ms if any click survives. At note-on the cutoff now glides up over a few ms instead of teleporting - which is also what an analog ladder does (a capacitor voltage can't jump).
- **Cost:** two multiply-adds per sample per channel; one `std::exp` per `updateCoefficients` call (~690 Hz per voice). Negligible.
- **NOT bit-exact** for existing projects: the glide replaces the jump even for plain knob moves. Audibly identical for static settings (the smoother converges in ~5 ms and then `cutS == cut` exactly, so the steady-state inner loop is bit-identical).

## Interaction with the feedback-saturation patch

`ai/todo/ladder-filter-feedback-saturation.patch` (reverted, undecided) touches the SAME `processSample` lines - whichever lands second needs a trivial rebase. They compose cleanly: the tanh clips `resoclip`, this patch smooths the `c`/`r` it multiplies with. The saturation patch would additionally bound step transients in the loop, but it is a character change, not a click fix - this one is the click fix.

## Testing

1. Sublime, ladder mode, short amp attack + 1 ms filter-env attack, resonance mid: the note-on click must be gone. (Before the patch, a dry render shows click transients spaced at 64-sample boundaries - ~1.45 ms at 44.1k - that spacing is the fingerprint of the mechanism.)
2. Filter-env decay/release sweeps at high resonance: the stepping/"weird noises" must be gone.
3. Static cutoff/res, no modulation: A/B against unpatched build - should be audibly identical after the first ~5 ms.
4. Fast knob sweeps by hand: should sound the same or smoother (base freq was already 30 ms-smoothed).
5. scriptnode `filters.ladder` (uses `processFrame` -> same `processSample`): confirm no regression; note its own `Smoothing` parameter (seconds!) sits in the wrapper and is unaffected.
