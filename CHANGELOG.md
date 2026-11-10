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
