# Flashy Flesh 🌀

**An open-source audiovisual state-engineering installation.**

Three counter-rotating LED spiral disks + synchronised strobe lighting + directed audio + ESP32 WiFi controller. Designed for psychedelic festivals. Grounded in current neuroscience. Buildable by any maker with a laser cutter and 3D printer.

> *"The importance of context in the annealing model is hard to overstate... the intentional content present when entropic disintegration happens provides important constraints for which new patterns form."*
> — Michael Johnson, Neural Annealing (QRI, 2019)

---

## What it does

Flashy Flesh uses five neuroscience-backed mechanisms to facilitate non-ordinary states of consciousness:

| Mechanism | Evidence Level |
|---|---|
| **Photic driving / Frequency-Following Response** | Robust (40 Hz AV: Zheng et al. 2022) |
| **Klüver form constants** — endogenous visual geometry | Well-established (Klüver 1928) |
| **DMN suppression** — neuroplasticity window | Strong w/ psychedelics; mild w/ AV alone |
| **QRI Symmetry Theory of Valence** | Theoretical, under validation |
| **Neural Annealing** — set/setting mechanism | Theoretical + empirical support |

**It is not a medical device. It does not guarantee altered states. Individual neurology, mindset, and context are primary determinants.**

---

## Build specs

- **Disk 1** — 60 cm, 4-arm Archimedean spiral, CW, 5–12 RPM
- **Disk 2** — 45 cm, 3-arm spiral, CCW, 5.35 RPM (moiré interference with Disk 1)
- **Disk 3** — 30 cm, hexagonal lattice + radial spokes, slow 1–3 RPM
- **LEDs** — SK6812 addressable RGB via 6-wire gold-contact slip ring
- **Controller** — ESP32 + FastLED + FreeRTOS + WiFi AP
- **Strobe** — 50W RGBW, PWM 0.1–4 Hz (hardware-enforced cap, photosensitivity safety)
- **Audio** — I2S BPM sync + directed PA speakers + 3.5mm headphone jack
- **Power** — 230V generator → Mean Well 24V → 5V/12V rails, ~155W average
- **Estimated build cost** — ~€2,400 (components + fabrication)

---

## Presets

| Name | Target state | Disk RPM | Strobe |
|---|---|---|---|
| Alpha Bloom | Flow / open awareness | 8–12 CW | 0.5 Hz |
| Theta Dive | Hypnagogic / creative depth | 5–7 CW | 0.3 Hz |
| Oceanic | Boundary softening | 3–5 CW | 0.1 Hz |
| Gamma Flash | Insight / peak presence | 10–14 CW | 2–4 Hz |
| Integration | Memory consolidation | 6–9 CW | off |

All presets are JSON — upload via the device's WiFi AP (`192.168.4.1`) during live performance. See `/community-presets/` for the format.

---

## Repo structure

```
flashy-flesh-machine/
├── firmware/              # ESP32 Arduino sketches
│   ├── main.ino
│   ├── presets.json
│   └── webui/             # WiFi AP web interface
├── hardware/              # KiCad schematics + fabrication files
│   ├── schematics.kicad
│   ├── disk-1-4arm.svg    # laser cut file
│   ├── disk-2-3arm.svg
│   └── motor-mount.stl    # 3D print file
├── docs/
│   ├── whitepaper.pdf     # full scientific whitepaper
│   ├── BOM.csv            # bill of materials
│   ├── assembly-guide.md
│   └── safety-signage.pdf
├── illustrations/         # all diagrams (PNG + SVG)
├── community-presets/     # DJ/VJ JSON scripts
└── index.html             # demo website
```

---

## Safety

- **Strobe hard cap: 4 Hz** — hardware timer interrupt, cannot be software-overridden
- Photosensitivity exclusion criteria in `/docs/safety-signage.pdf`
- 3-second physical consent button required to activate
- Emergency stop: simultaneous press of both rotary encoders → immediate blackout
- Standing format (lower immersion, easy exit)
- Festival deployment requires at least one trained operator

---

## Licences

- **Firmware:** MIT
- **Hardware:** CERN Open Hardware Licence — Permissive (CERN-OHL-P)
- **Documentation:** CC BY 4.0

---

## References

1. Zheng et al. (2022). Gamma oscillations and application of 40-Hz AV stimulation. *Frontiers in Human Neuroscience.* PMC9759142.
2. Carhart-Harris & Friston (2019). REBUS and the anarchic brain. *Pharmacological Reviews.*
3. Johnson, M. (2019). Neural annealing: toward a neural theory of everything. OpenTheory.net.
4. Gómez Emilsson, A. (2020). Symmetry Theory of Valence. QRI Blog.
5. Klüver, H. (1928). *Mescal: The Divine Plant and its Psychological Effects.* Kegan Paul.
6. Taylor, R.P. et al. (2011). Fractal dimension and visual preference. *Nonlinear Dynamics, Psychology, and Life Sciences.*

---

*Built by Dan N. & Claude (Chief Engineer) · February 2026 · Targeting Modem Festival, Croatia*
