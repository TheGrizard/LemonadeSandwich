/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* MACRO with Braids (LEMONADE SANDWICH, 2026-11-08): Mutable Instruments Braids' macro-oscillator by Emilie Gillet
 * (firmware/third_party/mi/braids, MIT; braids_glue.cc; at 44.1 kHz: tools/braids_44k.py) as an engine, in Plaits'
 * place (eng_macro.c: too heavy on the AC79, its floating point; Braids is integer throughout, a voice a third to a
 * half of an ORACLE voice). MODEL: the models kept (BRAIDS_LEAN: textures, voices, FM, CSAW, FOLD, the swarm, TOY); TIMBR and COLOR: Braids' two (the voice's SHAPE moves TIMBR); TONE: a low-pass after it (the voice's
 * CUTOFF moves it); BITS, RATE: Braids' own bit and rate reduction; STRIK: a note strikes it (VOWEL's consonant, the
 * grains and FM restarted); DRIVE: a soft clip. The track's ADSR shapes it. Four voices a part, each a Braids
 * oscillator (688 bytes) from the audio arena. MELODEE_BRAIDS 0: none of it. */
#define ENGI_MACRO 21u
#if MELODEE_BRAIDS
#include "braids_glue.h"

/* the models one can pick: Braids' shapes (settings.h MacroOscillatorShape), in four or five letters, grouped:
 * analog, voices, FM, noise, then the wavetables (their 33 KB table in once PHYS was out, 2026-11-08). Its gain to an even level
 * (measured on the host: the RMS over TIMBR and COLOR 0..127 and five octaves, to about 9000; 4x at most, DUST's
 * 2.5x: its grains' peaks) and its voice budget units (its cost against Plaits'
 * VA, 4 units: 1 at most a quarter of it, else 2) */
static const struct { uint8_t shape, gain_q4, units; } BR_MODEL[] = {
    {0, 9, 1},   {3, 8, 2},   {14, 16, 2}, {16, 9, 1},     /* CSAW FOLD SWARM TOY */
    {21, 7, 1},  {22, 13, 1}, {23, 28, 2},                  /* VOSIM VOWEL VFOF */
    {25, 6, 1},  {26, 6, 2},  {27, 6, 2},                   /* FM FBFM WTFM */
    {41, 21, 2}, {42, 16, 1}, {43, 8, 1}, {44, 24, 2}, {45, 40, 1}, {46, 6, 1},   /* NOISE TWINQ CLKN CLOUD DUST QPSK */
    {37, 9, 1},  {38, 9, 2},  {39, 9, 2},                   /* WTBL WMAP WLINE (back: PHYS out, room for their table) */
};
static const char *const N_BR_MODEL[] = {"CSAW", "FOLD", "SWARM", "TOY", "VOSIM", "VOWEL", "VFOF", "FM", "FBFM",
                                         "WTFM", "NOISE", "TWINQ", "CLKN", "CLOUD", "DUST",
                                         "QPSK", "WTBL", "WMAP", "WLINE"};
