# Mutable Instruments code (MIT)

Plaits' synthesis (`plaits/dsp`, `plaits/resources.*`) and the parts of stmlib it uses, by Emilie Gillet, under
the MIT licence (`LICENSE`), compatible with this project's GPL-3.0. Melodee-tess's MACRO engine
(`firmware/src/eng_macro.c`, `firmware/src/macro_glue.cc`) is built on it. "Mutable Instruments" and "Plaits"
are Emilie Gillet's names; this is not her product and she does not support it.

Taken from https://github.com/pichenettes/eurorack at f4cfa76 (2026-10-12) and
https://github.com/pichenettes/stmlib at d18def8 (2023-09-03). The Python resource generators, the hardware
drivers, the UI and the bootloader are left out.

Changes (each marked `melodee-tess` in the code):
- `plaits/dsp/dsp.h`: the sample rate is `PLAITS_SAMPLE_RATE` (the FM-1's 44.1 kHz), 48 kHz by default.
- `plaits/dsp/fx/diffuser.h`: its LFO by that rate.
- `plaits/dsp/dsp.h`: `PLAITS_SAMPLE_RATE_EXACT` tunes by the true rate (Plaits' own corrects for its chip's
  47,872 Hz); `PLAITS_BLOCK_SIZE` (16 on the FM-1: two renders a block of 32).
- `plaits/dsp/voice.h`, `voice.cc`: each model has a `PLAITS_E_` switch. `PLAITS_LEAN` builds LEMONADE SANDWICH's
  set (2026-11-07): phase distortion, string machine, waveshaping, 2-op FM, swarm, filtered noise and particles.
  The rest are left out (the wavetable, wave terrain, speech and 6-OP models for their tables); a left-out model's
  number is kept, virtual analog standing in. Each model keeps Plaits' own number.
- `plaits/dsp/engine/particle_engine.h`, `swarm_engine.h`: with `PLAITS_LEAN`, 3 particles and 4 swarm voices, not
  6 and 8 (their cost on the AC79).
- `plaits/dsp/noise/particle.h`, `engine/swarm_engine.h`: no denormals (a -400 dB square wave into the particles'
  filters; the grains' envelope coefficient flushed to zero).
- `plaits/dsp/fx/low_pass_gate.h`, `physical_modelling/resonator.h`, `string.cc`, `string.h`, `string_voice.cc`
  (2026-10-17): no denormals in dying notes (the AC79's FPU is slow on them; Plaits' Cortex-M4 flushes them): the
  resonator's mode states are flushed, a -400 dB square wave rides into the gate's, the string's and its exciter's
  filters.

## Braids (LEMONADE SANDWICH, 2026-11-08)

`braids/`: Mutable Instruments Braids' macro-oscillator (MIT, Emilie Gillet; eurorack repository, `braids/`):
`macro_oscillator`, `analog_oscillator`, `digital_oscillator`, `resources`, `excitation.h`, `svf.h`,
`parameter_interpolation.h`, `settings.h`, unchanged but for:

- `resources.cc`: its tables that depend on the sample rate (the pitch, the SVF, the grains' rate) at 44.1 kHz, not
  96 kHz: `tools/braids_44k.py` writes them with Braids' own formulas (`--check` in the tests).
- `digital_oscillator.cc` VOWEL: the formant increments x 96 / 44.1, the formants where they were.
- `BRAIDS_LEAN` (the device's): the physical models, drums, comb, Z filters, harmonics, wavetables, paraphonic, ring, Morse
  and all analog models but CSAW and FOLD are not linked (the filtered noise or CSAW in their table places, never
  chosen: `firmware/src/braids_glue.cc`), and the delay lines they needed (16 KB an oscillator) are gone: 688 bytes.
