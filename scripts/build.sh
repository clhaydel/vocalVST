#!/usr/bin/env bash
# Configure + build plugin, tests and tools (Release). Usage: scripts/build.sh [extra cmake args]
set -euo pipefail
cd "$(dirname "$0")/.."
# optional local dev-header prefix (machines without root access to install -dev packages)
[ -f "$HOME/.local/sysdeps/env.sh" ] && source "$HOME/.local/sysdeps/env.sh"
git submodule update --init --depth 1 external/JUCE
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build build -j"$(nproc)"
echo "VST3: build/HarshVox_artefacts/Release/VST3/Harsh Vox.vst3"