_Static_assert(NELEM(BR_MODEL) == NELEM(N_BR_MODEL), "BR_MODEL / N_BR_MODEL");
static const uint16_t BR_TONE[128] = {   /* TONE's one-pole, Q15: 80 Hz x 2^(TONE/16), 1 - e^(-2 pi f / 44100); 127 open */
      371,   388,   405,   423,   441,   461,   481,   502,   524,   547,   571,   596,
      622,   649,   678,   708,   739,   771,   805,   840,   876,   915,   955,   996,
     1040,  1085,  1132,  1181,  1232,  1286,  1342,  1400,  1460,  1524,  1589,  1658,
     1729,  1804,  1881,  1962,  2046,  2134,  2225,  2320,  2419,  2521,  2628,  2740,
     2856,  2976,  3102,  3232,  3367,  3508,  3654,  3806,  3964,  4128,  4299,  4475,
     4659,  4849,  5046,  5251,  5462,  5682,  5909,  6145,  6389,  6641,  6901,  7171,
     7449,  7737,  8033,  8339,  8655,  8980,  9315,  9660, 10014, 10379, 10753, 11137,
    11532, 11935, 12349, 12772, 13205, 13646, 14097, 14556, 15024, 15499, 15982, 16472,
    16968, 17470, 17977, 18489, 19005, 19523, 20044, 20566, 21088, 21609, 22129, 22646,
    23159, 23667, 24169, 24663, 25149, 25626, 26092, 26546, 26987, 27414, 27827, 28224,
    28604, 28968, 29313, 29641, 29950, 30240, 30511, 30763,
};
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
        hold = cnt = 0;                                 /* (and its RATE's hold: another engine's state in v->s[2] held
                                                         * one sample for minutes, measured: the voice silent) */
    }
    if (cnt > rate) cnt = 0;                            /* (a legato voice from another engine, or RATE turned down) */
    pitch = m->pitch16 * 8 + ((m->fine * 277) >> 9);   /* (1/128 semitone; fine: 1/4096 of the rate) */
    timbre = clamp(p[P_E1] + ((m->shape - 64 * 256) >> 8), 0, 127) << 8;   /* (the voice's SHAPE moves it) */
    color = (int32_t)p[P_E2] << 8;
    tone = clamp(p[P_E3] + (m->cutoff >> 8), 0, 127);   /* (the voice's CUTOFF moves it: envelope, cords, expression) */
    a = BR_TONE[tone];                                  /* (the one-pole's coefficient) */
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
static const preset_t MC_PRESETS[] = {      /* (the leads and voices MONO, the rest POLY; model: 0 CSAW 1 FOLD
                                             * 2 SWARM 3 TOY 4 VOSIM 5 VOWEL 6 VFOF 7 FM 8 FBFM 9 WTFM 10 NOISE
                                             * 11 TWINQ 12 CLKN 13 CLOUD 14 DUST 15 QPSK 16 WTBL 17 WMAP
                                             * 18 WLINE) */
    BRP("INIT CSAW",     0,  64,  64, 127,   0,  64, 127,  30, 0),   /* (the tests' pitch: a saw held) */
    BRP("FOLD LEAD",     1,  70,  40, 110,   2,  64, 110,  35, 1),
    BRP("SAW SWARM",     2,  80,  60,  90,  40,  64, 127,  70, 0),
    BRP("TOY BOX",       3,  40,  60, 110,   0,  50,  60,  40, 0),
    BRP("VOSIM VOX",     4,  50,  60, 110,   4,  64, 110,  40, 1),
    BRP("VOWEL AH",      5,  30,  64, 120,   2,  64, 110,  40, 1),
    BRP("CHOIR FOF",     6,  40,  60, 115,  60,  64, 127,  80, 0),
    BRP("FM BELL",       7,  70,  40, 120,   0,  60,   0,  60, 0),
    BRP("FB GROWL",      8,  80,  70, 100,   0,  64, 110,  30, 1),
    BRP("CHAOS FM",      9,  60,  90, 110,   0,  64, 110,  40, 1),
    BRP("WIND",         10,  50,  70, 120,  70,  64, 127,  90, 0),
    BRP("TWIN PEAKS",   11,  60,  50, 120,  50,  64, 127,  80, 0),
    BRP("BIT NOISE",    12,  50,  30, 110,  20,  64, 120,  50, 0),
    BRP("GRAIN CLOUD",  13,  60,  60, 120,  60,  64, 127,  85, 0),
    BRP("DUST",         14,  30,  60, 120,  10,  64, 127,  70, 0),
    BRP("QPSK DATA",    15,  50,  50, 100,   0,  64, 110,  30, 0),
    BRP("WAVE TABLE",   16,  50,  50, 115,  10,  64, 120,  50, 0),
    BRP("WAVE MAP",     17,  60,  60, 110,  40,  64, 127,  70, 0),
    BRP("WAVE LINE",    18,  40,  70, 110,  40,  64, 127,  70, 0),
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
