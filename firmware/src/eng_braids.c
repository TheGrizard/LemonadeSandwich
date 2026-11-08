/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* MACRO with Braids (LEMONADE SANDWICH, 2026-11-08): Mutable Instruments Braids' macro-oscillator by Emilie Gillet
 * (firmware/third_party/mi/braids, MIT; braids_glue.cc; at 44.1 kHz: tools/braids_44k.py) as an engine, in Plaits'
 * place (eng_macro.c: too heavy on the AC79, its floating point; Braids is integer throughout, a voice a third to a
 * half of an ORACLE voice). MODEL: the models kept (BRAIDS_LEAN: textures, voices, FM, wavetables, CSAW, FOLD, the
 * swarm, TOY); TIMBR and COLOR: Braids' two (the voice's SHAPE moves TIMBR); TONE: a low-pass after it (the voice's
 * CUTOFF moves it); BITS, RATE: Braids' own bit and rate reduction; STRIK: a note strikes it (VOWEL's consonant, the
 * grains and FM restarted); DRIVE: a soft clip. The track's ADSR shapes it. Four voices a part, each a Braids
 * oscillator (688 bytes) from the audio arena. MELODEE_BRAIDS 0: none of it. */
#define ENGI_MACRO 21u
#if MELODEE_BRAIDS
#include "braids_glue.h"

/* the models one can pick: Braids' shapes (settings.h MacroOscillatorShape), in four or five letters, grouped:
 * analog, voices, FM, wavetables, noise. Its gain to an even level (measured on the host: RMS at TIMBR and COLOR
 * half way, to about 9000; the impulsive ones twice at most) and its voice budget units (its cost against Plaits'
 * VA, 4 units: 1 at most a quarter of it, else 2) */
static const struct { uint8_t shape, gain_q4, units; } BR_MODEL[] = {
    {0, 10, 1},  {3, 10, 2},  {14, 27, 2}, {16, 9, 1},     /* CSAW FOLD SWARM TOY */
    {21, 8, 1},  {22, 13, 1}, {23, 24, 2},                  /* VOSIM VOWEL VFOF */
    {25, 6, 1},  {26, 6, 2},  {27, 6, 2},                   /* FM FBFM WTFM */
    {37, 8, 1},  {38, 8, 2},  {39, 17, 2},                  /* WTBL WMAP WLINE */
    {41, 32, 2}, {42, 19, 1}, {43, 8, 1}, {44, 28, 2}, {45, 32, 1}, {46, 6, 1},   /* NOISE TWINQ CLKN CLOUD DUST QPSK */
};
static const char *const N_BR_MODEL[] = {"CSAW", "FOLD", "SWARM", "TOY", "VOSIM", "VOWEL", "VFOF", "FM", "FBFM",
                                         "WTFM", "WTBL", "WMAP", "WLINE", "NOISE", "TWINQ", "CLKN", "CLOUD", "DUST",
                                         "QPSK"};
_Static_assert(NELEM(BR_MODEL) == NELEM(N_BR_MODEL), "BR_MODEL / N_BR_MODEL");
static const char *const N_BR_BITS[] = {"16", "12", "10", "8", "6", "4", "3"};
static const uint8_t BR_BITS[] = {16, 12, 10, 8, 6, 4, 3};
static const char *const N_BR_RATE[] = {"44K", "22K", "15K", "11K", "7K", "5K", "3K"};
static const uint8_t BR_RATE[] = {1, 2, 3, 4, 6, 8, 12};
static const char *const N_BR_ONOFF[] = {"OFF", "ON"};
#define BR_POLY 4
#define BR_MAGIC 0x53444942u                            /* "BIDS": the part's oscillators made (the arena gives zeros) */

static uint32_t mc_bytes(void) { return 8u + BR_POLY * bg_size(); }
static uint32_t *br_part(const track_t *t) { return resource_get(RES_ENGINE0 + (uint32_t)(t - trk), mc_bytes()); }
static uint32_t br_model(const track_t *t) { return (uint32_t)clamp(t->p[P_E0], 0, (int32_t)NELEM(BR_MODEL) - 1); }

static void br_note_on(struct track *t, voice_t *v)
{
    (void)t;
    v->s[0] = 1;                                        /* (the next render strikes it) */
}

