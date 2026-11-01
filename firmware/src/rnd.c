/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Thomas Bradley (tessera-rnd) */
/* RND: one press makes a whole new piece, after Cyma Forma's RND synth. MENU > REC BUTTON set to RND turns REC into
 * the roll: a tap rolls every track at once (an instrument engine, never DRUM, with a preset pushed far from where it
 * started; a generator and its settings; inserts, sends, cords; one key and scale for all), a hold swaps back to how
 * things were before the last roll (held again: the roll once more), so a roll can be compared with what was there.
 *
 * Everything a roll writes is an ordinary Tessera setting, so EDIT, FX, MODS and GEN fine-tune it afterwards. Ranges
 * lean wide and rough (noise, feedback, odd tunings) and slow (long gates, slow divisions) for ambient work through
 * outboard effects. What a roll leaves alone: tempo, the steps of STEP patterns, the arpeggiator's own settings.
 *
 * Cost: at most one FM6 and one PHYS track a roll (two FM6 pads crackle on the FM-1, engines.c TRK_DEF), one or two
 * inserts a track, the send effects through SPLIT. The swap copy is one snapshot of what a roll touches (.pool). */

/* ------------------------------------------------------------ the setting: MENU > REC BUTTON --- */
/* 0 RECORD (REC as upstream), 1 RND. Saved in favorites.factory[15][24] bit 6 (free: settings_test.c), as the panel
 * gamma's bits are, so it comes back at power-on with the other device settings */
#define RND_SET_BIT 0x40u
static uint32_t rnd_mode(void) { return (favorites.factory[15][24] & RND_SET_BIT) != 0u; }
static void rnd_mode_put(uint32_t on)
{
    uint8_t *b = &favorites.factory[15][24];
    uint8_t v = (uint8_t)(on ? *b | RND_SET_BIT : *b & ~RND_SET_BIT);
    if (v == *b) return;
    *b = v;
    settings_save();
}

/* ------------------------------------------------------------ randomness --- */
static uint32_t rnd_state = 0x6D2B79F5u;          /* xorshift32 (never 0) */
static uint32_t rnd_seed;                          /* the last roll's seed (shown: RND 1A2B3C4D) */
static uint8_t rnd_backed;                         /* 1: the hold swapped back to before the roll */
static uint32_t rnd_next(void)
{
    uint32_t x = rnd_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return rnd_state = x;
}
static int32_t rr(int32_t lo, int32_t hi)          /* lo..hi, both ends */
{
    if (hi <= lo) return lo;
    return lo + (int32_t)(rnd_next() % (uint32_t)(hi - lo + 1));
}
static int rnd_pct(uint32_t pct) { return rnd_next() % 100u < pct; }
/* one of n codes, by weight (w[i]: 0 never) */
static uint32_t rnd_pick(const uint8_t *code, const uint8_t *w, uint32_t n)
{
    uint32_t total = 0, i, k;
    for (i = 0; i < n; i++) total += w[i];
    if (!total) return code[0];
    k = rnd_next() % total;
    for (i = 0; i < n; i++) {
        if (k < w[i]) return code[i];
        k -= w[i];
    }
    return code[n - 1u];
}
/* a value between a and b, wild 0..100 of the way from a (the preset's) to a fresh one in lo..hi */
static int32_t rnd_push(int32_t a, int32_t lo, int32_t hi, uint32_t wild)
{
    int32_t b = rr(lo, hi);
    return a + (b - a) * (int32_t)wild / 100;
}

