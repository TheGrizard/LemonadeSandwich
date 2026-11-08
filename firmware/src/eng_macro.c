/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* MACRO (phase 4, tessera-notes/docs/design/phase4/PLAN.md): Mutable Instruments Plaits' synthesis by Emilie Gillet
 * (firmware/third_party/mi, MIT; macro_glue.cc) as an engine. MODEL: Plaits' models (the lean set, its own order);
 * HARM, TIMBR, MORPH: Plaits' three, made for modulation (MODS' E2..E4); DECAY and COLOR: its low-pass gate; HIT ON:
 * a note strikes it and the gate shapes it, OFF: it sounds while held, the track's ADSR shapes it; AUX: Plaits' second
 * output mixed in. One voice a track (Plaits is one voice), its memory (the voice and its 16 KB) from the audio
 * arena. MELODEE_PLAITS 0: none of it (the engine's ID reserved). */
#define ENGI_MACRO 21u
#if MELODEE_PLAITS
#include "macro_glue.h"

/* the models one can pick: Plaits' engine numbers (voice.cc's order), named in five letters. LEMONADE SANDWICH's set
 * (2026-11-07, voice.h PLAITS_E_): the textures (filtered noise, particles, the swarm: a granular cloud), the string
 * machine (HARM picks its chord: low, octaves and fifths), phase distortion, the waveshaper, 2-op FM. VA and its
 * filter (ORACLE does them), the 6-op FM (FM6 gone), the additive, string, modal, grain, chord, chiptune and drum
 * models are out; the wavetable, wave terrain and speech (their tables, 60 KB and more) wait for room */
static const uint8_t MC_MODEL[] = {17, 18, 16, 6, 1, 9, 10};
static const char *const N_MC_MODEL[] = {"NOISE", "DUST", "CLOUD", "STRNG", "PHASE", "SHAPE", "FM"};
_Static_assert(sizeof MC_MODEL == NELEM(N_MC_MODEL), "MC_MODEL / N_MC_MODEL");
static const char *const N_MC_HIT[] = {"OFF", "ON"};
#define MC_MAGIC 0x4F52434Du                            /* "MCRO": the part's voice made (the arena gives zeros) */

static uint32_t mc_bytes(void) { return 8u + mg_size(); }
static uint32_t *mc_part(const track_t *t) { return resource_get(RES_ENGINE0 + (uint32_t)(t - trk), mc_bytes()); }

static void mc_note_on(struct track *t, voice_t *v)
{
    (void)t;
    v->s[0] = 1;                                        /* (the next render strikes it) */
}

static void mc_render(struct track *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    uint32_t *h = mc_part(t), half, i;
    const int16_t *p = t->p;
    mg_params_t q;
    int16_t o[MG_BLOCK], a[MG_BLOCK];
    int32_t aux = p[P_E7];
    if (!h)
        return;
    if (h[0] != MC_MAGIC) {
        mg_init(h + 2);
        h[0] = MC_MAGIC;
    }
    q.model = MC_MODEL[clamp(p[P_E0], 0, (int32_t)sizeof MC_MODEL - 1)];
    q.note = (float)m->pitch16 * (1.0f / 16.0f) + (float)m->fine * 0.0042266f;   /* (fine: 1/4096 of the rate) */
    q.harmonics = (float)p[P_E1] * (1.0f / 127.0f);
    q.timbre = (float)clamp(p[P_E2] + (m->cutoff >> 8), 0, 127) * (1.0f / 127.0f);   /* (the voice's CUTOFF moves it) */
    q.morph = (float)clamp(p[P_E3] + ((m->shape - 64 * 256) >> 8), 0, 127) * (1.0f / 127.0f);       /* (SHAPE moves it) */
    q.decay = (float)p[P_E4] * (1.0f / 127.0f);
    q.lpg_colour = (float)p[P_E5] * (1.0f / 127.0f);
    q.gated = (uint8_t)(p[P_E6] != 0);
    q.trigger = (uint8_t)v->s[0];
    v->s[0] = 0;
    for (half = 0; half * MG_BLOCK < n; half++) {       /* (CTL 32: two renders of 16) */
        mg_render(h + 2, &q, o, a);
        q.trigger = 0;
        for (i = 0; i < MG_BLOCK && half * MG_BLOCK + i < n; i++) {
            int32_t s = o[i] + (((a[i] - o[i]) * aux) >> 7);
            out[half * MG_BLOCK + i] += voice_amp(s << 1, m, half * MG_BLOCK + i);
        }
    }
}

/* the voice budget units of a model: VA's 4 measured on the device (one to two Prophet voices), the others by their
 * cost against VA on the host (2026-10-17: FM 3.3x; 2026-11-07, the particles and the swarm halved, voice.h: NOISE
 * 1.5x, DUST 2.3x, CLOUD 1.5x, STRNG 1.7x, PHASE 1.3x, SHAPE 1.1x) */
static const uint8_t MC_UNITS[] = {6, 9, 6, 7, 5, 4, 13};
_Static_assert(sizeof MC_UNITS == sizeof MC_MODEL, "MC_UNITS / MC_MODEL");
static uint32_t mc_units(const struct track *t) { return MC_UNITS[clamp(t->p[P_E0], 0, (int32_t)sizeof MC_MODEL - 1)]; }

#define MCP(n, mo, ha, ti, mr, de, co, hit, ax, a, d, s, r) {n, {mo, ha, ti, mr, de, co, hit, ax}, {a, d, s, r}, 0, 1}
static const preset_t MC_PRESETS[] = {      /* (model: 0 NOISE 1 DUST 2 CLOUD 3 STRNG 4 PHASE 5 SHAPE 6 FM) */
    MCP("INIT PHASE",   4, 64,  0, 64, 64, 64, 0,  0,   0, 64, 127, 30),   /* (a sine held: the tests' pitch) */
    MCP("WIND",         0, 64, 60, 70, 64, 64, 0,  0,  70, 64, 127, 90),
    MCP("HISS BED",     0, 105, 95, 25, 64, 64, 0, 0,  60, 64, 127, 80),
    MCP("WHISTLE",      0, 64, 50, 118, 64, 64, 0, 0,  50, 64, 127, 70),
    MCP("DUST",         1, 20, 28, 30, 64, 64, 0,  0,  10, 64, 127, 60),
    MCP("RAIN GLASS",   1, 70, 55, 92, 64, 64, 0,  0,  10, 64, 127, 70),
    MCP("SWARM PAD",    2, 40, 72, 85, 64, 64, 0,  0,  80, 64, 127, 90),
    MCP("GRIT CLOUD",   2, 100, 35, 20, 64, 64, 0, 0,  40, 64, 127, 70),
    MCP("STRING MACH",  3,  0, 50, 55, 64, 64, 0,  0,  60, 64, 127, 80),   /* (HARM 0: octaves) */
    MCP("ENSEMBLE",     3,  8, 85, 30, 64, 64, 0,  0,  70, 64, 127, 85),   /* (HARM 8: fifths) */
    MCP("PD KEYS",      4, 50, 60, 50, 60, 70, 1,  0,   0, 64, 127, 40),
    MCP("CZ BASS",      4, 30, 80, 20, 64, 64, 0,  0,   0, 64, 127, 20),
    MCP("FOLD LEAD",    5, 60, 70, 50, 64, 64, 0,  0,   4, 64, 127, 35),
    MCP("FM BELL",      6, 80, 60, 30, 80, 40, 1,  0,   0, 64, 127, 60),
    MCP("FM GROWL",     6, 20, 90, 60, 64, 64, 0,  0,   0, 64, 127, 25),
};
#undef MCP

static const engine_t ENG_MACRO = {
    .name = "MACRO",
    .page_title = {"MACRO", "GATE"},
    .edit = {
        {"MODEL", F_ENUM, 0, (int16_t)(sizeof MC_MODEL - 1), 4, N_MC_MODEL, 0},
        {"HARM", F_PCT, 0, 127, 64, 0, 0},
        {"TIMBR", F_PCT, 0, 127, 64, 0, 0},
        {"MORPH", F_PCT, 0, 127, 64, 0, 0},
        {"DECAY", F_PCT, 0, 127, 64, 0, 0},
        {"COLOR", F_PCT, 0, 127, 64, 0, 0},
        {"HIT", F_ENUM, 0, 1, 0, N_MC_HIT, 0},
        {"AUX", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = MC_PRESETS,
    .npresets = NELEM(MC_PRESETS),
    .note_on = mc_note_on,
    .render = mc_render,
    .knob = {P_E1, P_E2, P_E3, P_E0},
    .poly = 1,
    .units = mc_units,
};
#endif