static void br_render(struct track *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    uint32_t *h = br_part(t), k = (uint32_t)(v - t->v), i, done;
    const int16_t *p = t->p;
    const uint32_t md = br_model(t);
    uint8_t *osc;
    int16_t o[BG_BLOCK];
    int32_t pitch, timbre, color, tone, a, y = v->s[1], hold = v->s[3], cnt = v->s[2], drive = p[P_E7];
    int32_t rate = BR_RATE[clamp(p[P_E5], 0, (int32_t)NELEM(BR_RATE) - 1)];
    uint32_t bits = BR_BITS[clamp(p[P_E4], 0, (int32_t)NELEM(BR_BITS) - 1)];
    int32_t mask = -(1 << (16u - bits));            /* (the bits kept of 16) */
    if (!h || k >= BR_POLY)
        return;
    osc = (uint8_t *)(h + 2) + k * bg_size();
    if (h[0] != BR_MAGIC) {
        uint32_t j;
        for (j = 0; j < BR_POLY; j++) bg_init((uint8_t *)(h + 2) + j * bg_size());
        h[0] = BR_MAGIC;
    }
    if (v->s[0]) {
        v->s[0] = 0;
        if (p[P_E6]) bg_strike(osc);
        y = 0;                                          /* (a voice taken over starts its filter clean) */
    }
    pitch = m->pitch16 * 8 + ((m->fine * 277) >> 9);   /* (1/128 semitone; fine: 1/4096 of the rate) */
    timbre = clamp(p[P_E1] + ((m->shape - 64 * 256) >> 8), 0, 127) << 8;   /* (the voice's SHAPE moves it) */
    color = (int32_t)p[P_E2] << 8;
    tone = clamp(p[P_E3] + (m->cutoff >> 8), 0, 127);   /* (the voice's CUTOFF moves it: envelope, cords, expression) */
    a = (tone + 1) * (tone + 1) * 2;                    /* (a one-pole's coefficient, Q15: 127 all but open) */
    for (done = 0; done < n; done += BG_BLOCK) {
        uint32_t len = n - done < BG_BLOCK ? n - done : BG_BLOCK;
        bg_render(osc, BR_MODEL[md].shape, pitch, timbre, color, o, len);
        for (i = 0; i < len; i++) {
            int32_t s;
            if (cnt <= 0) {                             /* RATE: a sample held */
                hold = o[i] & mask;
                cnt = rate;
            }
            cnt--;
            s = (hold * BR_MODEL[md].gain_q4) >> 4;
            y += ((s - y) * a) >> 15;                   /* TONE */
            s = y;
            if (drive) s = soft_knee(s * (16 + drive / 2) >> 4, 22000);   /* DRIVE */
            out[done + i] += voice_amp(s, m, done + i);
        }
    }
    v->s[1] = y;
    v->s[2] = cnt;
    v->s[3] = hold;
}

static uint32_t mc_units(const struct track *t) { return BR_MODEL[br_model(t)].units; }

#define BRP(n, mo, ti, co, to, a, d, s, r, mono) {n, {mo, ti, co, to, 0, 0, 1, 0}, {a, d, s, r}, 0, mono}
static const preset_t MC_PRESETS[] = {      /* (the leads and voices MONO, the rest POLY; model: 0 CSAW 1 FOLD 2 SWARM 3 TOY 4 VOSIM 5 VOWEL 6 VFOF 7 FM 8 FBFM
                                             * 9 WTFM 10 WTBL 11 WMAP 12 WLINE 13 NOISE 14 TWINQ 15 CLKN 16 CLOUD
                                             * 17 DUST 18 QPSK) */
    BRP("INIT CSAW",    0, 64, 64, 127,  0, 64, 127, 30, 0),   /* (the tests' pitch: a saw held) */
    BRP("FOLD LEAD",    1, 70, 40, 110,  2, 64, 110, 35, 1),
    BRP("SAW SWARM",    2, 80, 60, 90,  40, 64, 127, 70, 0),
    BRP("TOY BOX",      3, 40, 60, 110,  0, 50, 60, 40, 0),
    BRP("VOSIM VOX",    4, 50, 60, 110,  4, 64, 110, 40, 1),
    BRP("VOWEL AH",     5, 30, 64, 120,  2, 64, 110, 40, 1),
    BRP("CHOIR FOF",    6, 40, 60, 115, 60, 64, 127, 80, 0),
    BRP("FM BELL",      7, 70, 40, 120,  0, 60, 0, 60, 0),
    BRP("FB GROWL",     8, 80, 70, 100,  0, 64, 110, 30, 1),
    BRP("CHAOS FM",     9, 60, 90, 110,  0, 64, 110, 40, 1),
    BRP("WAVE TABLE",  10, 50, 50, 115, 10, 64, 120, 50, 0),
    BRP("WAVE MAP",    11, 60, 60, 110, 40, 64, 127, 70, 0),
    BRP("WAVE LINE",   12, 40, 70, 110, 40, 64, 127, 70, 0),
    BRP("WIND",        13, 50, 70, 120, 70, 64, 127, 90, 0),
    BRP("TWIN PEAKS",  14, 60, 50, 120, 50, 64, 127, 80, 0),
    BRP("BIT NOISE",   15, 50, 30, 110, 20, 64, 120, 50, 0),
    BRP("GRAIN CLOUD", 16, 60, 60, 120, 60, 64, 127, 85, 0),
    BRP("DUST",        17, 30, 60, 120, 10, 64, 127, 70, 0),
    BRP("QPSK DATA",   18, 50, 50, 100,  0, 64, 110, 30, 0),
};
#undef BRP

static const engine_t ENG_MACRO = {
    .name = "MACRO",
    .page_title = {"MACRO", "CRUSH"},
    .edit = {
        {"MODEL", F_ENUM, 0, (int16_t)(NELEM(BR_MODEL) - 1), 0, N_BR_MODEL, 0},
        {"TIMBR", F_PCT, 0, 127, 64, 0, 0},
        {"COLOR", F_PCT, 0, 127, 64, 0, 0},
        {"TONE", F_PCT, 0, 127, 127, 0, 0},
        {"BITS", F_ENUM, 0, (int16_t)(NELEM(BR_BITS) - 1), 0, N_BR_BITS, 0},
        {"RATE", F_ENUM, 0, (int16_t)(NELEM(BR_RATE) - 1), 0, N_BR_RATE, 0},
        {"STRIK", F_ENUM, 0, 1, 1, N_BR_ONOFF, 0},
        {"DRIVE", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = MC_PRESETS,
    .npresets = NELEM(MC_PRESETS),
    .note_on = br_note_on,
    .render = br_render,
    .knob = {P_E1, P_E2, P_E3, P_E0},
    .poly = BR_POLY,
    .units = mc_units,
};
#endif
