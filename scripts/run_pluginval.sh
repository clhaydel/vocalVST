#!/usr/bin/env bash
# Host validation with pluginval (strictness 8). Downloads pluginval if needed; uses xvfb-run when headless.
set -euo pipefail
cd "$(dirname "$0")/.."
PV_DIR=build/pluginval
if [ ! -x "$PV_DIR/pluginval" ]; then
    mkdir -p "$PV_DIR"
    curl -sfL -o "$PV_DIR/pv.zip" https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_Linux.zip
    unzip -o -q "$PV_DIR/pv.zip" -d "$PV_DIR"
fi
PLUGIN="build/HarshVox_artefacts/Release/VST3/Harsh Vox.vst3"
RUN=()
[ -z "${DISPLAY:-}" ] && command -v xvfb-run >/dev/null && RUN=(xvfb-run -a -s "-screen 0 1600x1000x24")
"${RUN[@]}" "$PV_DIR/pluginval" --strictness-level 8 --validate-in-process --timeout-ms 600000 "$PLUGIN"
