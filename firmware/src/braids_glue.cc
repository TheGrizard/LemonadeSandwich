// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Theo Dainis (melodee-tess)
// MACRO's Braids oscillators (firmware/third_party/mi/braids, MIT, Emilie Gillet; at 44.1 kHz: tools/braids_44k.py):
// built as C++ apart from melodee.c (build.py), no C++ library (no exceptions, no RTTI, no heap: each oscillator
// lives in the memory eng_braids.c gives it, the audio arena). Integer arithmetic throughout: cheap on the AC79.
#include <stddef.h>
#include "braids/macro_oscillator.h"
#include "braids_glue.h"

#ifdef MACRO_HOST
#include <new>                                              // (host tests: the C++ library's placement new)
#else
inline void *operator new(size_t, void *p) { return p; }   // (placement new: <new> is not there)
#endif

extern "C" uint32_t bg_size(void) { return (sizeof(braids::MacroOscillator) + 7u) & ~7u; }

extern "C" void bg_init(void *mem)
{
    braids::MacroOscillator *o = new (mem) braids::MacroOscillator;
    o->Init();
    o->set_shape(braids::MACRO_OSC_SHAPE_FILTERED_NOISE);
}

extern "C" void bg_strike(void *mem) { static_cast<braids::MacroOscillator *>(mem)->Strike(); }

extern "C" void bg_render(void *mem, int32_t shape, int32_t pitch, int32_t timbre, int32_t color, int16_t *out,
                          uint32_t n)
{
    static const uint8_t sync[BG_BLOCK] = {0};
    braids::MacroOscillator *o = static_cast<braids::MacroOscillator *>(mem);
#ifdef BRAIDS_LEAN
    static const uint64_t kept =                            // (the models built: digital_oscillator.cc BRAIDS_X)
        1ull << 0 | 1ull << 3 | 1ull << 14 | 1ull << 16 | 1ull << 21 | 1ull << 22 | 1ull << 23 | 1ull << 25 |
        1ull << 26 | 1ull << 27 | 1ull << 37 | 1ull << 38 | 1ull << 39 | 1ull << 41 | 1ull << 42 | 1ull << 43 |
        1ull << 44 | 1ull << 45 | 1ull << 46;
    if (shape < 0 || shape >= braids::MACRO_OSC_SHAPE_LAST || !(kept >> shape & 1u))
        shape = braids::MACRO_OSC_SHAPE_FILTERED_NOISE;
#else
    if (shape < 0 || shape >= braids::MACRO_OSC_SHAPE_LAST) shape = braids::MACRO_OSC_SHAPE_FILTERED_NOISE;
#endif
    pitch = pitch < 0 ? 0 : pitch > 32767 ? 32767 : pitch;
    o->set_shape(static_cast<braids::MacroOscillatorShape>(shape));
    o->set_pitch(static_cast<int16_t>(pitch));
    o->set_parameters(static_cast<int16_t>(timbre), static_cast<int16_t>(color));
    o->Render(sync, out, n);
}
