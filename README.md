# DYSEKT

**A non-destructive, time-stretching sample slicer instrument plugin for Windows (VST3).**

DYSEKT is a fork of [INTERSECT](https://github.com/tucktuckg00se/INTERSECT) by tucktuckg00se, extended and modified with the assistance of AI (Claude by Anthropic). It retains the full core engine of INTERSECT while adding a reworked GUI and expanded DAW integration features.

> INTERSECT is licensed under the GNU General Public License v3.0. DYSEKT inherits that licence — source code is open and free to modify and redistribute under the same terms.

---

## What DYSEKT adds over INTERSECT

| Feature | INTERSECT | DYSEKT |
|---|---|---|
| Platforms | VST3 / AU / Standalone | VST3 (Windows) |
| Logo bar | Part of controls bar | Separate dedicated bar |
| MIDI keyboard panel | ✗ | ✓ Toggleable 2-octave piano with transpose and per-slice MIDI note highlighting |
| File browser panel | ✗ | ✓ Toggleable embedded browser — double-click to load |
| Waveform rendering | Flat solid fill | ✓ Switchable — original hard mode or soft gradient + glowing outline |
| Slice overlap prevention | ✗ | ✓ Slices cannot be dragged to physically overlap |
| MIDI Learn context menu | Appears at fixed position | ✓ Appears at mouse cursor |
| DAW Quick Controls | ✗ | ✓ All knobs fire begin/end gesture — assignable in Cubase and other hosts |
| MIDI Learn (host-side) | ✗ | ✓ Touch any knob to assign a MIDI CC directly from your DAW |
| Slice Start parameter | Internal only | ✓ Exposed as automatable DAW parameter (normalised 0–1) |
| Slice End parameter | Internal only | ✓ Exposed as automatable DAW parameter (normalised 0–1) |
| Slice Start/End automation | ✗ | ✓ DAW automation applied live to selected slice during playback |
| Settings persistence | Scale + theme | ✓ Scale + theme + waveform style |

---

## Features inherited from INTERSECT

- Non-destructive sample slicing — load one sample per instance, place slices freely
- Drag-and-drop sample loading (WAV, OGG, AIFF, FLAC, MP3)
- Three time/pitch algorithms — Repitch, Stretch, Bungee
- Per-slice parameter locking — override BPM, pitch, cents, ADSR, mute group, tonality, formant, grain mode independently per slice
- Parameter inheritance — slices fall back to sample-level defaults unless locked
- MIDI-triggered slice playback — slices mapped from root note C2 (MIDI 36) upward
- Lazy Chop — real-time slice creation while the sample plays
- Auto Chop — transient detection or equal-split slicing
- Snap to zero-crossing
- Follow MIDI mode — played notes auto-select slices in the UI
- Undo / Redo
- Zoom and scroll on the waveform
- Multiple themes (dark, light, lazy, snow, ghost, hack)
- Scalable UI

---

## Building

Requires CMake 3.22+, Visual Studio 2022, and the submodules (JUCE, bungee, signalsmith-stretch, signalsmith-linear) present in the repository.

```bash
git clone --recursive https://github.com/solocky-coder/DYSEKT.git
cd DYSEKT
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The built plugin will be at:
```
build/Dysekt_artefacts/Release/VST3/DYSEKT.vst3
```

Alternatively, push to the `main` branch and GitHub Actions will build it automatically. Download the artifact from the **Actions** tab.

---

## Licence

GNU General Public License v3.0 — see [LICENSE](LICENSE) for details.
Original work by [tucktuckg00se](https://github.com/tucktuckg00se/INTERSECT).
Modifications and additions by solocky-coder, developed with AI assistance (Claude by Anthropic).