/* ------------------------------------------------------------ what a roll touches (and the swap keeps) --- */
/* the Tessera settings a roll writes, MODE first (a MODE change resets what follows it), STEPS before PULSES */
static const uint8_t RND_TX[] = {
    TX_MODE,
    TX_TUR_LEN, TX_TUR_PROB, TX_TUR_SPAN,
    TX_EUC_STEPS, TX_EUC_PULSES, TX_EUC_ROT,
    TX_MKV_LEAP, TX_MKV_DENS, TX_MKV_SPAN,
    TX_MRP_POS, TX_MRP_SNPA, TX_MRP_SNPB,
    TX_BNC_SPEED, TX_BNC_ANGLE, TX_BNC_BALLS,
    TX_AWK_LENA, TX_AWK_LENB, TX_AWK_PROB,
    TX_POL_DIV1, TX_POL_DIV2, TX_POL_DIV3, TX_POL_DIV4, TX_POL_SPAN,
    TX_QD_DIR, TX_QD_PROB, TX_QD_LINK, TX_QD_RANGE,
    TX_CS_Y, TX_CS_DENS, TX_CS_SIZE, TX_CS_AUTO, TX_CS_LEN, TX_CS_FILL,
    TX_SR_START, TX_SR_INTV, TX_SR_LOOP,
    TX_CH_PROG, TX_CH_BARS, TX_CH_HIT, TX_CH_OCT, TX_CH_BREATH,
    TX_EV_EVERY, TX_EV_AMOUNT, TX_EV_LOCK, TX_EV_SEED,
    TX_FM_FORM, TX_FM_CONTRAST, TX_FM_DEVELOP,
    TX_TW_SHIFT, TX_TW_DROP, TX_TW_HUMAN, TX_TW_FOLLOW,
    TX_TW_BRANCH, TX_TW_BR_TO, TX_TW_LOGIC, TX_TW_COND,
    TX_NL_MODE, TX_NL_LEN,
    TX_SUB_RATCH, TX_SUB_CHANCE, TX_SUB_HUMAN,
};
#define RND_NTX (sizeof RND_TX / sizeof RND_TX[0])
static const uint8_t RND_G[] = {                    /* the project's: the send effects */
    G_MFXTYPE, G_MFXMIX, G_MFXRATE, G_MFXCHAR,
    G_MFX2TYPE, G_MFX2MIX, G_MFX2RATE, G_MFX2CHAR,
    G_MFX3TYPE, G_MFX3MIX, G_MFX3RATE, G_MFX3CHAR,
    G_FXRETURN, G_FXMODE,
};
#define RND_NG (sizeof RND_G / sizeof RND_G[0])

typedef struct {
    uint8_t valid;                                  /* 1: a copy to swap with */
    uint8_t key_link;                               /* (KEY LINK: the project's, one value whichever track) */
    uint8_t eng[NTRK], preset[NTRK], user[NTRK], user_native[NTRK];
    int16_t p[NTRK][P_COUNT];
    uint8_t fm6[NTRK][FP_SIZE + 1u];
    uint8_t oracle[NTRK][OR_N];
    int16_t tx[TESSERA_TRACKS][RND_NTX];
    int16_t g[RND_NG];
    cord_t cord[CORD_LIVE];
} rnd_snap_t;
static rnd_snap_t rnd_snap __attribute__((section(".pool")));

/* the snapshot and what is there now change places (the hold: before / after the roll) */
static void rnd_swap(void)
{
    uint32_t tr, i;
    for (tr = 0; tr < NTRK; tr++) {
        track_t *t = &trk[tr];
        uint8_t b[FP_SIZE + 1u];
        fm1_irq_off();                              /* the audio ISR must not see half a sound (ui.c undo_step) */
        panic_req |= (uint8_t)(1u << tr);
        { uint8_t v = t->eng_req; t->eng_req = rnd_snap.eng[tr]; rnd_snap.eng[tr] = v; }
        { uint8_t v = t->preset; t->preset = rnd_snap.preset[tr]; rnd_snap.preset[tr] = v; }
        { uint8_t v = t->user; t->user = rnd_snap.user[tr]; rnd_snap.user[tr] = v; }
        { uint8_t v = t->user_native; t->user_native = rnd_snap.user_native[tr]; rnd_snap.user_native[tr] = v; }
        for (i = 0; i < P_COUNT; i++) {
            int16_t v = t->p[i];
            t->p[i] = rnd_snap.p[tr][i];
            rnd_snap.p[tr][i] = v;
        }
        memcpy(b, or_patch_of(t), OR_N);
        memcpy(or_patch[tr], rnd_snap.oracle[tr], OR_N);
        memcpy(rnd_snap.oracle[tr], b, OR_N);
        fm1_irq_on();
        if (memcmp(fm6_patch[tr], rnd_snap.fm6[tr], FP_SIZE)) {   /* (only a roll to FM6 changes it) */
            memcpy(b, fm6_patch[tr], FP_SIZE);
            fm6_set_patch(tr, rnd_snap.fm6[tr]);
            memcpy(rnd_snap.fm6[tr], b, FP_SIZE);
        }
    }
    for (tr = 0; tr < TESSERA_TRACKS; tr++) {      /* (all read first: a value can read through the MODE, as
                                                     * Turing's LOCK is its PROB turned round) */
        int16_t now[RND_NTX];
        for (i = 0; i < RND_NTX; i++) now[i] = (int16_t)pid_get(PID_TESS(RND_TX[i]), tr);
        for (i = 0; i < RND_NTX; i++) pid_set(PID_TESS(RND_TX[i]), tr, rnd_snap.tx[tr][i]);
        memcpy(rnd_snap.tx[tr], now, sizeof now);
    }
    {
        uint8_t v = (uint8_t)pid_get(PID_TESS(TX_KEY_LINK), 0);
        pid_set(PID_TESS(TX_KEY_LINK), 0, rnd_snap.key_link);
        rnd_snap.key_link = v;
    }
    for (i = 0; i < RND_NG; i++) {
        uint32_t pid = PID_GLOBAL(RND_G[i]);
        int16_t v = (int16_t)pid_get(pid, 0);
        pid_set(pid, 0, rnd_snap.g[i]);
        rnd_snap.g[i] = v;
    }
    for (i = 0; i < CORD_LIVE; i++) {
        cord_t v = *cord_get(i);
        if (!cord_set(i, &rnd_snap.cord[i])) { cord_t off; memset(&off, 0, sizeof off); cord_set(i, &off); }
        rnd_snap.cord[i] = v;
    }
}

