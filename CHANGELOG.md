# Changelog

Harsh Vox uses [semantic versioning](https://semver.org). While the plugin is in active revision (0.x):

- **0.MINOR.0**: a round of revisions (new options, module changes, preset changes).
- **0.x.PATCH**: fixes only, no change to sound or parameters.
- **1.0.0**: first stable release. After that, a parameter or preset change that alters saved projects
  bumps MAJOR.

The version lives in one place: `project(HarshVox VERSION x.y.z)` in `CMakeLists.txt`. It feeds the plugin
binary, the version shown in the UI, saved state, and the name of the Windows zip. To release, bump that
line, add a section here, commit, and push a matching tag (`git tag v0.3.0 && git push origin v0.3.0`).
CI refuses a tag that doesn't match the CMake version and uses this file's section as the release notes.

## [0.3.0]

### Changed
- **New interface: military field-hardware styling.**
  - The panel: khaki enamel, black fluted bakelite knobs with painted scales, bat toggle switches, jewel
    lamps, cream VU meters for input and output, a preset drum behind a PROGRAM window, and an engraved
    data plate.
  - Module bays: modules are plug-in units with thumbscrews, a name plate and a pull handle; empty bays
    have blanking plates.
  - Lower section: a rotary MODULE selector, two green-phosphor CRT scopes, and V/F/P classifier lamps.
  - The paint finish uses CC0 photo textures (ambientCG): relief, chips, scratches, grime, fingerprints
    and rust. It is cached per window scale.
- **Resizing:** the panel now scales as one piece with a fixed aspect ratio, from 900 x 561 to
  2440 x 1520 (default 1220 x 760). It no longer reflows.
- Fonts: panel lettering now uses IBM Plex Sans Condensed / Plex Mono (OFL).
- The preset menu has a "Save preset..." item. The STORE button saves too.

### Added
- **Organic Carrier Vocoder: Analyze (Dry / Local)**, default Dry, so existing presets are unchanged. Local
  takes the vowel shape (LPC tract), its loudness and the envelope that drives Density/React from the
  slot's own input, so modules before it shape the result. Dry uses the shared analysis of the plugin
  input, as before. Pitch Lock always uses the shared pitch tracker. Local costs about +0.8% CPU per Carrier.

## [0.2.0]

### Added
- **Input Mode (global): Stereo / Mono, default Stereo.** Stereo keeps left and right separate through
  every slot and the global dry path. Mono sums the input first (the 0.1.0 behaviour). Analysis always
  uses the mono sum. Presets don't change it. Stereo roughly doubles per-module CPU, because each module
  runs once per channel.
- The version number is shown in the UI, next to the latency readout.

### Changed
- **Static Ruin logo** in the header is larger and at full brightness, matching the house layout.
- **Organic Carrier Vocoder: Ping Decay** range is now 0.2–2 ms (was 0.2–20 ms). The old 50% point
  (2 ms) is the new maximum. Default 1 ms (was 2 ms). Saved projects with a decay above 2 ms load at 2 ms.
- **Saliva Clicks** now outputs the clicks alone, so Mix 100% means clicks only and Mix sets the
  voice/click balance. Before, the module added its clicks on top of the voice, so the dry voice was
  always present. *Gagged* and *Swallowed Tongue* were rebalanced to sound as before (Saliva Mix 50%,
  +6 dB on Saliva and on the slot before it). For a 0.1.0 voice + clicks sound, use Mix 50% and Output +6 dB.
- Windows build artifacts and release zips include the version: `HarshVox-<version>-windows-x64-vst3.zip`.

## [0.1.0]

- First release: 13 modules, 8 reorderable slots plus splitter sends, 4 modulation slots, 14 factory
  presets, preset export as code.
