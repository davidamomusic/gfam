#!/usr/bin/env python3
"""Sanity statistics + spectrogram sheet for build/preset_*.wav."""
import glob, wave, sys
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

files = sorted(glob.glob("build/preset_*.wav"))
fig, axes = plt.subplots(len(files), 2, figsize=(13, 2.2 * len(files)))
names = ["808 KICK", "SEQ BASS", "HATS", "FM TOMS", "MUSICBOX", "ZAP SYNC"]
bad = False
for i, fn in enumerate(files):
    w = wave.open(fn)
    sr = w.getframerate()
    x = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).astype(float) / 32768
    peak = np.abs(x).max()
    rms = np.sqrt((x ** 2).mean())
    clip = (np.abs(x) > 0.98).mean() * 100
    dc = x.mean()
    win = int(sr * 0.01)
    env = np.sqrt((x[: len(x) // win * win].reshape(-1, win) ** 2).mean(1))
    thr = 0.2 * env.max()
    hits = int(((env[1:] > thr) & (env[:-1] <= thr)).sum())
    spec = np.abs(np.fft.rfft(x * np.hanning(len(x))))
    fr = np.fft.rfftfreq(len(x), 1 / sr)
    cent = (spec * fr).sum() / spec.sum()
    ok = np.isfinite(x).all() and peak > 0.05 and clip < 5 and abs(dc) < 0.05 and hits >= 3
    bad |= not ok
    print(f"{names[i]:9s} peak={peak:.2f} rms={rms:.3f} clip={clip:4.1f}% dc={dc:+.3f} "
          f"hits~{hits:2d} centroid={cent:6.0f}Hz  {'OK' if ok else '<-- CHECK'}")
    a, b = axes[i]
    t = np.arange(len(x)) / sr
    a.plot(t, x, lw=0.4); a.set_ylim(-1, 1); a.set_ylabel(names[i], fontsize=8)
    b.specgram(x, NFFT=512, Fs=sr, noverlap=384, cmap="magma", vmin=-110, vmax=-30)
plt.tight_layout()
plt.savefig("build/presets.png", dpi=70)
sys.exit(1 if bad else 0)