/* what is there now, into the snapshot (before a roll) */
static void rnd_take(void)
{
    uint32_t tr, i;
    for (tr = 0; tr < NTRK; tr++) {
        track_t *t = &trk[tr];
        rnd_snap.eng[tr] = t->eng_req;
        rnd_snap.preset[tr] = t->preset;
        rnd_snap.user[tr] = t->user;
        rnd_snap.user_native[tr] = t->user_native;
        memcpy(rnd_snap.p[tr], t->p, sizeof t->p);
        memcpy(rnd_snap.fm6[tr], fm6_patch[tr], FP_SIZE);
        memcpy(rnd_snap.oracle[tr], or_patch_of(t), OR_N);
    }
    for (tr = 0; tr < TESSERA_TRACKS; tr++)
        for (i = 0; i < RND_NTX; i++)
            rnd_snap.tx[tr][i] = (int16_t)pid_get(PID_TESS(RND_TX[i]), tr);
    rnd_snap.key_link = (uint8_t)pid_get(PID_TESS(TX_KEY_LINK), 0);
    for (i = 0; i < RND_NG; i++)
        rnd_snap.g[i] = (int16_t)pid_get(PID_GLOBAL(RND_G[i]), 0);
    for (i = 0; i < CORD_LIVE; i++)
        rnd_snap.cord[i] = *cord_get(i);
    rnd_snap.valid = 1;
}

/* ------------------------------------------------------------ the roll: sound --- */
static void tp(uint32_t tr, uint32_t id, int32_t v) { pid_set(PID_TRACK(id), tr, v); }
static void tx(uint32_t tr, uint32_t id, int32_t v) { pid_set(PID_TESS(id), tr, v); }

/* the engines a roll may pick (built, offered, never DRUM): FM6 and PHYS at most once each a roll */
static uint32_t rnd_engine(uint32_t *fm6s, uint32_t *phys)
{
    uint8_t code[NENG_SHOWN], w[NENG_SHOWN];
    uint32_t n = 0, k;
    for (k = 0; k < NENG_SHOWN; k++) {
        uint32_t e = eng_vis(k), wt = 2u;
        if (!eng_ok(e) || e == ENGI_DRUM || !ENGINES[e]->npresets) continue;
        if (e == ENGI_FM6) wt = *fm6s ? 0u : 3u;
        if (e == ENGI_PHYS) wt = *phys ? 0u : 2u;
        if (e == ENGI_ORACLE) wt = 3u;
        code[n] = (uint8_t)e;
        w[n++] = (uint8_t)wt;
    }
    k = n ? rnd_pick(code, w, n) : ENGI_FALLBACK;
    if (k == ENGI_FM6) (*fm6s)++;
    if (k == ENGI_PHYS) (*phys)++;
    return k;
}

