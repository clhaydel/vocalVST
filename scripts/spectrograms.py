#!/usr/bin/env python3
"""Render a grid of spectrograms (dry vs every preset) for one test vocal, for visual review.
usage: scripts/spectrograms.py <renders_dir> <test_vocal.wav> <out.png> [seconds]"""
import sys, glob, os
import numpy as np, soundfile as sf
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

renders, src, out = sys.argv[1], sys.argv[2], sys.argv[3]
secs = float(sys.argv[4]) if len(sys.argv) > 4 else 3.0
stem = os.path.splitext(os.path.basename(src))[0]
files = [("DRY", src)] + [(os.path.basename(d), glob.glob(os.path.join(d, stem + "__*.wav"))[0])
                          for d in sorted(glob.glob(os.path.join(renders, "*"))) if glob.glob(os.path.join(d, stem + "__*.wav"))]
cols = 2
rows = (len(files) + 1) // cols
fig, axes = plt.subplots(rows, cols, figsize=(14, 2.3 * rows), squeeze=False)
for ax, (name, f) in zip(axes.flat, files):
    x, sr = sf.read(f)
    if x.ndim > 1: x = x.mean(1)
    x = x[: int(secs * sr)]
    ax.specgram(x + 1e-9, NFFT=1024, Fs=sr, noverlap=768, cmap="magma", vmin=-130, vmax=-20)
    ax.set_ylim(0, 12000)
    r = 20 * np.log10(np.sqrt(np.mean(x ** 2)) + 1e-12)
    ax.set_title(f"{name}   rms {r:.1f} dBFS", fontsize=9)
    ax.tick_params(labelsize=7)
for ax in axes.flat[len(files):]: ax.axis("off")
fig.tight_layout()
fig.savefig(out, dpi=70)
print("wrote", out)
