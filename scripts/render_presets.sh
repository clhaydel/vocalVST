#!/usr/bin/env bash
# Listening-test renders: every factory preset x every WAV in a folder (default test_vocals/),
# written to renders/<Preset>/<vocal>__<Preset>.wav (latency-compensated), plus spectrogram sheets.
# Usage: scripts/render_presets.sh [vocals_dir] [out_dir] [preset name|all]
set -euo pipefail
cd "$(dirname "$0")/.."
IN="${1:-test_vocals}"; OUT="${2:-renders}"; PRESET="${3:-all}"
CLI=build/hv_cli_artefacts/Release/hv_cli
[ -x "$CLI" ] || { echo "build first: scripts/build.sh"; exit 1; }
"$CLI" render --in "$IN" --out "$OUT" --preset "$PRESET"
if python3 -c "import matplotlib, soundfile" 2>/dev/null; then
    mkdir -p "$OUT/_spectrograms"
    for f in "$IN"/*.wav; do
        python3 scripts/spectrograms.py "$OUT" "$f" "$OUT/_spectrograms/$(basename "${f%.wav}").png" 4
    done
fi
