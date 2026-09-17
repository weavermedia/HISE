# HISE commercial licence and the "prototype in HISE, ship in JUCE" route

**Date:** 2026-09-04
**Status:** Research note. No code. Not legal advice - the contract text below was read by Claude, the conclusions are a reading of it, and anything that matters commercially should be confirmed with Christoph or a lawyer.

**Sources read:**

- `license.txt` in this repo (GPL v3 header with the "commercial licenses available on request" pointer).
- https://hise.dev/ (public licensing summary and pricing).
- https://hise.dev/hise_license_preview.pdf - the "HISE 3.x.x License Agreement" (10 pages, GPL v3 reproduced as Annex 3). Dan notes the 3.x.x version number is stale but the terms are what he signed.
- Forum threads: https://forum.hise.audio/topic/2386/license/33 and https://forum.hise.audio/topic/7807/hise-vst-commercial-license (general only; no contract terms in either).

## Context

Dan has held the HISE commercial licence for over a year without having released a HISE-built product. The plan was three free, closed-source plugins built with HISE as a marketing boost, followed by paid plugins built in pure JUCE. The problem realised on 2026-09-04: the subscription must stay active for as long as any closed-source HISE-built product is distributed, and free plugins are effectively never withdrawn. So the free plugins alone would keep the subscription running indefinitely.

The question was therefore: can HISE be used purely as a prototyping tool, with the resulting project handed to Claude to reimplement in plain JUCE, and does the licence allow that?

## Short answer

Yes. The licence attaches to HISE's source code inside a distributed binary, and to nothing else. Prototyping in the IDE is explicitly GPL-covered development use. A JUCE plugin that contains no HISE code needs no HISE licence at any point, before or after cancelling. The project folder (scripts, XML, assets) is Dan's own work and data, and is a sufficient specification for a reimplementation without opening any HISE source file.

## What the contract actually says

Section and annex references are to the PDF above.

- **Subject matter.** Annex 1: "This agreement covers the rights of use of the HISE code base in a proprietary derivative." Annex 3: "any product that includes any part of the HISE codebase constitutes a covered work within the scope of the GPL v3". That is the whole scope. There is no non-compete, no derived-knowledge clause, no restriction on tooling, and nothing about what may be built after leaving.
- **Development use is GPL, not commercial.** Annex 2, last sentence: "The usage of HISE during the development phase is being covered by the GPL v3 license." So using the IDE to prototype needs no subscription.
- **When the subscription must exist.** Annex 2: the agreement "must be concluded as soon as a single proprietary product using the HISE codebase is being published and must be kept active for as long as any product using the HISE codebase is being distributed."
- **Termination.** Section C.5 and Annex 2: on termination "the sale of the products equipped with the Software must either be halted or meet the requirements of the alternative right of use" (GPL). So every closed-source HISE product must be off sale, or open-sourced, before the subscription ends. Term is rolling three-month blocks with four weeks' notice, billed monthly via FastSpring.
- **Confidentiality.** Section H requires "strictest secrecy" about the other party's trade secrets and knowledge, surviving termination. H.3 carves out "the usage of the Software". The public code base is GPL, so contains no trade secrets. The one non-public component is the copy protection module (Annex 1 point 4, "not part of the public HISE code base"). If Dan holds that module: do not port it, do not paste it into an AI session.
- **hi_backend is excluded** from proprietary use regardless (Annex 1). Irrelevant to a port, noted for completeness.
- **Pricing tier inconsistency.** Section C.2 measures revenue "regardless of whether this revenue is related to the Software or not"; Annex 2 says "the sum of all revenues that is being generated using products based on HISE". If JUCE-only products sell alongside HISE ones near the 50,000 EUR threshold, the two readings give different tiers. Ask Christoph if it ever matters.
- **Pricing (Annex 2 and hise.dev):** INDIE 600 EUR/year up to 50,000 EUR annual revenue, PRO 3,600 EUR/year above. Company-wide, unlimited products while active. JUCE commercial licence required separately.

## The three piles in a HISE workflow

1. **Dan's own material.** HISEScript, scriptnode graph XML, SNEX, custom C++ node logic, UI layout XML, presets, images, fonts, audio, samples, sample maps. Dan's copyright (hise.dev: "You will keep the copyright of any code you write"). Porting these needs nobody's permission.
2. **HISE's own source.** hi_core, hi_dsp_library, stock scriptnode nodes, the sampler, modulation system, CSS engine, etc. Christoph's copyright under GPL v3. Copying or closely paraphrasing any of it into a JUCE plugin makes that plugin a derivative work, which then needs GPL release or a live HISE subscription.
3. **Third-party libraries bundled in HISE.** Per Annex 4: JUCE (GPL/commercial), dywapitchtrack (MIT), FFTConvolver (MIT); optional IPP (proprietary), RLottie (MIT). Also in this fork: Faust (LGPL), Loris (GPL), RTNeural (BSD), melatonin_blur (MIT). Same analysis per library if any are pulled into the port.

## Is the project folder all Dan's?

Yes, with two exceptions.