/* ORACLE's own patch roughed up: waves and noise on, POLY MOD and sync often, its own envelopes slow or snapping */
static void rnd_oracle(uint32_t tr, uint32_t wild)
{
    uint8_t *p = or_patch[tr];
    uint32_t i;
    (void)or_patch_of(&trk[tr]);                    /* (its patch made, if it never was) */
    for (i = 0; i < OR_N; i++) {
        uint32_t m = or_byte_max(i);
        if (!rnd_pct(wild)) continue;
        p[i] = (uint8_t)rr(0, (int32_t)m);
    }
    if (!(p[OR_SAW_A] | p[OR_PULSE_A] | p[OR_SAW_B] | p[OR_TRI_B] | p[OR_PULSE_B]))
        p[rnd_pct(50) ? OR_SAW_A : OR_PULSE_B] = 1;    /* (something to hear) */
    if (p[OR_LEVEL_A] < 50u && p[OR_LEVEL_B] < 50u) p[OR_LEVEL_A] = (uint8_t)rr(70, 127);
    if (p[OR_CUTOFF] < 30u) p[OR_CUTOFF] = (uint8_t)rr(30, 110);
    if (p[OR_SUSTAIN_AMP] < 40u && p[OR_DECAY_AMP] < 40u) p[OR_SUSTAIN_AMP] = (uint8_t)rr(60, 127);
    p[OR_RELEASE_AMP] = (uint8_t)rr(40, 127);
    p[OR_NOISE] = (uint8_t)(rnd_pct(45) ? rr(20, 127) : p[OR_NOISE]);   /* (hiss and grit, often) */
}

static void rnd_sound(uint32_t tr, uint32_t *fm6s, uint32_t *phys)
{
    track_t *t = &trk[tr];
    uint32_t e = rnd_engine(fm6s, phys), wild = (uint32_t)rr(40, 100), k;
    const engine_t *en = ENGINES[e];
    engine_load_of(t, e, (uint32_t)rr(0, (int32_t)en->npresets - 1));
    for (k = 0; k < 8u; k++) {                      /* the engine's eight, pushed from the preset's */
        const param_desc_t *d = pid_desc(PID_TRACK(P_E0 + k), tr);
        if (!d || d->max <= d->min) continue;
        int32_t lo = d->min, hi = d->max;
        if (e == ENGI_ORACLE && k >= 6u) lo = -20;  /* (LVL A, LVL B: not both shut) */
        tp(tr, P_E0 + k, d->fmt == F_ENUM ? (rnd_pct(wild) ? rr(lo, hi) : t->p[P_E0 + k])
                                          : rnd_push(t->p[P_E0 + k], lo, hi, wild));
    }
    if (e == ENGI_ORACLE) rnd_oracle(tr, wild * 2u / 3u);
    /* the envelope: slow swells or hard snaps, long tails for the room outside */
    tp(tr, P_ATK, rnd_pct(60) ? rr(30, 127) : rr(0, 20));
    tp(tr, P_DEC, rr(20, 127));
    tp(tr, P_SUS, rr(30, 127));
    tp(tr, P_REL, rr(50, 127));
    tp(tr, P_ED_FLT, rr(-64, 63));
    tp(tr, P_ED_PIT, rnd_pct(20) ? rr(-64, 63) : rr(-6, 6));
    tp(tr, P_ED_SHP, rr(-64, 63));
    /* the LFO: slow mostly, now and then fast enough to tear */
    tp(tr, P_LRATE, rnd_pct(75) ? rr(0, 60) : rr(60, 127));
    tp(tr, P_LWAVE, rr(0, 4));
    tp(tr, P_LFADE, rr(0, 127));
    tp(tr, P_LD_PIT, rnd_pct(25) ? rr(-40, 40) : rr(-4, 4));
    tp(tr, P_LD_FLT, rr(-64, 63));
    tp(tr, P_LD_SHP, rr(-64, 63));
    tp(tr, P_LD_AMP, rnd_pct(40) ? rr(0, 90) : 0);
    tp(tr, P_LTRIG, rr(0, 1));
    tp(tr, P_LSYNC, rnd_pct(30) ? rr(1, 10) : 0);
    /* the voice */
    {
        static const uint8_t vm[] = {V_POLY, V_MONO, V_LEGATO, V_UNISON}, vw[] = {50, 15, 25, 10};
        tp(tr, P_VOICE, rnd_pick(vm, vw, 4));
    }
    tp(tr, P_GLIDE, rnd_pct(40) ? rr(10, 127) : 0);
    tp(tr, P_DETUNE, rr(0, 127));
    tp(tr, P_SPRD, rr(0, 127));
    tp(tr, P_CHRD, rnd_pct(15) && e != ENGI_FM6 ? rr(1, 2) : 0);   /* (now and then a cluster; DIA3, DIA7) */
    for (k = 0; k < 4u; k++) tp(tr, P_M1SRC + 3u * k, 0);          /* (the routes: cords below) */
    /* the mix: all four in, spread */
    tp(tr, P_LEVEL, rr(88, 118));
    tp(tr, P_PAN, rr(-45, 45));
    tp(tr, P_MUTE, 0);
    tp(tr, P_AMODE, 0);
    {
        static const uint8_t tr_c[] = {(uint8_t)(24 - 24), (uint8_t)(24 - 12), 24, (uint8_t)(24 + 7), (uint8_t)(24 + 12)};
        static const uint8_t tr_w[] = {12, 28, 30, 10, 20};
        tp(tr, P_TRANS, (int32_t)rnd_pick(tr_c, tr_w, 5) - 24);  /* (registers apart: a low one, a high one) */
    }
}

