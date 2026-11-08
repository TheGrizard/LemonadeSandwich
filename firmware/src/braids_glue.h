/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* MACRO's C side of Braids (braids_glue.cc, firmware/third_party/mi/braids, MIT): one oscillator in caller-owned memory */
#ifndef BRAIDS_GLUE_H
#define BRAIDS_GLUE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define BG_BLOCK 24u                    /* (Braids renders 24 samples at most at a time) */
uint32_t bg_size(void);                 /* one oscillator's bytes */
void bg_init(void *mem);
void bg_strike(void *mem);              /* a note's start: the struck models (VOWEL's consonant, the grains) */
/* n samples (n <= BG_BLOCK, even) of Braids' model shape (settings.h MacroOscillatorShape) at pitch (1/128 semitone,
 * 60 << 7 middle C), timbre and color 0..32767 */
void bg_render(void *mem, int32_t shape, int32_t pitch, int32_t timbre, int32_t color, int16_t *out, uint32_t n);
#ifdef __cplusplus
}
#endif
#endif