| Folder / file | Status |
|---|---|
| `Scripts/` | Dan's. Calling the HISE API from a script is interface use, not copying HISE. |
| `XmlPresetBackups/`, `UserPresets/`, `project_info.xml` | Dan's data in HISE's schema. GPL section 2 (Annex 3): output of a covered work is covered "only if the output, given its content, constitutes a covered work". A tree of Dan's settings is not HISE code. |
| `DspNetworks/` | Dan's topology and parameter values. Node type names reference HISE implementations but do not contain them. |
| `Images/`, `AudioFiles/`, `Samples/`, `SampleMaps/`, fonts | Dan's, or third-party under their own licences. Check font licences permit plugin embedding (applies whichever framework ships). |
| `AdditionalSourceCode/` | Dan's logic. Compiled result is a HISE derivative because it subclasses HISE base classes; the algorithm is Dan's and can be re-expressed against JUCE. |
| `Binaries/` | **Not Dan's.** Generated Projucer project and wrapper source are HISE and JUCE code. Ignore entirely for a port. |
| Anything pasted from HISE docs, snippets, example projects | **Christoph's.** Boilerplate interface scripts, LookAndFeel snippets from the docs, example-project code. Usually a few lines; a spec-based rewrite makes it moot. Do not treat as Dan's if the project is ever open-sourced. |

## How to do the port cleanly

- Hand over the project folder (minus `Binaries/`) and a behavioural description of what the prototype does. Ask for a JUCE implementation of that behaviour.
- Do not hand over HISE module sources and ask for a "pure JUCE version" of them. Copyright covers expression, not ideas. Reimplementing behaviour from a spec is clean; paraphrasing hi_dsp_library is not.
- Concrete example: a scriptnode graph with a stock ladder filter node. Topology, ranges and modulation routing are Dan's. The ladder implementation is HISE's. The port keeps the topology and swaps the node for `juce::dsp::LadderFilter` or a filter written fresh.
- UI: interface XML gives layout, sizes, colours. LookAndFeel scripts use a Graphics API that maps almost line for line onto `juce::Graphics`. Both are Dan's.
- **Fidelity caveat (not a legal one).** Some of the sound lives in HISE's implementation choices: its ladder filter, oscillator anti-aliasing, envelope curve shapes, parameter smoothing. A fresh implementation will differ slightly. Matching by ear or by measurement (tune until the response lines up) is legitimate black-box work. Matching by reading HISE source and paraphrasing crosses the line. If exactness matters for a particular node, say so and take the measurement route.

## Options for the three free plugins (position as of 2026-09-04)

- **Closed source, built with HISE (chosen for now).** Sublime ships in HISE as announced (a "coming soon" email went to the 45k list in late August 2026), Wasapus and WhatThe follow. The subscription runs for as long as any of them is distributed. Cost is the INDIE tier, but note C.2 measures the tier on total company revenue "regardless of whether this revenue is related to the Software or not", so free plugins can still land in the PRO tier once Meat Beats as a whole passes 50,000 EUR.
- **Replace with JUCE-built V2 versions later (the realistic exit).** A V2 does not have to sound identical and does not have to read HISE state. Give it a new plugin code so V1 and V2 coexist on users' machines: old sessions keep loading the already-installed V1 binary, new work uses V2, and no importer or parameter-ID matching is needed. Once V2 is out, withdraw every V1 download, give the four weeks' notice inside the current three-month term, and the subscription ends. Start the JUCE side with a small plugin from scratch, not a port of a finished HISE project.
- **Reimplement in JUCE as an in-place update (rejected).** Exact sound replication is a product bar a clean-room reimplementation is unlikely to clear, and an in-place update implies session compatibility, which needs a HISE-state importer and parameter IDs observed from the installed build. Too much for the release window. `~/Code/sublime-juce/PORT-PROMPT.md` exists from this exploration and can sit unused.
- **Withdraw the free plugins and stop paying.** Always available, at the cost of community standing.
- **Open the source under GPL and stop paying (very remote possibility, not planned).** The contract names it as the second exit route (C.5, Annex 2: cease distribution "or fulfil the requirements of the GPL license") and both the HISE and JUCE subscriptions would end. Dan's position: open-sourcing Meat Beats products moves the company away from "serious tools for producers" towards a hobbyist free-for-all that is not aligned with his intentions. Kept on record only. If it ever came up: copyright in audio, images and presets stays with Dan regardless; Christoph has said on the forum that the GPL "won't interfere with your samples" provided they are licensed "permissively enough so that people can use your GPL licensed plugin"; hise.dev says assets "required to compile your project must be available" to binary recipients; and because Sublime embeds its assets (`EmbedAudioFiles`/`EmbedImageFiles` = Yes) the conservative GPL reading (FAQ: "included in the same executable file ... definitely combined in one program") is that embedded assets must be provided under terms allowing redistribution with the plugin. Externally loaded assets would not be affected.

## Next step

Ship Sublime in HISE. Learn JUCE on a small from-scratch plugin. Revisit the V2 route when the JUCE side is comfortable.