/* the inserts: one, sometimes two, picked for grit; FX 3 stays off (the CPU) */
static void rnd_inserts(uint32_t tr)
{
    static const uint8_t fx[] = {FX_PHASER, FX_WIDTH, FX_FORMANT, FX_TAPE, FX_SPIT, FX_ECHO, FX_CHORUS, FX_FILTER};
    static const uint8_t fw[] = {10, 6, 14, 16, 18, 10, 8, 18};
    uint32_t s, n = rnd_pct(35) ? 2u : 1u;
    for (s = 0; s < 3u; s++) {
        uint32_t base = P_FXTYPE + s * 4u;
        if (s < n) {
            tp(tr, base, rnd_pick(fx, fw, NELEM(fx)));
            tp(tr, base + 1u, rr(40, 127));
            tp(tr, base + 2u, rr(0, 127));
            tp(tr, base + 3u, rr(0, 127));
        } else {
            tp(tr, base, FX_OFF);
        }
    }
    tp(tr, P_FXSEND, rr(20, 110));
    tp(tr, P_FXSEND2, rr(0, 100));
    tp(tr, P_FXSEND3, rnd_pct(50) ? rr(0, 90) : 0);
}

/* the send effects (SPLIT: a send to each): a space, a smear, sometimes a third */
static void rnd_sends(void)
{
    static const uint8_t s1[] = {FX_REVERB, FX_ROOM}, w1[] = {3, 1};
    static const uint8_t s2[] = {FX_ECHO, FX_TAPE, FX_CHORUS, FX_PHASER}, w2[] = {4, 2, 2, 2};
    static const uint8_t s3[] = {FX_OFF, FX_SPIT, FX_FORMANT, FX_FILTER, FX_WIDTH}, w3[] = {5, 2, 2, 2, 1};
    const uint8_t *code[3] = {s1, s2, s3}, *w[3] = {w1, w2, w3};
    const uint32_t n[3] = {NELEM(s1), NELEM(s2), NELEM(s3)};
    uint32_t s;
    pid_set(PID_GLOBAL(G_FXMODE), 0, 1);
    for (s = 0; s < 3u; s++) {
        uint32_t base = G_MFXTYPE + s * 4u;
        pid_set(PID_GLOBAL(base), 0, rnd_pick(code[s], w[s], n[s]));
        pid_set(PID_GLOBAL(base + 1u), 0, rr(60, 127));
        pid_set(PID_GLOBAL(base + 2u), 0, rr(0, 127));
        pid_set(PID_GLOBAL(base + 3u), 0, rr(20, 127));
    }
    pid_set(PID_GLOBAL(G_FXRETURN), 0, rr(60, 110));
}

/* ------------------------------------------------------------ the roll: what plays --- */
static void rnd_generator(uint32_t tr, uint32_t *chords)
{
    static const uint8_t gm[] = {TESS_ENG_TURING, TESS_ENG_EUCLID, TESS_ENG_MARKOV, TESS_ENG_MORPH, TESS_ENG_BOUNCE,
                                 TESS_ENG_AWAKE, TESS_ENG_POLY, TESS_ENG_QUAD, TESS_ENG_CONSTEL, TESS_ENG_SERIES,
                                 TESS_ENG_CHORD};
    uint8_t gw[] = {12, 10, 12, 8, 8, 10, 8, 6, 10, 8, 8};
    uint32_t mode;
    if (*chords) gw[NELEM(gm) - 1u] = 0;            /* (one CHORD track a roll: QNT CHRD has one to follow) */
    mode = rnd_pick(gm, gw, NELEM(gm));
    if (mode == TESS_ENG_CHORD) (*chords)++;
    tx(tr, TX_MODE, (int32_t)mode);
    switch (mode) {
    case TESS_ENG_TURING:
        tx(tr, TX_TUR_LEN, rr(2, 16)); tx(tr, TX_TUR_PROB, rr(0, 60)); tx(tr, TX_TUR_SPAN, rr(1, 3)); break;
    case TESS_ENG_EUCLID: {
        int32_t st = rr(3, 32);
        tx(tr, TX_EUC_STEPS, st); tx(tr, TX_EUC_PULSES, rr(1, st * 3 / 5 > 1 ? st * 3 / 5 : 1));
        tx(tr, TX_EUC_ROT, rr(0, st - 1)); break;
    }
    case TESS_ENG_MARKOV:
        tx(tr, TX_MKV_LEAP, rr(0, 100)); tx(tr, TX_MKV_DENS, rr(15, 80)); tx(tr, TX_MKV_SPAN, rr(1, 3)); break;
    case TESS_ENG_MORPH:
        tx(tr, TX_MRP_POS, rr(0, 127)); tx(tr, TX_MRP_SNPA, rr(0, 7)); tx(tr, TX_MRP_SNPB, rr(0, 7)); break;
    case TESS_ENG_BOUNCE:
        tx(tr, TX_BNC_SPEED, rr(1, 40)); tx(tr, TX_BNC_ANGLE, rr(0, 90)); tx(tr, TX_BNC_BALLS, rr(1, 4)); break;
    case TESS_ENG_AWAKE:
        tx(tr, TX_AWK_LENA, rr(1, 16)); tx(tr, TX_AWK_LENB, rr(1, 16)); tx(tr, TX_AWK_PROB, rr(20, 90)); break;
    case TESS_ENG_POLY:
        tx(tr, TX_POL_DIV1, rr(1, 16)); tx(tr, TX_POL_DIV2, rr(0, 16)); tx(tr, TX_POL_DIV3, rr(0, 16));
        tx(tr, TX_POL_DIV4, rr(0, 16)); tx(tr, TX_POL_SPAN, rr(1, 3)); break;
    case TESS_ENG_QUAD:
        tx(tr, TX_QD_DIR, rr(0, 3)); tx(tr, TX_QD_PROB, rr(20, 90)); tx(tr, TX_QD_LINK, 0); tx(tr, TX_QD_RANGE, rr(0, 3));
        break;
    case TESS_ENG_CONSTEL:
        tx(tr, TX_CS_Y, rr(0, 14)); tx(tr, TX_CS_DENS, rr(10, 80)); tx(tr, TX_CS_SIZE, rr(0, 7));
        tx(tr, TX_CS_AUTO, rr(0, 10)); tx(tr, TX_CS_LEN, rr(1, 16)); tx(tr, TX_CS_FILL, rnd_pct(25)); break;
    case TESS_ENG_SERIES:
        tx(tr, TX_SR_START, rr(0, 127)); tx(tr, TX_SR_INTV, rr(1, 7)); tx(tr, TX_SR_LOOP, rr(0, 16)); break;
    case TESS_ENG_CHORD:
        tx(tr, TX_CH_PROG, rr(0, 6)); tx(tr, TX_CH_BARS, rnd_pct(70) ? rr(2, 3) : rr(0, 1));
        tx(tr, TX_CH_HIT, rnd_pct(60) ? 0 : rr(1, 3)); tx(tr, TX_CH_OCT, rr(-1, 1)); tx(tr, TX_CH_BREATH, rr(0, 100));
        break;
    default: break;
    }
    tx(tr, TX_EV_SEED, rr(1, (int32_t)TESS_SEED_MAX));
    /* the phrase alive: EVOLVE often, a FORM now and then */
    tx(tr, TX_EV_EVERY, rnd_pct(60) ? rr(1, 5) : 0);
    tx(tr, TX_EV_AMOUNT, rr(10, 70));
    if (mode != TESS_ENG_TURING) tx(tr, TX_EV_LOCK, rr(20, 90));   /* (Turing's LOCK is its PROB, set above) */
    tx(tr, TX_FM_FORM, rnd_pct(25) ? rr(1, 6) : 0);
    tx(tr, TX_FM_CONTRAST, rr(10, 90));
    tx(tr, TX_FM_DEVELOP, rr(0, 60));
    tx(tr, TX_TW_SHIFT, 0);                         /* (the register: TRN above) */
    tx(tr, TX_TW_DROP, rnd_pct(50) ? rr(0, 45) : 0);
    tx(tr, TX_TW_HUMAN, rr(0, 50));
    tx(tr, TX_TW_FOLLOW, 0);
    tx(tr, TX_TW_BRANCH, 0); tx(tr, TX_TW_BR_TO, 0); tx(tr, TX_TW_LOGIC, 0); tx(tr, TX_TW_COND, 0);
    /* how long notes hold: the step's, or long fixed ones for drones */
    tx(tr, TX_NL_MODE, rnd_pct(40) ? 1 : 0);
    tx(tr, TX_NL_LEN, rr(2, 5));
    /* now and then a stutter */
    tx(tr, TX_SUB_RATCH, rnd_pct(15) ? rr(1, 5) : 0);
    tx(tr, TX_SUB_CHANCE, rr(30, 100));
    tx(tr, TX_SUB_HUMAN, rr(0, 25));
    /* the track's clock: slow by leaning */
    {
        static const uint8_t dv[] = {0, 1, 2, 4, 6, 7}, dw[] = {24, 26, 10, 8, 20, 12};   /* 1/4 1/8 1/16 8T 1/2 1/1 */
        tp(tr, P_SDIV, rnd_pick(dv, dw, NELEM(dv)));
    }
    tp(tr, P_SLEN, rr(8, 32));
    tp(tr, P_SGATE, rr(50, 127));
    tp(tr, P_SSWING, rnd_pct(30) ? rr(5, 40) : 0);
}

/* one key and scale for all four: modes mostly, often something stranger (EDOs, JI, gamelan, maqam) */
static void rnd_key(void)
{
    static const uint8_t modal[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
    const param_desc_t *d = pid_desc(PID_TRACK(P_SCALE), 0);
    int32_t top = d ? d->max : 14, scale, root = rr(0, 11);
    uint32_t tr;
    scale = rnd_pct(55) ? modal[rnd_next() % NELEM(modal)] : rr(16, top);
    tx(0, TX_KEY_LINK, 1);
    for (tr = 0; tr < NTRK; tr++) {
        tp(tr, P_SCALE, scale);
        tp(tr, P_ROOT, root);
    }
    scale_share(&trk[0]);
}

/* cords: a handful, slow wanderers mostly, into the sound, the effects and the generators' own knobs */
static uint32_t rnd_gen_dest(uint32_t tr)
{
    switch (tessera.mode[tr]) {
    case TESS_ENG_TURING: return rnd_pct(50) ? CD_TUR_PROB : CD_TUR_LEN;
    case TESS_ENG_EUCLID: return rnd_pct(50) ? CD_EUC_PULSES : CD_EUC_ROT;
    case TESS_ENG_MARKOV: return rnd_pct(50) ? CD_MKV_LEAP : CD_MKV_DENS;
    case TESS_ENG_MORPH: return CD_MRP_POS;
    case TESS_ENG_BOUNCE: return rnd_pct(50) ? CD_BNC_SPEED : CD_BNC_ANGLE;
    case TESS_ENG_AWAKE: return CD_AWK_PROB;
    case TESS_ENG_QUAD: return CD_QD_PROB;
    case TESS_ENG_CONSTEL: return rnd_pct(50) ? CD_CS_DENS : CD_CS_Y;
    case TESS_ENG_SERIES: return CD_SR_START;
    case TESS_ENG_CHORD: return CD_CH_PROG;
    default: return CD_TW_DROP;
    }
}
static void rnd_cords(void)
{
    static const uint8_t src[] = {CS_LFO, CS_RAND, CS_SH, CS_SLEW, CS_CV, CS_NOTE, CS_DRIFT, CS_TORPOR, CS_APATHY,
                                  CS_INERTIA, CS_OCHD1, CS_OCHD3, CS_OCHD5, CS_OCHD8, CS_ENV, CS_GATE};
    static const uint8_t sw[] = {8, 4, 8, 6, 6, 4, 12, 6, 6, 6, 4, 4, 4, 4, 4, 4};
    static const uint8_t dst[] = {CD_CUTOFF, CD_SHAPE, CD_PITCH, CD_PAN, CD_SPREAD, CD_LFO_RATE, CD_LFO_DEPTH,
                                  CD_FX_SEND, CD_E1, CD_E2, CD_E3, CD_E4, CD_E5, CD_E6, CD_E7, CD_E8,
                                  CD_FX1_MIX, CD_FX1_RATE, CD_FX1_CHAR, CD_FX2_CHAR, CD_RELEASE, CD_ENV_FLT,
                                  CD_EV_AMOUNT, CD_SF1_MIX, CD_SF2_CHAR, 0xFF /* the generator's own */};
    static const uint8_t dw[] = {14, 10, 4, 5, 4, 5, 4, 5, 6, 6, 6, 6, 6, 6, 4, 4, 5, 5, 6, 4, 3, 4, 3, 3, 3, 12};
    uint32_t i, n = (uint32_t)rr(3, 9);
    for (i = 0; i < CORD_LIVE; i++) {
        cord_t c;
        memset(&c, 0, sizeof c);
        if (i < n) {
            c.src = (uint8_t)rnd_pick(src, sw, NELEM(src));
            c.src_trk = (uint8_t)rr(0, NTRK - 1);
            c.dst_trk = (uint8_t)(rnd_pct(60) ? c.src_trk : (uint32_t)rr(0, NTRK - 1));
            c.dst = (uint8_t)rnd_pick(dst, dw, NELEM(dst));
            if (c.dst == 0xFFu) c.dst = (uint8_t)rnd_gen_dest(c.dst_trk);
            c.amount = (int8_t)(c.dst == CD_PITCH ? (rnd_pct(50) ? rr(2, 12) : -rr(2, 12)) : rr(-70, 70));
            c.uni = (uint8_t)rnd_pct(30);
            if (!c.amount) c.amount = 20;
        }
        if (!cord_set(i, &c)) { memset(&c, 0, sizeof c); cord_set(i, &c); }
    }
}

/* ------------------------------------------------------------ the button --- */
static void rnd_say(const char *what, uint32_t seed)
{
    char b[12];
    uint32_t k;
    for (k = 0; k < 8u; k++) {
        uint32_t d = (seed >> (28u - 4u * k)) & 15u;
        b[k] = (char)(d < 10u ? '0' + d : 'A' + d - 10u);
    }
    b[8] = 0;
    ui_say(what, b);
    ui.force = 1;
}

static void rnd_roll(void)
{
    uint32_t tr, fm6s = 0, phys = 0, chords = 0;
    momentary_restore();
    rnd_seed = rnd_next() ^ fm1_ticks() ^ (fm1_ms << 7);
    if (!rnd_seed) rnd_seed = 0x2545F491u;
    rnd_state = rnd_seed;
    rnd_take();                                     /* (the hold swaps back to this) */
    rnd_backed = 0;
    undo_depth++;                                   /* (a roll is not four sound loads to undo one by one) */
    for (tr = 0; tr < NTRK; tr++) rnd_sound(tr, &fm6s, &phys);
    undo_depth--;
    undo_clear();
    rnd_key();
    for (tr = 0; tr < TESSERA_TRACKS; tr++) {
        rnd_generator(tr, &chords);
        rnd_inserts(tr);
    }
    rnd_sends();
    rnd_cords();
    sync_reload = 1;
    rnd_say("RND ", rnd_seed);
    if (!song.playing && !seq_counting() && !chain_busy()) transport_req = 1;   /* (it plays at once) */
}

/* the hold: before the roll / the roll again */
static void rnd_back(void)
{
    if (!rnd_snap.valid) {
        ui_message("TAP REC TO ROLL");
        return;
    }
    momentary_restore();
    rnd_swap();
    undo_clear();
    sync_reload = 1;
    rnd_backed ^= 1u;
    if (rnd_backed) ui_message("BEFORE THE ROLL");
    else rnd_say("RND ", rnd_seed);
}
