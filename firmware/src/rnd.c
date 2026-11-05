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
 * Cost (MANUAL "How many voices at once": the FM-1 crackles when it has more voices to work out than time for): at
 * most one FM6 and one PHYS track a roll (two FM6 pads crackle, engines.c TRK_DEF), never UNISON, the chord keys only
 * on a slow track, FM6 never on a fast division, a fast track's notes short (release, LENGTH; no ratchets), at most
 * two inserts in all and the effects within a CPU budget (rnd_fx_points), one ORACLE track at most (FM6, ORACLE and
 * PHYS cost about twice what TRIO does: two of them a roll at most, the rest TRIO), and BUDGET lowered to RND_BUDGET if it was set higher (rnd_cpu). The swap copy is one snapshot
 * of what a roll touches (.pool). */

/* ------------------------------------------------------------ the setting: MENU > REC BUTTON --- */
/* RND_OFF RECORD (REC as upstream), RND_FREE RND, RND_AMBIENT AMBIENT, RND_NICE NICE (AMBIENT's roles with nicer
 * sounds: rnd_roll), RND_FALL FALL (NICE's on TRIO's triangles, the melodies falling as norns' Fall), RND_LOOPS LOOPS
 * (after Eno's Music for Airports: sparse loops of lengths apart, drifting), RND_ORBIT ORBIT (BOUNCE and CONSTELLATION
 * only, LOOPS' smooth voices), RND_GLITCH GLITCH (two microsound tracks, a drone and a slow atonal melody), RND_PULSE
 * PULSE (FM6's low percussion, TRIO's noise percussion, a triangle or PHYS drone and melody), RND_WOZZA WOZZA
 * (minimal piano and synth, slow, off the grid, tape-worn: rnd_wozza_sound), RND_HALO HALO (after Pärt's
 * tintinnabuli: a line, bells on its triad, a slow canon, a halo: rnd_halo_sound), RND_MIRROR MIRROR (HALO's mirror:
 * glassy, digital, its rules loosened: rnd_mirror_sound). Saved as RND_CODE:
 * favorites.factory[15][24] bits 6 and 7 (free: settings_test.c), as the panel gamma's bits are, and a variant 0..2
 * beside them (0..3: settings_rnd_variant, panel.c: the settings record's retired lowcut word), so it comes back at power-on
 * with the other device settings. Room left: 0xC0's variant 2 (tessera-rnd's PLAITS, on its own branch). MENU > DRIFT beside it
 * (settings_rnd_drift: the same word's bits 8 and 9; rnd_drift) */
#define RND_SET_BIT 0x40u
#define RND_AMB_BIT 0x80u
enum { RND_OFF, RND_FREE, RND_AMBIENT, RND_NICE, RND_FALL, RND_LOOPS, RND_ORBIT, RND_GLITCH, RND_PULSE, RND_WOZZA,
       RND_HALO, RND_MIRROR, RND_MODES };
static const struct { uint8_t bits, variant; } RND_CODE[RND_MODES] = {
    {0x00u, 0}, {0x40u, 0}, {0xC0u, 0}, {0x80u, 0}, {0x80u, 1}, {0x80u, 2}, {0xC0u, 1}, {0x40u, 1}, {0x40u, 2},
    {0x40u, 3}, {0x80u, 3}, {0xC0u, 3},             /* (0xC0's variant 2: PLAITS, on its branch) */
};
static uint32_t rnd_mode(void) { return (favorites.factory[15][24] & (RND_SET_BIT | RND_AMB_BIT)) != 0u; }
static uint32_t rnd_setting(void)
{
    uint8_t b = favorites.factory[15][24] & (RND_SET_BIT | RND_AMB_BIT);
    uint32_t m, plain = RND_OFF;
    for (m = 0; m < RND_MODES; m++) {
        if (RND_CODE[m].bits != b) continue;
        if (RND_CODE[m].variant == settings_rnd_variant) return m;
        if (!RND_CODE[m].variant) plain = m;        /* (a variant unknown with these bits: the bits' own) */
    }
    return plain;
}
static void rnd_mode_put(uint32_t m)
{
    uint8_t *b = &favorites.factory[15][24];
    uint8_t v;
    m = m < RND_MODES ? m : RND_OFF;
    v = (uint8_t)((*b & ~(RND_SET_BIT | RND_AMB_BIT)) | RND_CODE[m].bits);
    if (v == *b && RND_CODE[m].variant == settings_rnd_variant) return;
    *b = v;
    settings_rnd_variant = RND_CODE[m].variant;
    settings_save();
}

/* ------------------------------------------------------------ randomness --- */
static uint32_t rnd_state = 0x6D2B79F5u;          /* xorshift32 (never 0) */
static uint32_t rnd_seed;                          /* the last roll's seed (shown: RND 1A2B3C4D) */
static uint8_t rnd_backed;                         /* 1: the hold swapped back to before the roll */
static uint8_t rnd_nice;                           /* 1: this roll is NICE's or FALL's (rnd_roll) */
static uint8_t rnd_fall;                           /* 1: this roll is FALL's: every track TRIO on triangles */
static uint8_t rnd_loops;                          /* 1: this roll is LOOPS': four EUCLID loops of lengths apart */
static uint8_t rnd_orbit;                          /* 1: this roll is ORBIT's: BOUNCE and CONSTELLATION only */
static uint32_t rnd_next(void)
{
    uint32_t x = rnd_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return rnd_state = x;
}
static int32_t rnd_rr(int32_t lo, int32_t hi)          /* lo..hi, both ends */
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
    int32_t b = rnd_rr(lo, hi);
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
    TX_TW_LG_WITH, TX_TW_BR_MODE, TX_SUB_SHAPE,     /* (GLITCH's links and rolls) */
    TX_GRD_X, TX_GRD_Y, TX_GRD_DENS, TX_GRD_KICK, TX_GRD_SNR, TX_GRD_HAT, TX_GRD_TOMS, TX_GRD_PART4,   /* (PULSE's ATLAS) */
};
#define RND_NTX (sizeof RND_TX / sizeof RND_TX[0])
static const uint8_t RND_G[] = {                    /* the project's: the send effects */
    G_MFXTYPE, G_MFXMIX, G_MFXRATE, G_MFXCHAR,
    G_MFX2TYPE, G_MFX2MIX, G_MFX2RATE, G_MFX2CHAR,
    G_MFX3TYPE, G_MFX3MIX, G_MFX3RATE, G_MFX3CHAR,
    G_FXRETURN, G_FXMODE,
    G_BPM,                                          /* (PULSE sets a dance tempo: held REC brings yours back) */
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
static void rnd_tp(uint32_t tr, uint32_t id, int32_t v) { pid_set(PID_TRACK(id), tr, v); }
static void rnd_tx(uint32_t tr, uint32_t id, int32_t v) { pid_set(PID_TESS(id), tr, v); }

/* the engines a roll may pick (built, offered, never DRUM): FM6, ORACLE and PHYS once each at most, and two of them
 * at most a roll (RND_HEAVY): each costs about twice TRIO, and on the FM-1 the rolls with all three were the ones past
 * 80 % CPU (host profile, 80 rolls: all of the heaviest held the three; the lightest two TRIO tracks) */
#define RND_HEAVY 2u
/* the roles of AMBIENT's four tracks (RND: RND_ROLE_FREE on each; GLITCH: two RND_ROLE_GLITCH, a drone, a melody) */
enum { RND_ROLE_FREE, RND_ROLE_DRONE, RND_ROLE_CHORDS, RND_ROLE_MELODY, RND_ROLE_GLITCH,
       RND_ROLE_LOWPERC, RND_ROLE_NOISE,           /* (PULSE: FM6's low percussion, TRIO's noise percussion) */
       RND_ROLE_OSTI,                              /* (WOZZA: two notes taking turns) */
       RND_ROLE_TINT, RND_ROLE_CANON };            /* (HALO: the tintinnabuli voice; the line slower, lower) */
static uint8_t rnd_role_of[NTRK];                  /* the last roll's roles (rnd_cpu, rnd_drift, the tests) */
static uint8_t rnd_glitch;                         /* 1: this roll is GLITCH's */
static uint8_t rnd_pulse;                          /* 1: this roll is PULSE's */
static uint8_t rnd_wozza;                          /* 1: this roll is WOZZA's */
static uint8_t rnd_halo;                           /* 1: this roll is HALO's */
static uint8_t rnd_mirror;                         /* 1: this roll is MIRROR's */
static uint8_t rnd_wz_lead = NTRK;                 /* WOZZA: the first melody's track (a second one sparser, higher) */
static uint8_t rnd_want_e = 0xFFu, rnd_want_pi = 0xFFu;   /* PULSE: the engine and preset its role wants (0xFF: rolled) */
static int rnd_perc(uint32_t tr)                   /* GLITCH's glitch tracks, PULSE's percussion: each hit its attack */
{
    return rnd_role_of[tr] == RND_ROLE_GLITCH || rnd_role_of[tr] == RND_ROLE_LOWPERC || rnd_role_of[tr] == RND_ROLE_NOISE;
}
/* an engine's weight for a role (FM6 ORACLE PHYS TRIO; 2 for any other): a drone noisy (ORACLE's noise, TRIO's noise
 * waves, PHYS bowed), chords a pad, a melody struck */
static uint32_t rnd_engine_weight(uint32_t e, uint32_t role)
{
    /* FM6 ORACLE PHYS TRIO; GLITCH's: FM6's zaps and metal, PHYS struck, ORACLE's noise, TRIO's noisy waves */
    static const uint8_t W[5][4] = {{3, 3, 2, 2}, {2, 4, 3, 3}, {4, 3, 1, 3}, {3, 2, 3, 3}, {4, 3, 4, 3}};
    uint32_t i = e == ENGI_FM6 ? 0u : e == ENGI_ORACLE ? 1u : e == ENGI_PHYS ? 2u : e == 6u ? 3u : 4u;
    return i < 4u ? W[role < 5u ? role : 0u][i] : 2u;
}
static uint32_t rnd_engine(uint32_t *fm6s, uint32_t *phys, uint32_t *oracles, uint32_t role)
{
    uint8_t code[NENG_SHOWN], w[NENG_SHOWN];
    uint32_t n = 0, k, full = *fm6s + *phys + *oracles >= RND_HEAVY;
    if (rnd_fall && eng_ok(6u)) return 6u;          /* (FALL: TRIO on every track) */
    if (rnd_want_e != 0xFFu && eng_ok(rnd_want_e)) { /* (PULSE: its role's own engine) */
        *fm6s += rnd_want_e == ENGI_FM6; *phys += rnd_want_e == ENGI_PHYS; *oracles += rnd_want_e == ENGI_ORACLE;
        return rnd_want_e;
    }
    for (k = 0; k < NENG_SHOWN; k++) {
        uint32_t e = eng_vis(k), wt = rnd_engine_weight(e, role);
        if (!eng_ok(e) || e == ENGI_DRUM || !ENGINES[e]->npresets) continue;
        if (e == ENGI_FM6 && *fm6s) wt = 0u;
        if (e == ENGI_PHYS && *phys) wt = 0u;
        if (e == ENGI_ORACLE && *oracles) wt = 0u;      /* (one at most: as heavy as FM6) */
        if (full && (e == ENGI_FM6 || e == ENGI_PHYS || e == ENGI_ORACLE)) wt = 0u;
        code[n] = (uint8_t)e;
        w[n++] = (uint8_t)wt;
    }
    k = n ? rnd_pick(code, w, n) : ENGI_FALLBACK;
    if (k == ENGI_FM6) (*fm6s)++;
    if (k == ENGI_PHYS) (*phys)++;
    if (k == ENGI_ORACLE) (*oracles)++;
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
        p[i] = (uint8_t)rnd_rr(0, (int32_t)m);
    }
    if (!(p[OR_SAW_A] | p[OR_PULSE_A] | p[OR_SAW_B] | p[OR_TRI_B] | p[OR_PULSE_B]))
        p[rnd_pct(50) ? OR_SAW_A : OR_PULSE_B] = 1;    /* (something to hear) */
    if (p[OR_LEVEL_A] < 50u && p[OR_LEVEL_B] < 50u) p[OR_LEVEL_A] = (uint8_t)rnd_rr(70, 127);
    if (p[OR_CUTOFF] < 30u) p[OR_CUTOFF] = (uint8_t)rnd_rr(30, 110);
    if (p[OR_SUSTAIN_AMP] < 40u && p[OR_DECAY_AMP] < 40u) p[OR_SUSTAIN_AMP] = (uint8_t)rnd_rr(60, 127);
    p[OR_RELEASE_AMP] = (uint8_t)rnd_rr(40, 127);
    p[OR_NOISE] = (uint8_t)(rnd_pct(45) ? rnd_rr(20, 127) : p[OR_NOISE]);   /* (hiss and grit, often) */
}

static void rnd_role_sound(uint32_t tr, uint32_t e, uint32_t role);
static void rnd_sound(uint32_t tr, uint32_t *fm6s, uint32_t *phys, uint32_t *oracles, uint32_t role)
{
    track_t *t = &trk[tr];
    uint32_t e = rnd_engine(fm6s, phys, oracles, role), k;
    uint32_t wild = (uint32_t)(rnd_nice ? rnd_rr(0, 20) : rnd_pulse ? rnd_rr(0, 25) : rnd_rr(40, 100));
    const engine_t *en = ENGINES[e];
    engine_load_of(t, e, rnd_want_pi < en->npresets && e == rnd_want_e ? rnd_want_pi
                                                                     : (uint32_t)rnd_rr(0, (int32_t)en->npresets - 1));
    for (k = 0; k < 8u; k++) {                      /* the engine's eight, pushed from the preset's */
        const param_desc_t *d = pid_desc(PID_TRACK(P_E0 + k), tr);
        if (!d || d->max <= d->min) continue;
        int32_t lo = d->min, hi = d->max;
        if (e == ENGI_ORACLE && k >= 6u) lo = -20;  /* (LVL A, LVL B: not both shut) */
        rnd_tp(tr, P_E0 + k, d->fmt == F_ENUM ? (!rnd_nice && rnd_pct(wild) ? rnd_rr(lo, hi) : t->p[P_E0 + k])
                                          : rnd_push(t->p[P_E0 + k], lo, hi, wild));   /* (NICE: the preset's kinds kept) */
    }
    if (e == ENGI_ORACLE && !rnd_nice) rnd_oracle(tr, wild * 2u / 3u);
    /* the envelope: slow swells or hard snaps, long tails for the room outside */
    rnd_tp(tr, P_ATK, rnd_pct(60) ? rnd_rr(30, 127) : rnd_rr(0, 20));
    rnd_tp(tr, P_DEC, rnd_rr(20, 127));
    rnd_tp(tr, P_SUS, rnd_rr(30, 127));
    rnd_tp(tr, P_REL, rnd_rr(50, 127));
    rnd_tp(tr, P_ED_FLT, rnd_rr(-64, 63));
    rnd_tp(tr, P_ED_PIT, rnd_pct(20) ? rnd_rr(-64, 63) : rnd_rr(-6, 6));
    rnd_tp(tr, P_ED_SHP, rnd_rr(-64, 63));
    /* the LFO: slow mostly, now and then fast enough to tear */
    rnd_tp(tr, P_LRATE, rnd_pct(75) ? rnd_rr(0, 60) : rnd_rr(60, 127));
    rnd_tp(tr, P_LWAVE, rnd_rr(0, 4));
    rnd_tp(tr, P_LFADE, rnd_rr(0, 127));
    rnd_tp(tr, P_LD_PIT, rnd_pct(25) ? rnd_rr(-40, 40) : rnd_rr(-4, 4));
    rnd_tp(tr, P_LD_FLT, rnd_rr(-64, 63));
    rnd_tp(tr, P_LD_SHP, rnd_rr(-64, 63));
    rnd_tp(tr, P_LD_AMP, rnd_pct(40) ? rnd_rr(0, 90) : 0);
    rnd_tp(tr, P_LTRIG, rnd_rr(0, 1));
    rnd_tp(tr, P_LSYNC, rnd_pct(30) ? rnd_rr(1, 10) : 0);
    /* the voice */
    {
        static const uint8_t vm[] = {V_POLY, V_MONO, V_LEGATO}, vw[] = {55, 20, 25};   /* (no UNISON: the CPU) */
        rnd_tp(tr, P_VOICE, rnd_pick(vm, vw, 3));
    }
    rnd_tp(tr, P_GLIDE, rnd_pct(40) ? rnd_rr(10, 127) : 0);
    rnd_tp(tr, P_DETUNE, rnd_rr(0, 127));
    rnd_tp(tr, P_SPRD, rnd_rr(0, 127));
    rnd_tp(tr, P_CHRD, rnd_pct(8) && e != ENGI_FM6 ? 1 : 0);    /* (now and then a triad, DIA3; rnd_cpu keeps it to a
                                                                 * slow track) */
    for (k = 0; k < 4u; k++) rnd_tp(tr, P_M1SRC + 3u * k, 0);          /* (the routes: cords below) */
    /* the mix: all four in, spread */
    rnd_tp(tr, P_LEVEL, rnd_rr(72, 96));             /* (under the default 104: four rough tracks at once kept
                                                     * off the output limiter with MASTER turned up) */
    rnd_tp(tr, P_PAN, rnd_rr(-45, 45));
    rnd_tp(tr, P_MUTE, 0);
    rnd_tp(tr, P_AMODE, 0);
    if (role != RND_ROLE_FREE) rnd_role_sound(tr, e, role);
}

/* AMBIENT: what each role's sound needs on top of the roll's own. A drone swells in and never quite stops, low and
 * still in the middle, ORACLE with its noise up, TRIO on a noisy wave, PHYS bowed; chords a slow pad; a melody struck
 * and ringing, higher up. No tremolo (LFO -> AMP) anywhere: a chop is not ambient */
static void rnd_role_sound(uint32_t tr, uint32_t e, uint32_t role)
{
    if (role == RND_ROLE_DRONE) {
        static const uint8_t tw[] = {7, 8, 12, 13, 0, 2}, tww[] = {4, 4, 2, 2, 2, 1};   /* P+N NOIS RING RING3 SAW3 PPT */
        static const uint8_t nw[] = {0, 4, 5, 3, 2}, nww[] = {4, 4, 3, 2, 2};           /* NICE: SAW3 TRI3 S&T SST PPT */
        rnd_tp(tr, P_ATK, rnd_rr(70, 127));
        rnd_tp(tr, P_DEC, rnd_rr(90, 127));
        rnd_tp(tr, P_SUS, rnd_rr(100, 127));
        rnd_tp(tr, P_REL, rnd_rr(100, 127));
        rnd_tp(tr, P_ED_PIT, 0);
        rnd_tp(tr, P_LRATE, rnd_rr(0, 25));
        rnd_tp(tr, P_LD_PIT, rnd_rr(-2, 2));
        rnd_tp(tr, P_LTRIG, 1);
        rnd_tp(tr, P_LSYNC, 0);
        rnd_tp(tr, P_VOICE, V_LEGATO);
        rnd_tp(tr, P_GLIDE, rnd_rr(50, 127));
        rnd_tp(tr, P_LEVEL, rnd_rr(70, 84));
        rnd_tp(tr, P_PAN, rnd_rr(-10, 10));
        if (e == ENGI_ORACLE) {
            uint8_t *p = or_patch[tr];
            p[OR_NOISE] = (uint8_t)(rnd_nice ? rnd_rr(0, 15) : rnd_rr(50, 127));
            p[OR_ATTACK_AMP] = (uint8_t)rnd_rr(60, 127);
            p[OR_SUSTAIN_AMP] = (uint8_t)rnd_rr(100, 127);
            p[OR_RELEASE_AMP] = (uint8_t)rnd_rr(90, 127);
            p[OR_RELEASE_ON] = 1;
        } else if (e == 6u) {
            rnd_tp(tr, P_E0, (int32_t)(rnd_nice ? rnd_pick(nw, nww, NELEM(nw)) : rnd_pick(tw, tww, NELEM(tw))));   /* (WAVE) */
        } else if (e == ENGI_PHYS) {
            rnd_tp(tr, P_E0 + 6u, rnd_rr(60, 127));     /* (BOW) */
        } else if (e == ENGI_FM6) {
            rnd_tp(tr, P_E0 + 7u, rnd_rr(10, 26));      /* (AEG: slower carriers, rnd_cpu's cap) */
        }
    } else if (role == RND_ROLE_CHORDS) {
        rnd_tp(tr, P_ATK, rnd_rr(40, 110));
        rnd_tp(tr, P_DEC, rnd_rr(60, 127));
        rnd_tp(tr, P_SUS, rnd_rr(90, 127));
        rnd_tp(tr, P_REL, rnd_rr(70, 110));
        rnd_tp(tr, P_ED_PIT, 0);
        rnd_tp(tr, P_LD_PIT, rnd_rr(-3, 3));
        rnd_tp(tr, P_VOICE, V_POLY);
        rnd_tp(tr, P_GLIDE, 0);
        rnd_tp(tr, P_CHRD, 0);
        rnd_tp(tr, P_LEVEL, rnd_rr(72, 88));
        rnd_tp(tr, P_PAN, rnd_rr(-20, 20));
        if (e == ENGI_ORACLE) {
            uint8_t *p = or_patch[tr];
            if (p[OR_NOISE] > 40u) p[OR_NOISE] = (uint8_t)rnd_rr(0, 40);
            p[OR_ATTACK_AMP] = (uint8_t)rnd_rr(40, 100);
            p[OR_SUSTAIN_AMP] = (uint8_t)rnd_rr(100, 127);
            p[OR_RELEASE_AMP] = (uint8_t)rnd_rr(70, 110);
            p[OR_RELEASE_ON] = 1;
        }
    } else if (role == RND_ROLE_LOWPERC) {         /* PULSE: the kick, FM6's (a body, a drop, a mallet) */
        rnd_tp(tr, P_ATK, 0);
        rnd_tp(tr, P_DEC, rnd_rr(10, 35));          /* (the pitch's drop, how long) */
        rnd_tp(tr, P_SUS, rnd_rr(0, 20));
        rnd_tp(tr, P_REL, rnd_rr(10, 40));
        rnd_tp(tr, P_ED_PIT, rnd_pct(85) ? -rnd_rr(40, 64) : -rnd_rr(10, 30));   /* (a kick's drop) */
        rnd_tp(tr, P_ED_FLT, 0);
        rnd_tp(tr, P_LD_PIT, 0);
        rnd_tp(tr, P_LD_FLT, rnd_rr(-20, 20));
        rnd_tp(tr, P_LD_SHP, 0);
        rnd_tp(tr, P_LRATE, rnd_rr(0, 60));
        rnd_tp(tr, P_VOICE, V_MONO);
        rnd_tp(tr, P_GLIDE, 0);
        rnd_tp(tr, P_CHRD, 0);
        rnd_tp(tr, P_LEVEL, rnd_rr(92, 100));
        rnd_tp(tr, P_PAN, 0);
        if (e == ENGI_FM6) {                        /* (ALG FB MLVL MRAT MEG VMOD DTUN AEG: the preset's, moved) */
            rnd_tp(tr, P_E0 + 1u, rnd_pulse ? rnd_rr(2, 7) : rnd_rr(-3, 5));   /* FB: grit (PULSE: dirty) */
            rnd_tp(tr, P_E0 + 2u, rnd_pulse ? rnd_rr(0, 45) : rnd_rr(-20, 30));   /* MLVL: brightness */
            rnd_tp(tr, P_E0 + 3u, rnd_pct(75) ? 0 : rnd_rr(-3, 3));   /* MRAT: now and then metal */
            rnd_tp(tr, P_E0 + 4u, rnd_rr(-30, 30)); /* MEG: the attack's bite */
            rnd_tp(tr, P_E0 + 5u, rnd_rr(3, 7));    /* VMOD: harder hits brighter */
            rnd_tp(tr, P_E0 + 6u, rnd_rr(0, 30));
            rnd_tp(tr, P_E0 + 7u, rnd_rr(-30, 20)); /* AEG: tight to ringing */
        } else if (e == ENGI_PHYS) {
            rnd_tp(tr, P_E0 + 6u, 0);               /* (no BOW: struck) */
        }
    } else if (role == RND_ROLE_NOISE) {           /* PULSE: TRIO's noise percussion (hats, snares, rims), or now and
                                                     * then a synthesized micro sound (ring, sync, a zap) */
        static const uint8_t micro[] = {13, 14, 9, 11, 12};   /* RING3 R+S SYNC SYNC3 RING */
        int mic = rnd_pct(25);
        rnd_tp(tr, P_ATK, 0);
        rnd_tp(tr, P_DEC, mic ? rnd_rr(0, 30) : rnd_rr(3, 45));   /* (closed to open, rims to snares) */
        rnd_tp(tr, P_SUS, 0);
        rnd_tp(tr, P_REL, mic ? rnd_rr(0, 20) : rnd_rr(0, 35));
        rnd_tp(tr, P_ED_PIT, mic && rnd_pct(60) ? (rnd_pct(50) ? rnd_rr(25, 63) : -rnd_rr(25, 64)) : 0);
        rnd_tp(tr, P_ED_FLT, rnd_pct(60) ? (rnd_pct(50) ? rnd_rr(20, 63) : -rnd_rr(20, 64)) : 0);
        rnd_tp(tr, P_LD_PIT, 0);
        rnd_tp(tr, P_LD_FLT, rnd_rr(-50, 50));
        rnd_tp(tr, P_LRATE, rnd_rr(30, 120));       /* (the filter moving under the hits) */
        rnd_tp(tr, P_VOICE, rnd_pct(60) ? V_MONO : V_POLY);
        rnd_tp(tr, P_GLIDE, 0);
        rnd_tp(tr, P_CHRD, 0);
        rnd_tp(tr, P_LEVEL, rnd_rr(80, 96));
        rnd_tp(tr, P_PAN, rnd_rr(-50, 50));
        if (e == 6u) {                              /* (WAVE INT2 INT3 DTN MODE CUT RES PW) */
            rnd_tp(tr, P_E0, mic ? (int32_t)micro[rnd_next() % NELEM(micro)] : rnd_pct(60) ? 8 : 7);   /* NOIS, P+N */
            rnd_tp(tr, P_E0 + 1u, mic ? rnd_rr(-24, 24) : 0);
            rnd_tp(tr, P_E0 + 2u, mic ? rnd_rr(-24, 24) : 0);
            rnd_tp(tr, P_E0 + 4u, mic ? rnd_rr(0, 3) : rnd_pick((const uint8_t[]){2, 1, 0, 3}, (const uint8_t[]){6, 3, 1, 1}, 4));
            rnd_tp(tr, P_E0 + 5u, rnd_rr(50, 127)); /* CUT */
            rnd_tp(tr, P_E0 + 6u, rnd_rr(30, 110)); /* RES: a ring in the noise */
        }
    } else if (role == RND_ROLE_GLITCH) {          /* GLITCH: microsound, a click, a zap, a grain (short to nothing) */
        rnd_tp(tr, P_ATK, rnd_rr(0, 3));
        rnd_tp(tr, P_DEC, rnd_rr(0, 35));
        rnd_tp(tr, P_SUS, rnd_pct(70) ? 0 : rnd_rr(0, 30));
        rnd_tp(tr, P_REL, rnd_rr(0, 25));
        rnd_tp(tr, P_ED_PIT, rnd_pct(55) ? (rnd_pct(50) ? rnd_rr(25, 63) : -rnd_rr(25, 64)) : 0);   /* (a zap) */
        rnd_tp(tr, P_ED_FLT, rnd_rr(-64, 63));
        rnd_tp(tr, P_LRATE, rnd_pct(60) ? rnd_rr(85, 127) : rnd_rr(0, 60));
        rnd_tp(tr, P_LD_PIT, rnd_pct(30) ? rnd_rr(-24, 24) : 0);
        rnd_tp(tr, P_LSYNC, 0);
        rnd_tp(tr, P_VOICE, rnd_pct(70) ? V_MONO : V_POLY);   /* (never LEGATO: every hit its attack) */
        rnd_tp(tr, P_GLIDE, rnd_pct(15) ? rnd_rr(5, 40) : 0);
        rnd_tp(tr, P_CHRD, 0);
        rnd_tp(tr, P_LEVEL, rnd_rr(74, 92));
        rnd_tp(tr, P_PAN, rnd_rr(-60, 60));
        if (e == ENGI_ORACLE) {
            uint8_t *p = or_patch[tr];
            p[OR_ATTACK_AMP] = (uint8_t)rnd_rr(0, 3);
            p[OR_DECAY_AMP] = (uint8_t)rnd_rr(0, 35);
            p[OR_SUSTAIN_AMP] = (uint8_t)(rnd_pct(70) ? 0 : rnd_rr(0, 30));
            p[OR_RELEASE_AMP] = (uint8_t)rnd_rr(0, 25);
            p[OR_DECAY_FILTER] = (uint8_t)rnd_rr(0, 40);
            p[OR_RELEASE_ON] = 1;
        } else if (e == ENGI_PHYS) {
            rnd_tp(tr, P_E0 + 6u, 0);                   /* (BOW off: struck) */
        } else if (e == ENGI_FM6) {
            rnd_tp(tr, P_E0 + 7u, rnd_rr(-64, -40));    /* (AEG: the carriers' release short) */
        }
    } else {
        rnd_tp(tr, P_ATK, rnd_rr(0, 40));
        rnd_tp(tr, P_SUS, rnd_rr(40, 110));
        rnd_tp(tr, P_REL, rnd_rr(60, 115));
        rnd_tp(tr, P_ED_PIT, rnd_rr(-4, 4));
        rnd_tp(tr, P_LRATE, rnd_rr(0, 50));
        rnd_tp(tr, P_LEVEL, rnd_rr(80, 96));
        rnd_tp(tr, P_PAN, rnd_rr(-50, 50));
        if (rnd_pct(30)) rnd_tp(tr, P_VOICE, V_LEGATO);
    }
    rnd_tp(tr, P_LD_AMP, role == RND_ROLE_GLITCH && rnd_pct(30) ? rnd_rr(40, 100) : 0);   /* (GLITCH: a chop now
                                                                                         * and then) */
    if (rnd_nice) {                                 /* NICE: the patch near the preset, clean and still */
        static const uint8_t clean[] = {0, 1, 2, 3, 4, 5}, cw[] = {4, 3, 2, 2, 4, 3};   /* SAW3 PLS3 PPT SST TRI3 S&T */
        rnd_tp(tr, P_ED_FLT, rnd_rr(-10, 30));
        rnd_tp(tr, P_ED_PIT, 0);
        rnd_tp(tr, P_ED_SHP, rnd_rr(-15, 15));
        rnd_tp(tr, P_LRATE, rnd_rr(10, 50));
        rnd_tp(tr, P_LWAVE, 0);                     /* (a sine) */
        rnd_tp(tr, P_LD_PIT, rnd_rr(-2, 2));
        rnd_tp(tr, P_LD_FLT, rnd_rr(-15, 15));
        rnd_tp(tr, P_LD_SHP, rnd_rr(-15, 15));
        rnd_tp(tr, P_LSYNC, 0);
        rnd_tp(tr, P_DETUNE, rnd_rr(0, 40));
        rnd_tp(tr, P_SPRD, rnd_rr(30, 90));
        if (role == RND_ROLE_MELODY) rnd_tp(tr, P_GLIDE, rnd_pct(20) ? rnd_rr(10, 50) : 0);
        if (e == 6u && trk[tr].p[P_E0] > 5) rnd_tp(tr, P_E0, (int32_t)rnd_pick(clean, cw, NELEM(clean)));
        if ((rnd_fall || rnd_pulse) && e == 6u) {   /* FALL (PULSE's slow two): three triangles, consonant, soft */
            static const uint8_t i2[] = {24, 36, 31, 12, 43}, i2w[] = {3, 3, 2, 1, 1};   /* (+24: 0 +12 +7 -12 +19) */
            static const uint8_t i3[] = {12, 24, 36, 0, 31}, i3w[] = {3, 2, 2, 1, 1};    /* (-12 0 +12 -24 +7) */
            rnd_tp(tr, P_E0, 4);                    /* WAVE TRI3 */
            rnd_tp(tr, P_E0 + 1u, (int32_t)rnd_pick(i2, i2w, NELEM(i2)) - 24);   /* INT2 */
            rnd_tp(tr, P_E0 + 2u, (int32_t)rnd_pick(i3, i3w, NELEM(i3)) - 24);   /* INT3 */
            rnd_tp(tr, P_E0 + 3u, rnd_rr(2, 12));   /* DTN, cents */
            rnd_tp(tr, P_E0 + 4u, 0);               /* MODE LP */
            rnd_tp(tr, P_E0 + 5u, rnd_rr(70, 127)); /* CUT */
            rnd_tp(tr, P_E0 + 6u, rnd_rr(0, 30));   /* RES */
            if (role == RND_ROLE_MELODY) {          /* (a leaf lands: struck, ringing out) */
                rnd_tp(tr, P_ATK, rnd_rr(0, 10));
                rnd_tp(tr, P_DEC, rnd_rr(40, 90));
                rnd_tp(tr, P_SUS, rnd_rr(0, 50));
                rnd_tp(tr, P_REL, rnd_rr(60, 110));
            }
        }
        if (e == ENGI_ORACLE && or_patch[tr][OR_NOISE] > 15u) or_patch[tr][OR_NOISE] = (uint8_t)rnd_rr(0, 15);
    }
}

/* ------------------------------------------------------------ the roll: effects within a CPU budget --- */
/* What each effect costs, in points (measured on the host, tests/ui_test.c test_rnd: four tracks rolled, one insert of
 * a kind on each, or one send effect; a point is about a twentieth of what a roll's engines cost), a send effect
 * RND_FX_BUS more (its bus: the tracks' sends summed, its output mixed back). On the FM-1 a roll's effects had cost as
 * much again as its engines (80 to 105 % CPU, the overload: late blocks are a digital crunch and the voices it sheds a
 * stutter), and the modes' three sends and inserts at 14 to 20 points 70 to 80 %, so a roll's effects share
 * RND_FX_POINTS, and only the light ones are rolled (rnd_fx_light: ECHO FILTER ROOM TAPE CHORUS; never REVERB,
 * FORMANT, WIDTH, PHASER or SPIT). Inserts: FX 1 only. */
#define RND_FX_BUS 2u
static int rnd_fx_light(uint32_t type)
{
    return type == FX_OFF || type == FX_ECHO || type == FX_FILTER || type == FX_ROOM || type == FX_TAPE ||
           type == FX_CHORUS;
}
static uint32_t rnd_fx_points(uint32_t type, int send)
{
    uint32_t bus = send ? RND_FX_BUS : 0u;          /* (no recursion: the build's stack audit) */
    switch (type) {
    case FX_OFF: return 0u;
    case FX_ECHO: case FX_FILTER: return bus + 1u;
    case FX_ROOM: return bus + 3u;
    case FX_SPIT: case FX_TAPE: case FX_CHORUS: return bus + 4u;
    case FX_PHASER: return bus + 5u;
    case FX_WIDTH: return bus + 7u;
    case FX_FORMANT: return bus + 9u;
    case FX_REVERB: return bus + 14u;
    default: return bus + 6u;
    }
}
#define RND_FX_POINTS 10u
/* 1: rolls pick effects (rnd_sends, rnd_inserts, rnd_fx_mode, rnd_fx_wozza); 0: every roll dry (rnd_fx_dry). Off since
 * tessera-rnd-19: with effects the FM-1 clicked now and then on NICE and the others, and ran at 70 to 80 % */
#define RND_FX_ROLLS 0
/* a roll without effects: the send effects and every track's three off, nothing sent */
static void rnd_fx_dry(void)
{
    uint32_t tr, s;
    for (s = 0; s < 3u; s++) pid_set(PID_GLOBAL(G_MFXTYPE + s * 4u), 0, FX_OFF);
    for (tr = 0; tr < NTRK; tr++) {
        for (s = 0; s < 3u; s++) rnd_tp(tr, P_FXTYPE + s * 4u, FX_OFF);
        rnd_tp(tr, P_FXSEND, 0);
        rnd_tp(tr, P_FXSEND2, 0);
        rnd_tp(tr, P_FXSEND3, 0);
    }
}
#define RND_INSERT_TRACKS 2u

/* the send effects (SPLIT: a send to each): a space (ROOM) or an echo, now and then the other too; the third off.
 * -> the points they use */
static uint32_t rnd_sends(void)
{
    uint32_t used = 0, s;
    uint32_t type[3];
    type[0] = rnd_pct(65) ? FX_ROOM : FX_ECHO;
    type[1] = rnd_pct(50) ? (type[0] == FX_ROOM ? FX_ECHO : FX_ROOM) : FX_OFF;
    type[2] = FX_OFF;
    pid_set(PID_GLOBAL(G_FXMODE), 0, 1);
    for (s = 0; s < 3u; s++) {
        uint32_t base = G_MFXTYPE + s * 4u;
        pid_set(PID_GLOBAL(base), 0, (int32_t)type[s]);
        pid_set(PID_GLOBAL(base + 1u), 0, rnd_rr(60, 120));
        pid_set(PID_GLOBAL(base + 2u), 0, rnd_rr(0, 127));
        pid_set(PID_GLOBAL(base + 3u), 0, rnd_rr(20, 120));
        used += rnd_fx_points(type[s], 1);
    }
    pid_set(PID_GLOBAL(G_FXRETURN), 0, rnd_rr(60, 100));
    return used;
}

/* the inserts: FX 1 on RND_INSERT_TRACKS tracks at most, each kind if the points left allow it; FX 2 and 3 off */
static void rnd_inserts(uint32_t used)
{
    static const uint8_t fx[] = {FX_FILTER, FX_ECHO, FX_TAPE, FX_CHORUS};
    static const uint8_t fw[] = {18, 12, 8, 6};
    uint8_t ok[NELEM(fx)], tracks[TESSERA_TRACKS] = {0, 1, 2, 3};
    uint32_t tr, k, n = 0, want = (uint32_t)rnd_rr(0, (int32_t)RND_INSERT_TRACKS);
    for (k = TESSERA_TRACKS - 1u; k > 0u; k--) {    /* (which tracks: shuffled) */
        uint32_t j = rnd_next() % (k + 1u);
        uint8_t v = tracks[k]; tracks[k] = tracks[j]; tracks[j] = v;
    }
    for (tr = 0; tr < TESSERA_TRACKS; tr++) {
        uint32_t t = tracks[tr], type = FX_OFF, s;
        if (n < want) {
            for (k = 0; k < NELEM(fx); k++) ok[k] = used + rnd_fx_points(fx[k], 0) <= RND_FX_POINTS ? fw[k] : 0u;
            type = rnd_pick(fx, ok, NELEM(fx));
            if (used + rnd_fx_points(type, 0) > RND_FX_POINTS) type = FX_OFF;
            if (type != FX_OFF) { used += rnd_fx_points(type, 0); n++; }
        }
        for (s = 0; s < 3u; s++) rnd_tp(t, P_FXTYPE + s * 4u, s == 0u ? (int32_t)type : FX_OFF);
        if (type != FX_OFF) {
            rnd_tp(t, P_FXMIX, rnd_rr(40, 110));
            rnd_tp(t, P_FXRATE, rnd_rr(0, 127));
            rnd_tp(t, P_FXCHAR, type == FX_SPIT ? rnd_rr(0, 70) : rnd_rr(0, 127));   /* (SPIT: its BITE not flat out) */
        }
        rnd_tp(t, P_FXSEND, rnd_rr(20, 110));
        rnd_tp(t, P_FXSEND2, song.g[G_MFX2TYPE] != FX_OFF ? rnd_rr(0, 100) : 0);
        rnd_tp(t, P_FXSEND3, 0);
    }
}

/* ------------------------------------------------------------ the roll: what plays --- */
/* AMBIENT: a drone moves between two to four notes, one every bar to four bars, legato; chords a CHORD track's
 * progression held (or on the beat) over two or four bars; a melody one of the line generators, sparser and slower
 * than RND's (no CHORD, POLY or QUAD: one line) */
static void rnd_generator(uint32_t tr, uint32_t *chords);
static void rnd_role_generator(uint32_t tr, uint32_t role, uint32_t *chords)
{
    if (role == RND_ROLE_FREE) {
        rnd_generator(tr, chords);
        return;
    }
    rnd_generator(tr, chords);                          /* (EVOLVE, TWEAK, TIMING and the rest as RND's, then:) */
    rnd_tx(tr, TX_FM_FORM, 0);
    rnd_tx(tr, TX_SUB_RATCH, 0);
    rnd_tp(tr, P_SSWING, 0);
    if (role == RND_ROLE_DRONE) {
        static const uint8_t dv[] = {7, 8, 9}, dw[] = {2, 4, 4};           /* 1/1 2BAR 4BAR */
        rnd_tx(tr, TX_MODE, TESS_ENG_TURING);
        rnd_tx(tr, TX_TUR_LEN, rnd_rr(2, 4));
        rnd_tx(tr, TX_TUR_PROB, rnd_rr(0, 15));
        rnd_tx(tr, TX_TUR_SPAN, 1);
        rnd_tp(tr, P_SDIV, rnd_pick(dv, dw, NELEM(dv)));
        rnd_tp(tr, P_SGATE, 127);
        rnd_tx(tr, TX_NL_MODE, 0);
        rnd_tx(tr, TX_EV_EVERY, rnd_pct(50) ? rnd_rr(4, 5) : 0);    /* (8 or 16 bars, or never) */
        rnd_tx(tr, TX_EV_AMOUNT, rnd_rr(10, 30));
        rnd_tx(tr, TX_TW_DROP, 0);
        rnd_tx(tr, TX_TW_HUMAN, rnd_rr(0, 20));
    } else if (role == RND_ROLE_CHORDS) {
        rnd_tx(tr, TX_MODE, TESS_ENG_CHORD);
        (*chords)++;
        rnd_tx(tr, TX_CH_PROG, rnd_rr(0, 6));
        rnd_tx(tr, TX_CH_BARS, rnd_pct(80) ? rnd_rr(2, 3) : 1);
        rnd_tx(tr, TX_CH_HIT, rnd_pct(75) ? 0 : 1);
        rnd_tx(tr, TX_CH_OCT, 0);                      /* (its register: rnd_registers) */
        rnd_tx(tr, TX_CH_BREATH, rnd_rr(0, 60));
        rnd_tp(tr, P_SDIV, 0);
        rnd_tp(tr, P_SGATE, 127);
        rnd_tx(tr, TX_NL_MODE, 0);
        rnd_tx(tr, TX_EV_EVERY, 0);
        rnd_tx(tr, TX_TW_DROP, 0);
    } else {
        static const uint8_t gm[] = {TESS_ENG_TURING, TESS_ENG_MARKOV, TESS_ENG_AWAKE, TESS_ENG_BOUNCE, TESS_ENG_CONSTEL,
                                     TESS_ENG_SERIES, TESS_ENG_MORPH, TESS_ENG_EUCLID};
        static const uint8_t gw[] = {12, 12, 12, 8, 12, 8, 6, 6};
        static const uint8_t dv[] = {0, 1, 4, 6, 2, 7}, dw[] = {24, 30, 14, 16, 8, 8};   /* 1/4 1/8 8T 1/2 1/16 1/1 */
        uint32_t tries = 0;
        while ((tessera.mode[tr] == TESS_ENG_CHORD || tessera.mode[tr] == TESS_ENG_POLY ||
                tessera.mode[tr] == TESS_ENG_QUAD) && tries++ < 8u) {   /* (a line generator, its settings rolled) */
            if (tessera.mode[tr] == TESS_ENG_CHORD && *chords) (*chords)--;
            rnd_generator(tr, chords);
            rnd_tx(tr, TX_FM_FORM, 0);
            rnd_tx(tr, TX_SUB_RATCH, 0);
            rnd_tp(tr, P_SSWING, 0);
        }
        if (tessera.mode[tr] == TESS_ENG_CHORD || tessera.mode[tr] == TESS_ENG_POLY || tessera.mode[tr] == TESS_ENG_QUAD) {
            if (tessera.mode[tr] == TESS_ENG_CHORD && *chords) (*chords)--;
            rnd_tx(tr, TX_MODE, (int32_t)rnd_pick(gm, gw, NELEM(gm)));
        }
        if (tessera.mode[tr] == TESS_ENG_MARKOV) rnd_tx(tr, TX_MKV_DENS, rnd_rr(15, 50));
        if (tessera.mode[tr] == TESS_ENG_CONSTEL) rnd_tx(tr, TX_CS_DENS, rnd_rr(10, 50));
        if (tessera.mode[tr] == TESS_ENG_AWAKE) rnd_tx(tr, TX_AWK_PROB, rnd_rr(20, 70));
        rnd_tp(tr, P_SDIV, rnd_pick(dv, dw, NELEM(dv)));
        rnd_tx(tr, TX_TW_DROP, rnd_pct(60) ? rnd_rr(10, 50) : 0);
        rnd_tx(tr, TX_NL_MODE, rnd_pct(30) ? 1 : 0);
        if (rnd_fall && rnd_pct(65)) {              /* FALL: BOUNCE's FALL (after ambalek's norns script): leaves */
            rnd_tx(tr, TX_MODE, TESS_ENG_BOUNCE);
            rnd_tx(tr, TX_BNC_BALLS, 5);
            rnd_tx(tr, TX_BNC_SPEED, rnd_rr(4, 20));
            rnd_tx(tr, TX_BNC_ANGLE, rnd_pct(60) ? rnd_rr(35, 55) : rnd_rr(15, 75));   /* (WIND: calm mostly) */
            rnd_tx(tr, TX_TW_DROP, rnd_pct(40) ? rnd_rr(5, 30) : 0);
            rnd_tp(tr, P_SDIV, rnd_pct(60) ? 1 : 0);   /* (1/8 or 1/4) */
        }
    }
}

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
    rnd_tx(tr, TX_MODE, (int32_t)mode);
    switch (mode) {
    case TESS_ENG_TURING:
        rnd_tx(tr, TX_TUR_LEN, rnd_rr(2, 16)); rnd_tx(tr, TX_TUR_PROB, rnd_rr(0, 60)); rnd_tx(tr, TX_TUR_SPAN, rnd_rr(1, 3)); break;
    case TESS_ENG_EUCLID: {
        int32_t st = rnd_rr(3, 32);
        rnd_tx(tr, TX_EUC_STEPS, st); rnd_tx(tr, TX_EUC_PULSES, rnd_rr(1, st * 3 / 5 > 1 ? st * 3 / 5 : 1));
        rnd_tx(tr, TX_EUC_ROT, rnd_rr(0, st - 1)); break;
    }
    case TESS_ENG_MARKOV:
        rnd_tx(tr, TX_MKV_LEAP, rnd_rr(0, 100)); rnd_tx(tr, TX_MKV_DENS, rnd_rr(15, 80)); rnd_tx(tr, TX_MKV_SPAN, rnd_rr(1, 3)); break;
    case TESS_ENG_MORPH:
        rnd_tx(tr, TX_MRP_POS, rnd_rr(0, 127)); rnd_tx(tr, TX_MRP_SNPA, rnd_rr(0, 7)); rnd_tx(tr, TX_MRP_SNPB, rnd_rr(0, 7)); break;
    case TESS_ENG_BOUNCE:
        rnd_tx(tr, TX_BNC_SPEED, rnd_rr(1, 40)); rnd_tx(tr, TX_BNC_ANGLE, rnd_rr(0, 90)); rnd_tx(tr, TX_BNC_BALLS, rnd_rr(1, 4)); break;
    case TESS_ENG_AWAKE:
        rnd_tx(tr, TX_AWK_LENA, rnd_rr(1, 16)); rnd_tx(tr, TX_AWK_LENB, rnd_rr(1, 16)); rnd_tx(tr, TX_AWK_PROB, rnd_rr(20, 90)); break;
    case TESS_ENG_POLY:
        rnd_tx(tr, TX_POL_DIV1, rnd_rr(1, 16)); rnd_tx(tr, TX_POL_DIV2, rnd_rr(0, 16));   /* (two or three rhythms:
                                                                                         * each one a voice) */
        rnd_tx(tr, TX_POL_DIV3, rnd_pct(40) ? rnd_rr(1, 16) : 0); rnd_tx(tr, TX_POL_DIV4, 0);
        rnd_tx(tr, TX_POL_SPAN, rnd_rr(1, 3)); break;
    case TESS_ENG_QUAD:
        rnd_tx(tr, TX_QD_DIR, rnd_rr(0, 3)); rnd_tx(tr, TX_QD_PROB, rnd_rr(20, 90)); rnd_tx(tr, TX_QD_LINK, 0); rnd_tx(tr, TX_QD_RANGE, rnd_rr(0, 3));
        break;
    case TESS_ENG_CONSTEL:
        rnd_tx(tr, TX_CS_Y, rnd_rr(0, 14)); rnd_tx(tr, TX_CS_DENS, rnd_rr(10, 80)); rnd_tx(tr, TX_CS_SIZE, rnd_rr(0, 7));
        rnd_tx(tr, TX_CS_AUTO, rnd_rr(0, 10)); rnd_tx(tr, TX_CS_LEN, rnd_rr(1, 16)); rnd_tx(tr, TX_CS_FILL, rnd_pct(25)); break;
    case TESS_ENG_SERIES:
        rnd_tx(tr, TX_SR_START, rnd_rr(0, 127)); rnd_tx(tr, TX_SR_INTV, rnd_rr(1, 7)); rnd_tx(tr, TX_SR_LOOP, rnd_rr(0, 16)); break;
    case TESS_ENG_CHORD:
        rnd_tx(tr, TX_CH_PROG, rnd_rr(0, 6)); rnd_tx(tr, TX_CH_BARS, rnd_pct(70) ? rnd_rr(2, 3) : rnd_rr(0, 1));
        rnd_tx(tr, TX_CH_HIT, rnd_pct(60) ? 0 : rnd_rr(1, 3)); rnd_tx(tr, TX_CH_OCT, rnd_rr(-1, 1)); rnd_tx(tr, TX_CH_BREATH, rnd_rr(0, 100));
        break;
    default: break;
    }
    rnd_tx(tr, TX_EV_SEED, rnd_rr(1, (int32_t)TESS_SEED_MAX));
    /* the phrase alive: EVOLVE often, a FORM now and then */
    rnd_tx(tr, TX_EV_EVERY, rnd_pct(60) ? rnd_rr(1, 5) : 0);
    rnd_tx(tr, TX_EV_AMOUNT, rnd_rr(10, 70));
    if (mode != TESS_ENG_TURING) rnd_tx(tr, TX_EV_LOCK, rnd_rr(20, 90));   /* (Turing's LOCK is its PROB, set above) */
    rnd_tx(tr, TX_FM_FORM, rnd_pct(25) ? rnd_rr(1, 6) : 0);
    rnd_tx(tr, TX_FM_CONTRAST, rnd_rr(10, 90));
    rnd_tx(tr, TX_FM_DEVELOP, rnd_rr(0, 60));
    rnd_tx(tr, TX_TW_SHIFT, 0);                         /* (the register: TRN above) */
    rnd_tx(tr, TX_TW_DROP, rnd_pct(50) ? rnd_rr(0, 45) : 0);
    rnd_tx(tr, TX_TW_HUMAN, rnd_rr(0, 50));
    rnd_tx(tr, TX_TW_FOLLOW, 0);
    rnd_tx(tr, TX_TW_BRANCH, 0); rnd_tx(tr, TX_TW_BR_TO, 0); rnd_tx(tr, TX_TW_LOGIC, 0); rnd_tx(tr, TX_TW_COND, 0);
    /* how long notes hold: the step's, or long fixed ones for drones */
    rnd_tx(tr, TX_NL_MODE, rnd_pct(40) ? 1 : 0);
    rnd_tx(tr, TX_NL_LEN, rnd_rr(2, 5));
    /* now and then a stutter */
    rnd_tx(tr, TX_SUB_RATCH, rnd_pct(15) ? rnd_rr(1, 5) : 0);
    rnd_tx(tr, TX_SUB_CHANCE, rnd_rr(30, 100));
    rnd_tx(tr, TX_SUB_SHAPE, 0);
    rnd_tx(tr, TX_SUB_HUMAN, rnd_rr(0, 25));
    /* the track's clock: slow by leaning */
    {
        static const uint8_t dv[] = {0, 1, 2, 4, 6, 7}, dw[] = {24, 26, 10, 8, 20, 12};   /* 1/4 1/8 1/16 8T 1/2 1/1 */
        rnd_tp(tr, P_SDIV, rnd_pick(dv, dw, NELEM(dv)));
    }
    rnd_tp(tr, P_SLEN, rnd_rr(8, 32));
    rnd_tp(tr, P_SGATE, rnd_rr(50, 127));
    rnd_tp(tr, P_SSWING, rnd_pct(30) ? rnd_rr(5, 40) : 0);
}

/* ------------------------------------------------------------ GLITCH --- */
static uint32_t rnd_gen_dest(uint32_t tr);
/* GLITCH (after Autechre, Richard Devine): two microsound tracks over two slow ones. The slow: a drone (AMBIENT's) and
 * a slow random or atonal melody (a tone row, QUAD; the infinity series in wide leaps; a wandering TURING or MARKOV),
 * one of them low and one mid-high, whichever. The glitch tracks: fast divisions (1/16, 1/32, triplets) on odd lengths
 * (polymeters), steps dropped, phrases that keep mutating (EVOLVE often), ratchets of up to eight hits (ACCEL, DECEL)
 * on an 1/8 track (one voice: MONO); and the two tied together now and then: one plays only with or against the other
 * (LOGIC), hits branching from one sound to the other (BRANCH), a bar condition (COND). Effects: rnd_fx_mode */
static void rnd_glitch_generator(uint32_t tr, uint32_t role, uint32_t *chords)
{
    uint32_t mode;
    if (role == RND_ROLE_DRONE) {
        rnd_role_generator(tr, RND_ROLE_DRONE, chords);
        return;
    }
    rnd_generator(tr, chords);                      /* (EVOLVE, TWEAK, TIMING as RND's, then:) */
    if (tessera.mode[tr] == TESS_ENG_CHORD && *chords) (*chords)--;
    rnd_tx(tr, TX_FM_FORM, 0);
    rnd_tp(tr, P_SSWING, 0);
    rnd_tx(tr, TX_TW_BRANCH, 0); rnd_tx(tr, TX_TW_BR_TO, 0); rnd_tx(tr, TX_TW_LOGIC, 0); rnd_tx(tr, TX_TW_COND, 0);
    if (role == RND_ROLE_MELODY) {                  /* slow, random or atonal */
        static const uint8_t gm[] = {TESS_ENG_QUAD, TESS_ENG_SERIES, TESS_ENG_TURING, TESS_ENG_MARKOV, TESS_ENG_AWAKE};
        static const uint8_t gw[] = {10, 8, 8, 6, 5};
        static const uint8_t dv[] = {0, 6, 7}, dw[] = {10, 10, 5};   /* 1/4 1/2 1/1 */
        mode = rnd_pick(gm, gw, NELEM(gm));
        rnd_tx(tr, TX_MODE, (int32_t)mode);
        switch (mode) {
        case TESS_ENG_QUAD:                         /* (a tone row: its four forms, folded into up to three octaves) */
            rnd_tx(tr, TX_QD_DIR, rnd_rr(0, 3)); rnd_tx(tr, TX_QD_PROB, rnd_rr(20, 70)); rnd_tx(tr, TX_QD_LINK, 0);
            rnd_tx(tr, TX_QD_RANGE, rnd_rr(0, 3)); break;
        case TESS_ENG_SERIES:
            rnd_tx(tr, TX_SR_START, rnd_rr(0, 127)); rnd_tx(tr, TX_SR_INTV, rnd_rr(3, 7));
            rnd_tx(tr, TX_SR_LOOP, rnd_pct(50) ? 0 : rnd_rr(5, 16)); break;
        case TESS_ENG_TURING:
            rnd_tx(tr, TX_TUR_LEN, rnd_rr(5, 16)); rnd_tx(tr, TX_TUR_PROB, rnd_rr(10, 50)); rnd_tx(tr, TX_TUR_SPAN, 3); break;
        case TESS_ENG_MARKOV:
            rnd_tx(tr, TX_MKV_LEAP, rnd_rr(50, 100)); rnd_tx(tr, TX_MKV_DENS, rnd_rr(20, 50)); rnd_tx(tr, TX_MKV_SPAN, 3); break;
        default:
            rnd_tx(tr, TX_AWK_LENA, rnd_rr(3, 12)); rnd_tx(tr, TX_AWK_LENB, rnd_rr(3, 12)); rnd_tx(tr, TX_AWK_PROB, rnd_rr(30, 70));
            break;
        }
        rnd_tx(tr, TX_SUB_RATCH, 0);
        rnd_tp(tr, P_SDIV, rnd_pick(dv, dw, NELEM(dv)));
        rnd_tx(tr, TX_TW_DROP, rnd_rr(20, 50));     /* (sparse) */
        rnd_tx(tr, TX_NL_MODE, rnd_pct(30) ? 1 : 0);
        rnd_tx(tr, TX_TW_HUMAN, rnd_rr(0, 30));
        return;
    }
    {                                               /* a glitch track */
        static const uint8_t gm[] = {TESS_ENG_EUCLID, TESS_ENG_POLY, TESS_ENG_TURING, TESS_ENG_MARKOV, TESS_ENG_CONSTEL,
                                     TESS_ENG_QUAD, TESS_ENG_BOUNCE};
        static const uint8_t gw[] = {12, 10, 8, 8, 8, 4, 4};
        static const uint8_t dv[] = {2, 5, 3, 1, 4}, dw[] = {12, 6, 4, 7, 3};   /* 1/16 16T 1/32 1/8 8T */
        static const uint8_t odd[] = {5, 7, 9, 11, 13, 15, 17, 19, 23, 29};
        int32_t st;
        mode = rnd_pick(gm, gw, NELEM(gm));
        rnd_tx(tr, TX_MODE, (int32_t)mode);
        switch (mode) {
        case TESS_ENG_EUCLID:
            st = odd[rnd_next() % NELEM(odd)];
            rnd_tx(tr, TX_EUC_STEPS, st); rnd_tx(tr, TX_EUC_PULSES, rnd_rr(2, st * 3 / 4)); rnd_tx(tr, TX_EUC_ROT, rnd_rr(0, st - 1));
            break;
        case TESS_ENG_POLY:
            rnd_tx(tr, TX_POL_DIV1, rnd_rr(3, 13)); rnd_tx(tr, TX_POL_DIV2, rnd_rr(2, 11));
            rnd_tx(tr, TX_POL_DIV3, rnd_pct(40) ? rnd_rr(2, 13) : 0); rnd_tx(tr, TX_POL_DIV4, 0); rnd_tx(tr, TX_POL_SPAN, rnd_rr(1, 3));
            break;
        case TESS_ENG_TURING:
            rnd_tx(tr, TX_TUR_LEN, rnd_rr(3, 13)); rnd_tx(tr, TX_TUR_PROB, rnd_rr(20, 80)); rnd_tx(tr, TX_TUR_SPAN, rnd_rr(2, 3)); break;
        case TESS_ENG_MARKOV:
            rnd_tx(tr, TX_MKV_LEAP, rnd_rr(30, 100)); rnd_tx(tr, TX_MKV_DENS, rnd_rr(40, 90)); rnd_tx(tr, TX_MKV_SPAN, rnd_rr(2, 3)); break;
        case TESS_ENG_CONSTEL:
            rnd_tx(tr, TX_CS_Y, rnd_rr(0, 14)); rnd_tx(tr, TX_CS_DENS, rnd_rr(40, 90)); rnd_tx(tr, TX_CS_SIZE, rnd_rr(0, 7));
            rnd_tx(tr, TX_CS_AUTO, rnd_rr(0, 10)); rnd_tx(tr, TX_CS_LEN, rnd_rr(3, 13)); rnd_tx(tr, TX_CS_FILL, rnd_pct(30)); break;
        case TESS_ENG_QUAD:
            rnd_tx(tr, TX_QD_DIR, rnd_rr(0, 3)); rnd_tx(tr, TX_QD_PROB, rnd_rr(30, 90)); rnd_tx(tr, TX_QD_LINK, 0);
            rnd_tx(tr, TX_QD_RANGE, rnd_rr(0, 3)); break;
        default:
            rnd_tx(tr, TX_BNC_SPEED, rnd_rr(20, 60)); rnd_tx(tr, TX_BNC_ANGLE, rnd_rr(0, 90)); rnd_tx(tr, TX_BNC_BALLS, rnd_rr(1, 4));
            break;
        }
        rnd_tp(tr, P_SDIV, rnd_pick(dv, dw, NELEM(dv)));
        rnd_tp(tr, P_SLEN, odd[rnd_next() % NELEM(odd)]);
        rnd_tp(tr, P_SGATE, rnd_rr(5, 40));
        rnd_tx(tr, TX_NL_MODE, 0);
        rnd_tx(tr, TX_TW_DROP, rnd_pct(55) ? rnd_rr(5, 40) : 0);
        rnd_tx(tr, TX_TW_HUMAN, rnd_rr(0, 20));
        rnd_tx(tr, TX_EV_EVERY, rnd_rr(1, 3));      /* (the phrase never quite the same) */
        rnd_tx(tr, TX_EV_AMOUNT, rnd_rr(30, 80));
        if (mode != TESS_ENG_TURING) rnd_tx(tr, TX_EV_LOCK, rnd_rr(10, 60));
        if (trk[tr].p[P_SDIV] == 1 && rnd_pct(60)) {   /* (an 1/8 track: ratchets, rolls up to eight hits, one voice) */
            rnd_tx(tr, TX_SUB_RATCH, rnd_rr(2, 7));
            rnd_tx(tr, TX_SUB_CHANCE, rnd_rr(15, 60));
            rnd_tx(tr, TX_SUB_SHAPE, rnd_rr(0, 2));
            rnd_tp(tr, P_VOICE, V_MONO);
        } else {
            rnd_tx(tr, TX_SUB_RATCH, 0);
        }
    }
}
/* the two glitch tracks tied together, now and then: b plays with or against a (LOGIC), a's hits branch to b's sound
 * (BRANCH), one of them only in some bars (COND) */
static void rnd_glitch_links(const uint8_t *role)
{
    uint32_t tr, a = NTRK, b = NTRK;
    for (tr = 0; tr < NTRK; tr++)
        if (role[tr] == RND_ROLE_GLITCH) { if (a == NTRK) a = tr; else b = tr; }
    if (b == NTRK) return;
    if (rnd_pct(40)) { rnd_tx(b, TX_TW_LOGIC, rnd_rr(1, 4)); rnd_tx(b, TX_TW_LG_WITH, (int32_t)a + 1); }
    if (rnd_pct(30)) {
        rnd_tx(a, TX_TW_BRANCH, rnd_rr(10, 45)); rnd_tx(a, TX_TW_BR_TO, (int32_t)b + 1); rnd_tx(a, TX_TW_BR_MODE, rnd_rr(0, 2));
    }
    if (rnd_pct(25)) rnd_tx(rnd_pct(50) ? a : b, TX_TW_COND, rnd_rr(1, 10));
}
/* GLITCH's registers: the slow two one low (-24 -12) and one mid-high (+12 +24), which of them dealt at random (the
 * drone as often high as low); the glitch tracks two of the three octaves left */
static void rnd_glitch_registers(const uint8_t *role)
{
    int8_t left[5] = {-24, -12, 0, 12, 24};
    int8_t lo = (int8_t)(rnd_pct(50) ? -24 : -12), hi = (int8_t)(rnd_pct(50) ? 12 : 24);
    uint32_t tr, n = 0, k, low_to_drone = (uint32_t)rnd_pct(50), slow = 0;
    for (tr = 0; tr < NTRK; tr++) {
        if (role[tr] != RND_ROLE_DRONE && role[tr] != RND_ROLE_MELODY) continue;
        rnd_tp(tr, P_TRANS, (role[tr] == RND_ROLE_DRONE) == (low_to_drone != 0u) ? lo : hi);
        slow++;
    }
    (void)slow;
    for (k = 0; k < 5u; k++)
        if (left[k] != lo && left[k] != hi) left[n++] = left[k];
    for (k = n - 1u; k > 0u; k--) {
        uint32_t j = rnd_next() % (k + 1u);
        int8_t v = left[k]; left[k] = left[j]; left[j] = v;
    }
    for (tr = 0, k = 0; tr < NTRK; tr++)
        if (role[tr] == RND_ROLE_GLITCH) rnd_tp(tr, P_TRANS, left[k++ % n]);
}
/* GLITCH's key: no key mostly: chromatic, or an EDO between the semitones (13 17 19 22 31), or a dark scale (PHRY LOC
 * WHOLE DIMHW SLENDR PELOG) */
static void rnd_key_glitch(void)
{
    static const uint8_t sc[] = {0, 20, 24, 26, 29, 35, 8, 10, 13, 14, 60, 61}, sw[] = {14, 3, 3, 3, 3, 2, 3, 3, 2, 3, 2, 2};
    int32_t scale = (int32_t)rnd_pick(sc, sw, NELEM(sc)), root = rnd_rr(0, 11);
    uint32_t tr;
    rnd_tx(0, TX_KEY_LINK, 1);
    for (tr = 0; tr < NTRK; tr++) {
        rnd_tp(tr, P_SCALE, scale);
        rnd_tp(tr, P_ROOT, root);
        rnd_tp(tr, P_QUANT, TP[P_QUANT].def);
    }
    scale_share(&trk[0]);
}
/* GLITCH's cords: four to nine. Into a glitch track fast sources (S&H, RANDOM, the note, the gate, the envelope, fast
 * chaos) on its sound, its pitch a little, its pan and its generator, so no two hits alike; into a slow track slow
 * wanderers on its sound (never its pitch) */
static void rnd_cords_glitch(const uint8_t *role)
{
    static const uint8_t fsrc[] = {CS_SH, CS_RAND, CS_NOTE, CS_GATE, CS_ENV, CS_LFO, CS_OCHD5, CS_OCHD8};
    static const uint8_t fsw[] = {10, 8, 4, 4, 5, 4, 3, 3};
    static const uint8_t fdst[] = {CD_CUTOFF, CD_SHAPE, CD_PITCH, CD_PAN, CD_LFO_RATE, CD_E1, CD_E2, CD_E3, CD_E4, CD_E5,
                                   CD_E6, CD_E7, CD_E8, 0xFF};
    static const uint8_t fdw[] = {10, 8, 5, 6, 3, 5, 5, 5, 5, 4, 4, 3, 3, 8};
    static const uint8_t ssrc[] = {CS_DRIFT, CS_TORPOR, CS_APATHY, CS_INERTIA, CS_LFO}, ssw[] = {10, 5, 5, 5, 5};
    static const uint8_t sdst[] = {CD_CUTOFF, CD_SHAPE, CD_PAN, CD_E1, CD_E2, CD_E3, CD_E4}, sdw[] = {10, 8, 4, 4, 4, 4, 4};
    uint8_t gl[NTRK], sl[NTRK];
    uint32_t i, n = (uint32_t)rnd_rr(4, 9), ng = 0, ns = 0, tr;
    for (tr = 0; tr < NTRK; tr++) {
        if (role[tr] == RND_ROLE_GLITCH) gl[ng++] = (uint8_t)tr;
        else sl[ns++] = (uint8_t)tr;
    }
    for (i = 0; i < CORD_LIVE; i++) {
        cord_t c;
        memset(&c, 0, sizeof c);
        if (i < n && ng) {
            int fast = !ns || rnd_pct(65);
            c.dst_trk = fast ? gl[rnd_next() % ng] : sl[rnd_next() % ns];
            c.src_trk = c.dst_trk;
            if (fast) {
                c.src = (uint8_t)rnd_pick(fsrc, fsw, NELEM(fsrc));
                c.dst = (uint8_t)rnd_pick(fdst, fdw, NELEM(fdst));
                if (c.dst == 0xFFu) c.dst = (uint8_t)rnd_gen_dest(c.dst_trk);
                c.amount = (int8_t)(c.dst == CD_PITCH ? (rnd_pct(50) ? rnd_rr(2, 12) : -rnd_rr(2, 12))
                                                      : (rnd_pct(50) ? rnd_rr(20, 80) : -rnd_rr(20, 80)));
            } else {
                c.src = (uint8_t)rnd_pick(ssrc, ssw, NELEM(ssrc));
                c.dst = (uint8_t)rnd_pick(sdst, sdw, NELEM(sdst));
                c.amount = (int8_t)(rnd_pct(50) ? rnd_rr(10, 45) : -rnd_rr(10, 45));
            }
            c.uni = (uint8_t)rnd_pct(30);
        }
        if (!cord_set(i, &c)) { memset(&c, 0, sizeof c); cord_set(i, &c); }
    }
}

/* ------------------------------------------------------------ PULSE --- */
/* PULSE: electronic dance music, busy and fast. A dance tempo (122-132, 133-145, now and then 160-174); a minor or
 * modal key. The KICK (RND_ROLE_LOWPERC): FM6 on a bass, drop or mallet preset low, the pitch dropping hard, four on
 * the floor or a broken beat (EUCLID, ATLAS), harder hits brighter (VMOD). The HATS (RND_ROLE_NOISE): TRIO's noise
 * through its high- or band-pass (hats, snares, claps by ATLAS' map, offbeat hats), rolls of two to four now and then,
 * a synthesized micro sound seldom. The BASS (RND_ROLE_DRONE in PULSE) low and the RIFF (RND_ROLE_MELODY) above, on
 * TRIO's three triangles or PHYS struck: a short repeating line (TURING locked, AWAKE, MARKOV, EUCLID, BOUNCE) at 1/16
 * or 1/8 that turns every few bars, plucked, pumping under the beat (the LFO a saw at 1/4 on the level, a sidechain's
 * feel). One swing for all four; TWEAK HUMAN's velocity on the drums; cords make each hit its own. Effects:
 * rnd_fx_mode */
static const uint8_t RND_FM6_PERC[] = {2, 10, 28, 29, 35, 5, 12, 34, 32, 21, 19, 13, 7};
static const uint8_t RND_FM6_PERC_W[] = {6, 5, 4, 3, 2, 2, 2, 1, 1, 1, 1, 1, 1};
/* FM BASS, SOLID BASS, DROP, MALLET, MOKUGYO, MARIMBA, DX MARIMBA, HYOSHIGI, BAMBOO, STEEL DRUM, KALIMBA, CLAVINET,
 * PLUCK */
static void rnd_pulse_sound(uint32_t tr, uint32_t role, uint32_t *fm6s, uint32_t *phys, uint32_t *oracles)
{
    uint8_t nice = rnd_nice;
    int slow = role == RND_ROLE_DRONE || role == RND_ROLE_MELODY;
    if (role == RND_ROLE_LOWPERC) {
        rnd_want_e = eng_ok(ENGI_FM6) && !*fm6s ? ENGI_FM6 : ENGI_PHYS;
        rnd_want_pi = rnd_want_e == ENGI_FM6 ? (uint8_t)rnd_pick(RND_FM6_PERC, RND_FM6_PERC_W, NELEM(RND_FM6_PERC)) : 0xFFu;
    } else if (role == RND_ROLE_NOISE) {
        rnd_want_e = 6u;
        rnd_want_pi = 0xFFu;
    } else {                                        /* (the bass and the riff: triangles or PHYS, one PHYS at most) */
        rnd_want_e = !*phys && eng_ok(ENGI_PHYS) && rnd_pct(role == RND_ROLE_DRONE ? 35 : 45) ? ENGI_PHYS : 6u;
        rnd_want_pi = 0xFFu;
        rnd_nice = 1;                               /* (NICE's still and clean voices) */
    }
    rnd_sound(tr, fm6s, phys, oracles, role);
    rnd_nice = nice;
    rnd_want_e = rnd_want_pi = 0xFFu;
    if (trk[tr].eng_req == ENGI_PHYS) rnd_tp(tr, P_E0 + 6u, rnd_pct(30) ? rnd_rr(10, 40) : 0);   /* (PHYS struck; a
                                                     * little bow's noise now and then) */
    if (!slow) return;
    if (trk[tr].eng_req == 6u && rnd_pct(40)) {     /* (TRIO: the triangles roughed up: noise in, or ring, or sync) */
        static const uint8_t wv[] = {7, 13, 14, 9}, ww[] = {4, 3, 2, 2};   /* P+N RING3 R+S SYNC */
        rnd_tp(tr, P_E0, rnd_pick(wv, ww, NELEM(wv)));
        rnd_tp(tr, P_E0 + 6u, rnd_rr(20, 80));      /* RES */
    }
    rnd_tp(tr, P_ATK, rnd_rr(0, 4));                /* (plucked: the bass, the riff) */
    rnd_tp(tr, P_DEC, rnd_rr(20, 70));
    rnd_tp(tr, P_SUS, role == RND_ROLE_DRONE ? rnd_rr(20, 90) : rnd_rr(0, 40));
    rnd_tp(tr, P_REL, rnd_rr(10, 50));
    rnd_tp(tr, P_ED_FLT, rnd_rr(15, 60));
    rnd_tp(tr, P_ED_PIT, 0);
    rnd_tp(tr, P_VOICE, role == RND_ROLE_DRONE ? V_MONO : rnd_pct(50) ? V_MONO : V_POLY);
    rnd_tp(tr, P_GLIDE, role == RND_ROLE_DRONE && rnd_pct(20) ? rnd_rr(5, 25) : 0);
    rnd_tp(tr, P_LEVEL, role == RND_ROLE_DRONE ? rnd_rr(84, 98) : rnd_rr(70, 88));
    rnd_tp(tr, P_PAN, role == RND_ROLE_DRONE ? 0 : rnd_rr(-35, 35));
    if (rnd_pct(role == RND_ROLE_DRONE ? 65 : 40)) {   /* (the pump: a saw at 1/4 on the level) */
        rnd_tp(tr, P_LWAVE, 2);
        rnd_tp(tr, P_LSYNC, 5);
        rnd_tp(tr, P_LD_AMP, rnd_rr(40, 85));
        rnd_tp(tr, P_LD_PIT, 0);
        rnd_tp(tr, P_LFADE, 0);
        rnd_tp(tr, P_LTRIG, 0);
    }
}
static void rnd_pulse_generator(uint32_t tr, uint32_t role, uint32_t *chords)
{
    uint32_t mode;
    int32_t st;
    rnd_generator(tr, chords);
    if (tessera.mode[tr] == TESS_ENG_CHORD && *chords) (*chords)--;
    rnd_tx(tr, TX_FM_FORM, 0);
    rnd_tx(tr, TX_TW_BRANCH, 0); rnd_tx(tr, TX_TW_BR_TO, 0); rnd_tx(tr, TX_TW_LOGIC, 0); rnd_tx(tr, TX_TW_COND, 0);
    rnd_tx(tr, TX_SUB_RATCH, 0);
    rnd_tx(tr, TX_NL_MODE, 0);
    rnd_tx(tr, TX_TW_DROP, 0);
    rnd_tp(tr, P_SLEN, 16);
    rnd_tx(tr, TX_EV_EVERY, rnd_rr(2, 6));          /* (a phrase a few bars, then it turns) */
    rnd_tx(tr, TX_EV_AMOUNT, rnd_rr(10, 30));
    if (role == RND_ROLE_LOWPERC) {                 /* the kick: four on the floor, or broken */
        uint32_t k = (uint32_t)rnd_rr(0, 99);
        if (k < 45u || k >= 70u) {
            static const uint8_t br[] = {3, 5, 6, 7};
            mode = TESS_ENG_EUCLID;
            rnd_tx(tr, TX_MODE, (int32_t)mode);
            rnd_tx(tr, TX_EUC_STEPS, 16);
            rnd_tx(tr, TX_EUC_PULSES, k < 45u ? 4 : br[rnd_next() % NELEM(br)]);
            rnd_tx(tr, TX_EUC_ROT, k < 45u ? 0 : rnd_rr(0, 15));
        } else {
            mode = TESS_ENG_GRIDS;
            rnd_tx(tr, TX_MODE, (int32_t)mode);
            rnd_tx(tr, TX_GRD_X, rnd_rr(0, 255)); rnd_tx(tr, TX_GRD_Y, rnd_rr(0, 255)); rnd_tx(tr, TX_GRD_DENS, rnd_rr(40, 70));
            rnd_tx(tr, TX_GRD_KICK, 100); rnd_tx(tr, TX_GRD_SNR, rnd_rr(0, 30)); rnd_tx(tr, TX_GRD_HAT, 0);
            rnd_tx(tr, TX_GRD_TOMS, rnd_rr(0, 40)); rnd_tx(tr, TX_GRD_PART4, 0);
        }
        rnd_tp(tr, P_SDIV, 2);
        rnd_tp(tr, P_SGATE, rnd_rr(30, 60));
        rnd_tx(tr, TX_TW_HUMAN, rnd_rr(20, 60));
    } else if (role == RND_ROLE_NOISE) {            /* the hats: ATLAS' hats and snares, sixteenths, offbeats */
        uint32_t k = (uint32_t)rnd_rr(0, 99);
        if (k < 55u) {
            mode = TESS_ENG_GRIDS;
            rnd_tx(tr, TX_MODE, (int32_t)mode);
            rnd_tx(tr, TX_GRD_X, rnd_rr(0, 255)); rnd_tx(tr, TX_GRD_Y, rnd_rr(0, 255)); rnd_tx(tr, TX_GRD_DENS, rnd_rr(50, 85));
            rnd_tx(tr, TX_GRD_KICK, 0); rnd_tx(tr, TX_GRD_SNR, rnd_rr(60, 100)); rnd_tx(tr, TX_GRD_HAT, 100);
            rnd_tx(tr, TX_GRD_TOMS, rnd_rr(0, 40)); rnd_tx(tr, TX_GRD_PART4, rnd_rr(1, 3));
        } else {
            static const uint8_t ln[] = {16, 12, 10, 14, 9, 7, 13}, lw[] = {4, 3, 2, 2, 2, 1, 1};
            int32_t steps = k < 80u ? (int32_t)rnd_pick(ln, lw, NELEM(ln)) : 16;
            mode = TESS_ENG_EUCLID;
            rnd_tx(tr, TX_MODE, (int32_t)mode);
            rnd_tx(tr, TX_EUC_STEPS, steps);        /* (a cycle of its own against the kick's 16: a polyrhythm) */
            rnd_tx(tr, TX_EUC_PULSES, k < 80u ? rnd_rr(steps / 2, steps * 4 / 5) : 4);
            rnd_tx(tr, TX_EUC_ROT, k < 80u ? rnd_rr(0, steps - 1) : 2);   /* (4, turned by 2: the offbeat) */
            if (k >= 80u) rnd_tp(tr, P_DEC, rnd_rr(30, 50));         /* (an open hat) */
        }
        rnd_tp(tr, P_SDIV, rnd_pct(30) ? 5 : 2);    /* (16T now and then: three against the kick's four) */
        rnd_tp(tr, P_SGATE, rnd_rr(20, 60));
        rnd_tx(tr, TX_TW_HUMAN, rnd_rr(50, 100));
        if (rnd_pct(40)) {                          /* (rolls of two to four now and then; TRIO, one voice) */
            rnd_tx(tr, TX_SUB_RATCH, rnd_rr(1, 3));
            rnd_tx(tr, TX_SUB_CHANCE, rnd_rr(5, 15));
            rnd_tx(tr, TX_SUB_SHAPE, rnd_rr(0, 2));
            rnd_tp(tr, P_VOICE, V_MONO);
        }
    } else if (role == RND_ROLE_DRONE) {            /* the bass: a short line, offbeat, rolling, or a cycle of its own */
        uint32_t k = (uint32_t)rnd_rr(0, 99);
        if (k < 30u) {
            st = rnd_pct(40) ? 4 : rnd_rr(4, 9);
            mode = TESS_ENG_EUCLID;
            rnd_tx(tr, TX_MODE, (int32_t)mode);
            rnd_tx(tr, TX_EUC_STEPS, 16); rnd_tx(tr, TX_EUC_PULSES, st);
            rnd_tx(tr, TX_EUC_ROT, st == 4 ? 2 : rnd_rr(0, 15));
        } else if (k < 55u) {                       /* (3 in 8, 5 in 12, 4 in 10 ..: against the kick) */
            static const uint8_t ln[] = {6, 10, 12, 7, 9, 5}, lw[] = {3, 3, 3, 2, 2, 1};
            int32_t steps = (int32_t)rnd_pick(ln, lw, NELEM(ln));
            mode = TESS_ENG_EUCLID;
            rnd_tx(tr, TX_MODE, (int32_t)mode);
            rnd_tx(tr, TX_EUC_STEPS, steps); rnd_tx(tr, TX_EUC_PULSES, rnd_rr(2, steps * 2 / 3));
            rnd_tx(tr, TX_EUC_ROT, rnd_rr(0, steps - 1));
        } else if (k < 70u) {                       /* (POLY: two or three pulses of their own lengths) */
            mode = TESS_ENG_POLY;
            rnd_tx(tr, TX_MODE, (int32_t)mode);
            rnd_tx(tr, TX_POL_DIV1, rnd_rr(3, 7)); rnd_tx(tr, TX_POL_DIV2, rnd_rr(2, 5));
            rnd_tx(tr, TX_POL_DIV3, rnd_pct(40) ? rnd_rr(5, 9) : 0); rnd_tx(tr, TX_POL_DIV4, 0); rnd_tx(tr, TX_POL_SPAN, 1);
        } else {
            mode = TESS_ENG_TURING;
            rnd_tx(tr, TX_MODE, (int32_t)mode);
            rnd_tx(tr, TX_TUR_LEN, rnd_pct(50) ? 8 : rnd_pct(50) ? 16 : rnd_rr(5, 13)); rnd_tx(tr, TX_TUR_PROB, rnd_rr(0, 12));
            rnd_tx(tr, TX_TUR_SPAN, 1);
        }
        rnd_tp(tr, P_SDIV, rnd_pct(70) ? 2 : 1);
        if (rnd_pct(35)) rnd_tp(tr, P_SLEN, rnd_rr(5, 14));   /* (the loop its own length) */
        rnd_tp(tr, P_SGATE, rnd_rr(30, 80));
        rnd_tx(tr, TX_TW_HUMAN, rnd_rr(0, 30));
    } else {                                        /* the riff: a short repeating line that turns */
        static const uint8_t gm[] = {TESS_ENG_TURING, TESS_ENG_AWAKE, TESS_ENG_MARKOV, TESS_ENG_BOUNCE, TESS_ENG_POLY};
        static const uint8_t gw[] = {30, 20, 15, 15, 20};
        static const uint8_t dv[] = {2, 1, 5, 4}, dw[] = {45, 25, 20, 10};   /* 1/16 1/8 16T 8T */
        mode = rnd_pick(gm, gw, NELEM(gm));
        rnd_tx(tr, TX_MODE, (int32_t)mode);
        switch (mode) {
        case TESS_ENG_TURING:
            rnd_tx(tr, TX_TUR_LEN, (int32_t)(4u << (rnd_next() % 3u))); rnd_tx(tr, TX_TUR_PROB, rnd_rr(0, 15));
            rnd_tx(tr, TX_TUR_SPAN, rnd_rr(1, 2)); break;
        case TESS_ENG_AWAKE:
            rnd_tx(tr, TX_AWK_LENA, rnd_rr(4, 8)); rnd_tx(tr, TX_AWK_LENB, rnd_rr(3, 7)); rnd_tx(tr, TX_AWK_PROB, rnd_rr(20, 50));
            break;
        case TESS_ENG_MARKOV:
            rnd_tx(tr, TX_MKV_LEAP, rnd_rr(20, 60)); rnd_tx(tr, TX_MKV_DENS, rnd_rr(50, 90)); rnd_tx(tr, TX_MKV_SPAN, rnd_rr(1, 2));
            break;
        case TESS_ENG_POLY:                         /* (pulses of their own lengths, each a pitch) */
            rnd_tx(tr, TX_POL_DIV1, rnd_rr(3, 9)); rnd_tx(tr, TX_POL_DIV2, rnd_rr(2, 7));
            rnd_tx(tr, TX_POL_DIV3, rnd_pct(50) ? rnd_rr(4, 11) : 0); rnd_tx(tr, TX_POL_DIV4, 0);
            rnd_tx(tr, TX_POL_SPAN, rnd_rr(1, 2)); break;
        default:
            rnd_tx(tr, TX_BNC_BALLS, rnd_rr(1, 3)); rnd_tx(tr, TX_BNC_SPEED, rnd_rr(25, 60)); rnd_tx(tr, TX_BNC_ANGLE, rnd_rr(15, 75));
            break;
        }
        rnd_tp(tr, P_SDIV, rnd_pick(dv, dw, NELEM(dv)));
        if (rnd_pct(50)) rnd_tp(tr, P_SLEN, rnd_pick((const uint8_t[]){5, 7, 9, 10, 12, 13, 14}, (const uint8_t[]){3, 3, 2, 2, 3, 1, 1}, 7));
        rnd_tp(tr, P_SGATE, rnd_rr(25, 70));
        rnd_tx(tr, TX_TW_HUMAN, rnd_rr(0, 40));
    }
}
/* the noise where the low hits are not (LOGIC NOT), a condition now and then */
static void rnd_pulse_links(const uint8_t *role)
{
    uint32_t tr, lo = NTRK, nz = NTRK;
    for (tr = 0; tr < NTRK; tr++) {
        if (role[tr] == RND_ROLE_LOWPERC) lo = tr;
        if (role[tr] == RND_ROLE_NOISE) nz = tr;
    }
    if (lo == NTRK || nz == NTRK) return;
    if (rnd_pct(20)) { rnd_tx(nz, TX_TW_LOGIC, 4); rnd_tx(nz, TX_TW_LG_WITH, (int32_t)lo + 1); }   /* (NOT) */
}
/* PULSE's registers: the kick -24 or -12 and the bass the other, the riff +12 or +24, the hats 0 or the high left; and
 * its tempo: a dance tempo, one swing for all four */
static void rnd_pulse_registers(const uint8_t *role)
{
    int32_t lp = rnd_pct(70) ? -24 : -12, sl = lp == -24 ? -12 : -24, sh = rnd_pct(50) ? 12 : 24, nz = rnd_pct(50) ? 0 : 36 - sh;
    uint32_t tr, k = (uint32_t)rnd_rr(0, 99);
    int32_t swing = rnd_pct(55) ? 0 : rnd_rr(10, 35);
    for (tr = 0; tr < NTRK; tr++) {
        rnd_tp(tr, P_TRANS, role[tr] == RND_ROLE_LOWPERC ? lp : role[tr] == RND_ROLE_NOISE ? nz :
                            role[tr] == RND_ROLE_DRONE ? sl : sh);
        rnd_tp(tr, P_SSWING, swing);
    }
    pid_set(PID_GLOBAL(G_BPM), 0, k < 50u ? rnd_rr(122, 132) : k < 80u ? rnd_rr(133, 145) : rnd_rr(160, 174));
}
/* PULSE's key: a minor or modal one (MIN DOR PHRY HARM MPEN MIX) */
static void rnd_key_pulse(void)
{
    static const uint8_t sc[] = {2, 3, 8, 7, 6, 4}, sw[] = {8, 6, 3, 2, 3, 1};
    int32_t scale = (int32_t)rnd_pick(sc, sw, NELEM(sc)), root = rnd_rr(0, 11);
    uint32_t tr;
    rnd_tx(0, TX_KEY_LINK, 1);
    for (tr = 0; tr < NTRK; tr++) {
        rnd_tp(tr, P_SCALE, scale);
        rnd_tp(tr, P_ROOT, root);
        rnd_tp(tr, P_QUANT, TP[P_QUANT].def);
    }
    scale_share(&trk[0]);
}
/* PULSE's cords: each hit its own (velocity into the decay and the brightness, RANDOM into the drop, the gain, the
 * cutoff, S&H the pan), the slow two wandering slowly */
static void rnd_cords_pulse(const uint8_t *role)
{
    static const uint8_t lsrc[] = {CS_VEL, CS_VEL, CS_RAND, CS_RAND, CS_RAND, CS_SH};
    static const uint8_t ldst[] = {CD_DECAY, CD_E2, CD_ENV_PIT, CD_GAIN, CD_E5, CD_E4};   /* (FM6: MLVL, MEG) */
    static const uint8_t nsrc[] = {CS_VEL, CS_RAND, CS_RAND, CS_SH, CS_RAND};
    static const uint8_t ndst[] = {CD_CUTOFF, CD_DECAY, CD_CUTOFF, CD_PAN, CD_GAIN};
    static const uint8_t ssrc[] = {CS_DRIFT, CS_TORPOR, CS_APATHY, CS_INERTIA}, ssw[] = {10, 5, 5, 5};
    static const uint8_t sdst[] = {CD_CUTOFF, CD_SHAPE, CD_PAN}, sdw[] = {10, 5, 3};   /* (never TRIO's intervals) */
    uint32_t i = 0, k, tr;
    cord_t c;
    for (tr = 0; tr < NTRK; tr++) {
        uint32_t n = role[tr] == RND_ROLE_LOWPERC || role[tr] == RND_ROLE_NOISE ? (uint32_t)rnd_rr(2, 3) : rnd_pct(60) ? 1u : 0u;
        uint8_t used = 0;
        for (k = 0; k < n && i < CORD_LIVE; k++) {
            uint32_t j;
            memset(&c, 0, sizeof c);
            c.src_trk = c.dst_trk = (uint8_t)tr;
            if (role[tr] == RND_ROLE_LOWPERC || role[tr] == RND_ROLE_NOISE) {
                const uint8_t *src = role[tr] == RND_ROLE_LOWPERC ? lsrc : nsrc, *dst = role[tr] == RND_ROLE_LOWPERC ? ldst : ndst;
                uint32_t m = role[tr] == RND_ROLE_LOWPERC ? NELEM(lsrc) : NELEM(nsrc);
                do j = rnd_next() % m; while ((used >> j) & 1u);
                used |= (uint8_t)(1u << j);
                c.src = src[j];
                c.dst = dst[j];
                if (trk[tr].eng_req != ENGI_FM6 && (c.dst == CD_E2 || c.dst == CD_E4 || c.dst == CD_E5)) c.dst = CD_CUTOFF;
                c.amount = (int8_t)(c.src == CS_VEL ? rnd_rr(20, 50) : c.dst == CD_ENV_PIT ? rnd_rr(10, 30) :
                                    rnd_pct(50) ? rnd_rr(15, 40) : -rnd_rr(15, 40));
            } else {
                c.src = (uint8_t)rnd_pick(ssrc, ssw, NELEM(ssrc));
                c.dst = (uint8_t)rnd_pick(sdst, sdw, NELEM(sdst));
                c.amount = (int8_t)(rnd_pct(50) ? rnd_rr(10, 35) : -rnd_rr(10, 35));
            }
            if (cord_set(i, &c)) i++;
        }
    }
    for (memset(&c, 0, sizeof c); i < CORD_LIVE; i++) cord_set(i, &c);
}
/* PULSE's four: a drone, a melody, the low percussion, the noise, in a random order */
static void rnd_roles_pulse(uint8_t *role)
{
    uint32_t k;
    role[0] = RND_ROLE_DRONE; role[1] = RND_ROLE_MELODY; role[2] = RND_ROLE_LOWPERC; role[3] = RND_ROLE_NOISE;
    for (k = NTRK - 1u; k > 0u; k--) {
        uint32_t j = rnd_next() % (k + 1u);
        uint8_t v = role[k]; role[k] = role[j]; role[j] = v;
    }
}

/* ------------------------------------------------------------ WOZZA MOZZA --- */
/* WOZZA MOZZA: minimal piano and synth, low-key and worn, from five reference tracks measured on the PC (their tempo,
 * keys, timing, timbre and lo-fi): slow (72-84 BPM), plain keys (major, the pentatonics, now and then minor, dorian or
 * lydian), dark (little above 2 kHz), off the grid, tape-worn. The BED (RND_ROLE_DRONE): a dark pad or organ moving
 * every bar to four, legato, now and then a slow tremolo. The OSTINATO (RND_ROLE_OSTI): two notes taking turns
 * (SERIES, two steps), a beat or two each. The MELODY: an itty bitty line on three to five notes, repeats, short phrases
 * and gaps (TURING short and nearly locked, AWAKE, MARKOV sparse; DROP), piano mostly (PHYS' stiff string hammered,
 * FM6's tines), the ostinato a low piano when one is left. The
 * TEXTURE (RND_ROLE_NOISE): NOISE's hiss, rumble and crackle held low under it all; or, now and then, a second melody
 * high up and sparser. The pitched voices wobble as worn tape (the LFO a sine at about 2.5 Hz on the pitch: the
 * references' wow); each step late by a player's amount (TIMING HUMAN). Effects: rnd_fx_wozza */
#define RND_WZ_WOW 75                               /* (LFO RATE: about 2.5 Hz) */
/* PHYS as a piano: the string model stiff (STRC past a quarter: dispersion, piano-like), hammered near its end, a
 * soft tone, a long decay, the hammer's thump a little (EXC); never bowed. A preset's knobs, set whatever it was */
static void rnd_wozza_piano(uint32_t tr)
{
    if (trk[tr].p[P_E0] != PM_STRING) return;       /* (VIBES, KALIMBA, MARIMBA: bars, as they are) */
    rnd_tp(tr, P_E0 + 1u, rnd_rr(70, 105));         /* STRC: stiff */
    rnd_tp(tr, P_E0 + 2u, rnd_rr(35, 65));          /* BRIT: soft, felt */
    rnd_tp(tr, P_E0 + 3u, rnd_rr(80, 102));         /* DAMP: rings on */
    rnd_tp(tr, P_E0 + 4u, rnd_rr(12, 22));          /* POS: near the end, as a hammer */
    rnd_tp(tr, P_E0 + 5u, rnd_rr(85, 110));         /* ACC */
    rnd_tp(tr, P_E0 + 6u, 0);                       /* BOW off */
    rnd_tp(tr, P_E0 + 7u, rnd_rr(5, 25));           /* EXC: the hammer */
}
static void rnd_wozza_sound(uint32_t tr, uint32_t role, uint32_t *fm6s, uint32_t *phys, uint32_t *oracles)
{
    uint8_t nice = rnd_nice;
    uint32_t e;
    if (role == RND_ROLE_NOISE) {
        rnd_want_e = eng_ok(11u) ? 11u : 6u;        /* (NOISE; TRIO's noise wave without it) */
        rnd_want_pi = 0;
    } else if (role == RND_ROLE_MELODY) {           /* piano mostly: PHYS' hammered string, FM6's tines */
        static const uint8_t fp[] = {0, 8, 38, 19}, fw[] = {6, 5, 1, 1};   /* TINE EP, DX TINE, KOTO WARM, KALIMBA */
        static const uint8_t pp[] = {9, 4, 1}, pw[] = {2, 1, 1};           /* VIBES, KALIMBA, MARIMBA */
        uint32_t k = (uint32_t)rnd_rr(0, 99);
        int ph = !*phys && eng_ok(ENGI_PHYS), fm = !*fm6s && eng_ok(ENGI_FM6);
        if (ph && (k < 50u || !fm)) rnd_want_e = ENGI_PHYS, rnd_want_pi = k < 92u || !fm ? 12 : rnd_pick(pp, pw, NELEM(pp));
        else if (fm) rnd_want_e = ENGI_FM6, rnd_want_pi = k < 90u ? (rnd_pct(55) ? 0 : 8) : rnd_pick(fp, fw, NELEM(fp));
        else rnd_want_e = 6u, rnd_want_pi = 10;     /* (TRIO's SOFT LEAD) */
    } else if (role == RND_ROLE_OSTI) {             /* a low piano when one is left; else TRIO's pads plucked */
        if (!*phys && eng_ok(ENGI_PHYS) && rnd_pct(75)) rnd_want_e = ENGI_PHYS, rnd_want_pi = 12;
        else if (!*fm6s && eng_ok(ENGI_FM6) && rnd_pct(75)) rnd_want_e = ENGI_FM6, rnd_want_pi = 0;
        else if (rnd_pct(30) && eng_ok(7u)) rnd_want_e = 7u, rnd_want_pi = 3;   /* (WHEEL's SOFT FLUTE, where built) */
        else rnd_want_e = 6u, rnd_want_pi = rnd_pct(50) ? 8 : 5;        /* (SOFT PAD, WARM PAD) */
    } else {                                        /* the bed: a dark pad (TRIO: the pianos keep FM6 and PHYS) */
        static const uint8_t tp[] = {5, 7, 8, 14, 6}, tw[] = {4, 3, 3, 2, 2};    /* WARM HOLLOW SOFT PAD, FIFTH DRONE, STRING MACH */
        if (rnd_pct(30) && eng_ok(7u)) rnd_want_e = 7u, rnd_want_pi = rnd_pct(60) ? 3 : 0;   /* (WHEEL, where built) */
        else rnd_want_e = 6u, rnd_want_pi = rnd_pick(tp, tw, NELEM(tp));
    }
    rnd_nice = 1;                                   /* (the presets' kinds kept, their knobs pushed a little) */
    rnd_sound(tr, fm6s, phys, oracles, RND_ROLE_FREE);
    rnd_nice = nice;
    rnd_want_e = rnd_want_pi = 0xFFu;
    e = trk[tr].eng_req;
    rnd_tp(tr, P_ED_PIT, 0);
    rnd_tp(tr, P_ED_SHP, 0);
    rnd_tp(tr, P_ED_FLT, rnd_rr(-10, 20));
    rnd_tp(tr, P_LD_FLT, 0);
    rnd_tp(tr, P_LD_SHP, 0);
    rnd_tp(tr, P_LD_AMP, 0);
    rnd_tp(tr, P_LWAVE, 0);                          /* (a sine: the wow) */
    rnd_tp(tr, P_LSYNC, 0);
    rnd_tp(tr, P_LTRIG, 0);
    rnd_tp(tr, P_LFADE, 0);
    rnd_tp(tr, P_LRATE, RND_WZ_WOW + rnd_rr(-3, 3));
    rnd_tp(tr, P_LD_PIT, role == RND_ROLE_NOISE ? 0 : rnd_pct(80) ? 1 : 0);   /* (a fifth of a semitone: worn tape) */
    rnd_tp(tr, P_GLIDE, 0);
    rnd_tp(tr, P_CHRD, 0);
    rnd_tp(tr, P_DETUNE, rnd_rr(0, 40));
    rnd_tp(tr, P_SPRD, rnd_rr(0, 60));
    if (role == RND_ROLE_NOISE) {                   /* hiss, rumble, crackle: held, low, dark */
        if (e == 11u) {
            rnd_tp(tr, P_E0, rnd_pct(80) ? 0 : 1);  /* MODE ANLG (DUST: the crackle alone) */
            rnd_tp(tr, P_E0 + 1u, rnd_rr(70, 127)); /* COLR: pink to brown (the rumble) */
            rnd_tp(tr, P_E0 + 2u, rnd_rr(55, 85));  /* FREQ */
            rnd_tp(tr, P_E0 + 3u, rnd_rr(0, 25));   /* RES */
            rnd_tp(tr, P_E0 + 4u, 0);               /* TRK: the same whatever the note */
            rnd_tp(tr, P_E0 + 5u, rnd_pct(65) ? rnd_rr(10, 60) : 0);   /* DENS: crackle */
            rnd_tp(tr, P_E0 + 6u, rnd_rr(20, 70));  /* DRFT */
            rnd_tp(tr, P_E0 + 7u, rnd_pct(30) ? rnd_rr(5, 30) : 0);    /* CRSH */
        } else {
            rnd_tp(tr, P_E0, 8);                    /* (TRIO: NOIS; NOISE is retired on the FM-1's build) */
            rnd_tp(tr, P_E0 + 5u, rnd_pct(50) ? rnd_rr(25, 45) : rnd_rr(45, 75));   /* (CUT: a rumble, or a hiss) */
            rnd_tp(tr, P_E0 + 6u, rnd_rr(0, 30));
        }
        rnd_tp(tr, P_ATK, rnd_rr(90, 120));
        rnd_tp(tr, P_DEC, 127);
        rnd_tp(tr, P_SUS, 127);
        rnd_tp(tr, P_REL, rnd_rr(90, 110));
        rnd_tp(tr, P_VOICE, V_LEGATO);
        rnd_tp(tr, P_LEVEL, rnd_rr(50, 66));
        rnd_tp(tr, P_PAN, 0);
        rnd_tp(tr, P_SPRD, rnd_rr(60, 127));
        return;
    }
    if (role == RND_ROLE_DRONE) {                   /* the bed: swells, holds, never quite stops */
        rnd_tp(tr, P_ATK, rnd_rr(70, 115));
        rnd_tp(tr, P_DEC, rnd_rr(90, 127));
        rnd_tp(tr, P_SUS, rnd_rr(100, 127));
        rnd_tp(tr, P_REL, rnd_rr(90, 115));
        rnd_tp(tr, P_VOICE, V_LEGATO);
        rnd_tp(tr, P_GLIDE, rnd_pct(30) ? rnd_rr(20, 60) : 0);
        rnd_tp(tr, P_LEVEL, rnd_rr(74, 88));
        rnd_tp(tr, P_PAN, rnd_rr(-10, 10));
        if (rnd_pct(35)) {                          /* (a slow tremolo: 1.3 to 5.5 Hz, the references') */
            rnd_tp(tr, P_LD_PIT, 0);
            rnd_tp(tr, P_LRATE, rnd_rr(66, 84));
            rnd_tp(tr, P_LD_AMP, rnd_rr(25, 60));
        }
        if (e == 6u) rnd_tp(tr, P_E0 + 5u, rnd_rr(40, 70));   /* (TRIO's CUT: dark) */
        if (e == ENGI_FM6) rnd_tp(tr, P_E0 + 7u, rnd_rr(10, 24));
        if (e == 7u) { rnd_tp(tr, P_E0 + 3u, rnd_rr(0, 30)); rnd_tp(tr, P_E0 + 5u, 0); }   /* (WHEEL: TOP low, no CLICK) */
        return;
    }
    if (role == RND_ROLE_OSTI) {                    /* soft, round, a little held */
        rnd_tp(tr, P_ATK, rnd_rr(2, 30));
        rnd_tp(tr, P_DEC, rnd_rr(60, 100));
        rnd_tp(tr, P_SUS, rnd_rr(30, 90));
        rnd_tp(tr, P_REL, rnd_rr(50, 85));
        rnd_tp(tr, P_VOICE, rnd_pct(60) ? V_MONO : V_POLY);
        rnd_tp(tr, P_LEVEL, rnd_rr(72, 86));
        rnd_tp(tr, P_PAN, rnd_rr(-25, 25));
        if (e == 6u) rnd_tp(tr, P_E0 + 5u, rnd_rr(35, 65));
        if (e == 7u) { rnd_tp(tr, P_E0 + 3u, rnd_rr(0, 20)); rnd_tp(tr, P_E0 + 5u, 0); }
        if (e == ENGI_FM6) rnd_tp(tr, P_E0 + 2u, rnd_rr(-20, 5));   /* (MLVL: mellow) */
        if (e == ENGI_PHYS) {                       /* (the low piano: struck, ringing) */
            rnd_wozza_piano(tr);
            rnd_tp(tr, P_ATK, 0);
            rnd_tp(tr, P_DEC, rnd_rr(80, 110));
            rnd_tp(tr, P_SUS, rnd_rr(0, 30));
            rnd_tp(tr, P_VOICE, V_POLY);
        }
        return;
    }
    rnd_tp(tr, P_ATK, rnd_rr(0, 3));                /* the melody: struck, ringing */
    rnd_tp(tr, P_DEC, rnd_rr(80, 115));
    rnd_tp(tr, P_SUS, rnd_rr(0, 40));
    rnd_tp(tr, P_REL, rnd_rr(70, 95));
    rnd_tp(tr, P_VOICE, V_POLY);
    rnd_tp(tr, P_LEVEL, rnd_rr(76, 92));
    rnd_tp(tr, P_PAN, rnd_rr(-35, 35));
    if (e == ENGI_FM6) { rnd_tp(tr, P_E0 + 2u, rnd_rr(-25, 5)); rnd_tp(tr, P_E0 + 7u, rnd_rr(0, 20)); }   /* (MLVL mellow, AEG
                                                     * ringing) */
    if (e == ENGI_PHYS) rnd_wozza_piano(tr);
    if (e == 6u) rnd_tp(tr, P_E0 + 5u, rnd_rr(45, 70));
}
/* an itty bitty line: a few notes, repeats, phrases and gaps, a player's timing */
static void rnd_wozza_line(uint32_t tr, int sparse)
{
    static const uint8_t dv[] = {1, 2, 4, 0}, dw[] = {55, 20, 10, 15};   /* 1/8 1/16 8T 1/4 */
    uint32_t k = (uint32_t)rnd_rr(0, 99);
    if (k < 50u) {                                  /* (TURING: a short loop, nearly locked, on an octave) */
        rnd_tx(tr, TX_MODE, TESS_ENG_TURING);
        rnd_tx(tr, TX_TUR_LEN, rnd_rr(3, 7));
        rnd_tx(tr, TX_TUR_PROB, rnd_rr(0, 12));
        rnd_tx(tr, TX_TUR_SPAN, 1);
    } else if (k < 80u) {                           /* (AWAKE: two loops, gaps) */
        rnd_tx(tr, TX_MODE, TESS_ENG_AWAKE);
        rnd_tx(tr, TX_AWK_LENA, rnd_rr(3, 6));
        rnd_tx(tr, TX_AWK_LENB, rnd_rr(2, 5));
        rnd_tx(tr, TX_AWK_PROB, rnd_rr(50, 85));
    } else {                                        /* (MARKOV: sparse, near steps) */
        rnd_tx(tr, TX_MODE, TESS_ENG_MARKOV);
        rnd_tx(tr, TX_MKV_LEAP, rnd_rr(5, 35));
        rnd_tx(tr, TX_MKV_DENS, rnd_rr(40, 70));
        rnd_tx(tr, TX_MKV_SPAN, 1);
    }
    rnd_tp(tr, P_SDIV, rnd_pick(dv, dw, NELEM(dv)));
    rnd_tx(tr, TX_TW_DROP, sparse ? rnd_rr(45, 70) : trk[tr].p[P_SDIV] == 2 ? rnd_rr(35, 60) : rnd_rr(5, 35));
    rnd_tx(tr, TX_EV_EVERY, rnd_pct(70) ? rnd_rr(3, 5) : 0);   /* (now and then the line turns: 4 to 16 bars) */
    rnd_tx(tr, TX_EV_AMOUNT, rnd_rr(10, 30));
    rnd_tx(tr, TX_TW_HUMAN, rnd_rr(30, 70));
    rnd_tx(tr, TX_SUB_HUMAN, rnd_rr(25, (int32_t)TESS_HUMAN_MS));
    rnd_tx(tr, TX_NL_MODE, 0);
    rnd_tp(tr, P_SGATE, rnd_rr(40, 90));
    if (rnd_pct(40)) rnd_tp(tr, P_SLEN, rnd_rr(5, 14));   /* (a loop its own length) */
    if (trk[tr].p[P_SDIV] == 0 && rnd_pct(60)) {     /* (on the beat: a note now and then repeated, two or three
                                                     * quick strikes: the references' repeats) */
        rnd_tx(tr, TX_SUB_RATCH, rnd_rr(1, 2));
        rnd_tx(tr, TX_SUB_CHANCE, rnd_rr(20, 50));
        rnd_tx(tr, TX_SUB_SHAPE, 0);
    }
}
static void rnd_wozza_generator(uint32_t tr, uint32_t role, uint32_t *chords)
{
    rnd_generator(tr, chords);                      /* (EVOLVE, TWEAK, TIMING and the rest as RND's, then:) */
    if (tessera.mode[tr] == TESS_ENG_CHORD && *chords) (*chords)--;
    rnd_tx(tr, TX_FM_FORM, 0);
    rnd_tx(tr, TX_TW_BRANCH, 0); rnd_tx(tr, TX_TW_BR_TO, 0); rnd_tx(tr, TX_TW_LOGIC, 0); rnd_tx(tr, TX_TW_COND, 0);
    rnd_tx(tr, TX_TW_SHIFT, 0);
    rnd_tx(tr, TX_SUB_RATCH, 0);
    rnd_tp(tr, P_SSWING, 0);
    rnd_tp(tr, P_SLEN, 16);
    if (role == RND_ROLE_DRONE || role == RND_ROLE_NOISE) {   /* the bed: two to four notes, a bar to four each,
                                                     * held (SERIES: never a rest, as TURING's 0 bits are); the
                                                     * texture: one note, held */
        static const uint8_t dv[] = {7, 8, 9}, dw[] = {2, 4, 4};   /* 1/1 2BAR 4BAR */
        rnd_tx(tr, TX_MODE, TESS_ENG_SERIES);
        rnd_tx(tr, TX_SR_START, rnd_rr(0, 127));
        rnd_tx(tr, TX_SR_INTV, rnd_rr(1, 3));
        rnd_tx(tr, TX_SR_LOOP, role == RND_ROLE_NOISE ? 1 : rnd_rr(2, 4));
        rnd_tp(tr, P_SDIV, role == RND_ROLE_NOISE ? 9 : rnd_pick(dv, dw, NELEM(dv)));
        rnd_tp(tr, P_SGATE, 127);
        rnd_tx(tr, TX_TW_DROP, 0);
        rnd_tx(tr, TX_EV_EVERY, role == RND_ROLE_DRONE && rnd_pct(50) ? rnd_rr(4, 5) : 0);   /* (8 or 16 bars, or
                                                     * never) */
        rnd_tx(tr, TX_EV_AMOUNT, rnd_rr(10, 30));
        rnd_tx(tr, TX_TW_HUMAN, rnd_rr(0, 20));
        rnd_tx(tr, TX_SUB_HUMAN, rnd_rr(0, 40));
        rnd_tx(tr, TX_NL_MODE, 0);
        return;
    }
    if (role == RND_ROLE_OSTI) {                    /* two notes taking turns, a beat or two each */
        static const uint8_t dv[] = {0, 6, 1}, dw[] = {55, 30, 15};   /* 1/4 1/2 1/8 */
        rnd_tx(tr, TX_MODE, TESS_ENG_SERIES);
        rnd_tx(tr, TX_SR_START, rnd_rr(0, 127));
        rnd_tx(tr, TX_SR_INTV, rnd_rr(2, 4));       /* (a third, a fourth, a fifth apart in the scale) */
        rnd_tx(tr, TX_SR_LOOP, 2);
        rnd_tp(tr, P_SDIV, rnd_pick(dv, dw, NELEM(dv)));
        rnd_tp(tr, P_SGATE, rnd_rr(60, 110));
        rnd_tx(tr, TX_TW_DROP, rnd_pct(50) ? rnd_rr(5, 25) : 0);
        rnd_tx(tr, TX_EV_EVERY, 0);
        rnd_tx(tr, TX_TW_HUMAN, rnd_rr(20, 50));
        rnd_tx(tr, TX_SUB_HUMAN, rnd_rr(20, (int32_t)TESS_HUMAN_MS));
        rnd_tx(tr, TX_NL_MODE, 0);
        return;
    }
    rnd_wozza_line(tr, rnd_role_of[tr] == RND_ROLE_MELODY && tr != rnd_wz_lead);
}
/* the registers: the bed low, in the middle now and then; the ostinato apart from it; the melody above; a second
 * melody higher; the texture where it is. And the tempo: slow */
static void rnd_wozza_registers(const uint8_t *role)
{
    int32_t bed = rnd_pct(40) ? -24 : rnd_pct(75) ? -12 : 0, osti = bed == -12 ? (rnd_pct(60) ? 0 : -24) : -12;
    int32_t mel = rnd_pct(60) ? 12 : rnd_pct(75) ? 0 : 24;
    uint32_t tr;
    for (tr = 0; tr < NTRK; tr++) {
        rnd_tp(tr, P_TRANS, role[tr] == RND_ROLE_DRONE ? bed : role[tr] == RND_ROLE_OSTI ? osti :
                            role[tr] == RND_ROLE_NOISE ? 0 : tr == rnd_wz_lead ? mel : mel < 24 ? mel + 12 : 24);
        rnd_tp(tr, P_SSWING, 0);
    }
    pid_set(PID_GLOBAL(G_BPM), 0, rnd_rr(72, 84));
}
/* the key: plain (MAJ, the pentatonics; MIN, DOR, LYD now and then) */
static void rnd_key_wozza(void)
{
    static const uint8_t sc[] = {5, 1, 6, 2, 3, 9}, sw[] = {7, 5, 3, 2, 2, 1};   /* PEN MAJ MPEN MIN DOR LYD */
    int32_t scale = (int32_t)rnd_pick(sc, sw, NELEM(sc)), root = rnd_rr(0, 11);
    uint32_t tr;
    rnd_tx(0, TX_KEY_LINK, 1);
    for (tr = 0; tr < NTRK; tr++) {
        rnd_tp(tr, P_SCALE, scale);
        rnd_tp(tr, P_ROOT, root);
        rnd_tp(tr, P_QUANT, TP[P_QUANT].def);
    }
    scale_share(&trk[0]);
}
/* the four: a bed, an ostinato, a melody, and the texture (a second melody in a quarter of rolls), in a random order */
static void rnd_roles_wozza(uint8_t *role)
{
    uint32_t k;
    role[0] = RND_ROLE_DRONE; role[1] = RND_ROLE_OSTI; role[2] = RND_ROLE_MELODY;
    role[3] = rnd_pct(75) ? RND_ROLE_NOISE : RND_ROLE_MELODY;
    for (k = NTRK - 1u; k > 0u; k--) {
        uint32_t j = rnd_next() % (k + 1u);
        uint8_t v = role[k]; role[k] = role[j]; role[j] = v;
    }
    for (k = 0, rnd_wz_lead = NTRK; k < NTRK; k++)
        if (role[k] == RND_ROLE_MELODY && rnd_wz_lead == NTRK) rnd_wz_lead = (uint8_t)k;
}

/* ------------------------------------------------------------ HALO --- */
/* HALO: after Arvo Pärt's tintinnabuli (Spiegel im Spiegel, Für Alina; the canons of Cantus): warm, hazy, slow (60 to
 * 72 BPM). The HALO itself (RND_ROLE_CHORDS): a pad holding the tonic triad (CHORD: DRONE, now and then TWO), breathing
 * slowly. The M-VOICE (RND_ROLE_MELODY): a slow line, mostly by step (SERIES, its steps one degree; FALL's leaves; a
 * loose TURING), on FALL's three triangles, warmer and hazier. The T-VOICE (RND_ROLE_TINT): bells
 * that follow the M-voice (FOLLOW) onto the nearest note of the triad (QNT CHRD): Pärt's tintinnabuli voice, an octave
 * above it or below (beside it, a high line's). The CANON (RND_ROLE_CANON): the M-voice again, half as fast, an octave
 * down (a high line's mostly in its own octave) (FOLLOW on a slower clock: a mensuration canon's slow voice). A
 * follower's TRANS is from the line's own note (FOLLOW takes it transposed), on the triangles or FM6's soft pads. Every voice detuned and spread; the line's and the halo's pitch drifting a hair
 * (the LFO slow on the pitch: haze). Dry, as every roll */
static uint8_t rnd_hl_m = NTRK;                     /* HALO: the M-voice's track (the others follow it) */
static void rnd_halo_tri(uint32_t tr)               /* FALL's three triangles, warmer, hazier */
{
    static const uint8_t i2[] = {24, 36, 31, 12, 43}, i2w[] = {3, 3, 2, 1, 1};   /* (+24: 0 +12 +7 -12 +19) */
    static const uint8_t i3[] = {12, 24, 36, 0, 31}, i3w[] = {3, 2, 2, 1, 1};    /* (-12 0 +12 -24 +7) */
    rnd_tp(tr, P_E0, 4);                            /* WAVE TRI3 */
    rnd_tp(tr, P_E0 + 1u, (int32_t)rnd_pick(i2, i2w, NELEM(i2)) - 24);   /* INT2 */
    rnd_tp(tr, P_E0 + 2u, (int32_t)rnd_pick(i3, i3w, NELEM(i3)) - 24);   /* INT3 */
    rnd_tp(tr, P_E0 + 3u, rnd_rr(6, 22));           /* DTN, cents: the haze */
    rnd_tp(tr, P_E0 + 4u, 0);                       /* MODE LP */
    rnd_tp(tr, P_E0 + 5u, rnd_rr(48, 80));          /* CUT: warm */
    rnd_tp(tr, P_E0 + 6u, rnd_rr(0, 20));           /* RES */
}
static void rnd_halo_sound(uint32_t tr, uint32_t role, uint32_t *fm6s, uint32_t *phys, uint32_t *oracles)
{
    uint8_t nice = rnd_nice;
    int ph = !*phys && eng_ok(ENGI_PHYS), fm = !*fm6s && eng_ok(ENGI_FM6);
    uint32_t e;
    if (role == RND_ROLE_CHORDS) {                  /* the halo: a warm pad */
        static const uint8_t tp[] = {5, 7, 8, 6}, tw[] = {4, 3, 3, 2};   /* WARM HOLLOW SOFT PAD, STRING MACH */
        rnd_want_e = 6u, rnd_want_pi = rnd_pick(tp, tw, NELEM(tp));
    } else if (role == RND_ROLE_MELODY) {           /* the M-voice: FALL's triangles (a bowed string in the middle
                                                     * was a hiss, measured) */
        rnd_want_e = 6u, rnd_want_pi = 8;
    } else if (role == RND_ROLE_TINT) {             /* the T-voice: bells */
        static const uint8_t fp[] = {30, 31, 1, 0, 33}, fw[] = {3, 3, 2, 2, 1};   /* ZEN BELL, RIN, BELL, TINE EP, FURIN */
        static const uint8_t pp[] = {9}, pw[] = {1};                              /* VIBES (BELL TREE: a glare; a bowl
                                                     * unbowed: silent) */
        if (fm && (rnd_pct(55) || !ph)) rnd_want_e = ENGI_FM6, rnd_want_pi = rnd_pick(fp, fw, NELEM(fp));
        else if (ph) rnd_want_e = ENGI_PHYS, rnd_want_pi = rnd_pick(pp, pw, NELEM(pp));
        else rnd_want_e = 6u, rnd_want_pi = 3;      /* (TRIO's RING BELL) */
    } else {                                        /* the canon: triangles or a soft pad, low (PHYS bowed: a
                                                     * hiss as often as not, measured) */
        if (fm && rnd_pct(35)) rnd_want_e = ENGI_FM6, rnd_want_pi = rnd_pct(50) ? 25 : 26;   /* SOFT PAD, SOFT SWELL */
        else rnd_want_e = 6u, rnd_want_pi = 8;
    }
    rnd_nice = 1;                                   /* (the presets' kinds kept, their knobs pushed a little) */
    rnd_sound(tr, fm6s, phys, oracles, RND_ROLE_FREE);
    rnd_nice = nice;
    rnd_want_e = rnd_want_pi = 0xFFu;
    e = trk[tr].eng_req;
    rnd_tp(tr, P_ED_PIT, 0);
    rnd_tp(tr, P_ED_SHP, 0);
    rnd_tp(tr, P_ED_FLT, rnd_rr(0, 15));
    rnd_tp(tr, P_LD_FLT, 0);
    rnd_tp(tr, P_LD_SHP, 0);
    rnd_tp(tr, P_LD_AMP, 0);
    rnd_tp(tr, P_LWAVE, 0);
    rnd_tp(tr, P_LSYNC, 0);
    rnd_tp(tr, P_LTRIG, 0);
    rnd_tp(tr, P_LFADE, 0);
    rnd_tp(tr, P_LRATE, rnd_rr(40, 56));            /* (0.4 to 1 Hz: a slow sway) */
    rnd_tp(tr, P_LD_PIT, (role == RND_ROLE_MELODY || role == RND_ROLE_CHORDS) && rnd_pct(50) ? 1 : 0);
    rnd_tp(tr, P_GLIDE, 0);
    rnd_tp(tr, P_CHRD, 0);
    rnd_tp(tr, P_DETUNE, rnd_rr(40, 100));
    rnd_tp(tr, P_SPRD, rnd_rr(60, 127));
    if (e == 6u && role != RND_ROLE_TINT && role != RND_ROLE_CHORDS) rnd_halo_tri(tr);
    if (role == RND_ROLE_CHORDS) {                  /* the halo: swells in, holds, breathes */
        if (e == 6u) { rnd_tp(tr, P_E0 + 3u, rnd_rr(10, 30)); rnd_tp(tr, P_E0 + 5u, rnd_rr(40, 65)); }   /* (DTN, CUT) */
        rnd_tp(tr, P_ATK, rnd_rr(90, 120));
        rnd_tp(tr, P_DEC, 127);
        rnd_tp(tr, P_SUS, rnd_rr(110, 127));
        rnd_tp(tr, P_REL, rnd_rr(105, 125));
        rnd_tp(tr, P_VOICE, V_POLY);
        rnd_tp(tr, P_LEVEL, rnd_rr(58, 72));
        rnd_tp(tr, P_PAN, 0);
        if (!trk[tr].p[P_LD_PIT]) {                 /* (no sway on the pitch: a breath on the filter) */
            rnd_tp(tr, P_LRATE, rnd_rr(20, 40));
            rnd_tp(tr, P_LD_FLT, rnd_rr(8, 20));
        }
    } else if (role == RND_ROLE_MELODY) {
        rnd_tp(tr, P_ATK, rnd_rr(15, 50));
        rnd_tp(tr, P_DEC, rnd_rr(90, 120));
        rnd_tp(tr, P_SUS, rnd_rr(60, 110));
        rnd_tp(tr, P_REL, rnd_rr(80, 110));
        rnd_tp(tr, P_VOICE, rnd_pct(50) ? V_LEGATO : V_MONO);
        rnd_tp(tr, P_LEVEL, rnd_rr(76, 90));
        rnd_tp(tr, P_PAN, rnd_rr(-20, 20));
    } else if (role == RND_ROLE_TINT) {             /* struck, ringing, a step aside in the stereo */
        rnd_tp(tr, P_ATK, rnd_rr(0, 8));
        rnd_tp(tr, P_DEC, rnd_rr(75, 105));
        rnd_tp(tr, P_SUS, rnd_rr(0, 20));
        rnd_tp(tr, P_REL, rnd_rr(65, 95));          /* (ringing, but the pool kept: a bell a step) */
        rnd_tp(tr, P_VOICE, V_POLY);
        rnd_tp(tr, P_LEVEL, rnd_rr(62, 76));
        rnd_tp(tr, P_PAN, rnd_pct(50) ? rnd_rr(20, 45) : -rnd_rr(20, 45));
        if (e == ENGI_FM6) { rnd_tp(tr, P_E0 + 2u, rnd_rr(-20, 0)); rnd_tp(tr, P_E0 + 7u, rnd_rr(0, 14)); }   /* (MLVL, AEG) */
        if (e == ENGI_PHYS) {                       /* (struck, never bowed; quieter than FM6's bells: up) */
            rnd_tp(tr, P_E0 + 2u, rnd_rr(50, 80));
            rnd_tp(tr, P_E0 + 6u, 0);
            rnd_tp(tr, P_LEVEL, rnd_rr(88, 100));
        }
    } else {                                        /* the canon: slow, low, held */
        rnd_tp(tr, P_ATK, rnd_rr(50, 100));
        rnd_tp(tr, P_DEC, rnd_rr(110, 127));
        rnd_tp(tr, P_SUS, rnd_rr(100, 127));
        rnd_tp(tr, P_REL, rnd_rr(100, 120));
        rnd_tp(tr, P_VOICE, V_LEGATO);
        rnd_tp(tr, P_LEVEL, rnd_rr(70, 84));
        rnd_tp(tr, P_PAN, rnd_rr(-15, 15));
        if (e == ENGI_FM6) rnd_tp(tr, P_E0 + 7u, rnd_rr(10, 24));
    }
}
/* HALO's generators: the halo's triad, the M-voice's line, then the two that follow it (the T-voice on the M-voice's
 * clock, the canon on one twice as slow) */
static void rnd_halo_generators(const uint8_t *role)
{
    static const uint8_t dv[] = {0, 6, 1}, dw[] = {55, 30, 15};   /* 1/4 1/2 1/8 */
    uint32_t tr, chords = 1, mdiv = 0, k;
    for (tr = 0, rnd_hl_m = NTRK; tr < NTRK; tr++) if (role[tr] == RND_ROLE_MELODY) rnd_hl_m = (uint8_t)tr;
    for (tr = 0; tr < TESSERA_TRACKS; tr++) {
        if (role[tr] == RND_ROLE_CHORDS) continue;
        rnd_generator(tr, &chords);                 /* (EVOLVE, TWEAK, TIMING and the rest as RND's; never CHORD) */
        rnd_tx(tr, TX_FM_FORM, 0);
        rnd_tx(tr, TX_TW_BRANCH, 0); rnd_tx(tr, TX_TW_BR_TO, 0); rnd_tx(tr, TX_TW_LOGIC, 0); rnd_tx(tr, TX_TW_COND, 0);
        rnd_tx(tr, TX_TW_SHIFT, 0);
        rnd_tx(tr, TX_TW_FOLLOW, 0);
        rnd_tx(tr, TX_SUB_RATCH, 0);
        rnd_tx(tr, TX_NL_MODE, 0);
        rnd_tp(tr, P_SSWING, 0);
        rnd_tp(tr, P_SLEN, 16);
    }
    for (tr = 0; tr < TESSERA_TRACKS; tr++) {
        if (role[tr] != RND_ROLE_CHORDS) continue;  /* the halo: the tonic triad held, four bars a chord */
        rnd_tx(tr, TX_MODE, TESS_ENG_CHORD);
        rnd_tx(tr, TX_CH_PROG, rnd_pct(70) ? 5 : 4);   /* DRONE (I alone), TWO (I IV) */
        rnd_tx(tr, TX_CH_BARS, 3);
        rnd_tx(tr, TX_CH_HIT, 0);
        rnd_tx(tr, TX_CH_OCT, 0);
        rnd_tx(tr, TX_CH_BREATH, rnd_rr(0, 40));
        rnd_tx(tr, TX_TW_DROP, 0);
        rnd_tx(tr, TX_EV_EVERY, 0);
        rnd_tx(tr, TX_SUB_HUMAN, 0);
        rnd_tp(tr, P_SDIV, 0);
        rnd_tp(tr, P_SGATE, 127);
    }
    if (rnd_hl_m < NTRK) {                          /* the M-voice: slow, mostly by step */
        tr = rnd_hl_m;
        k = (uint32_t)rnd_rr(0, 99);
        if (k < 65u) {
            rnd_tx(tr, TX_MODE, TESS_ENG_SERIES);
            rnd_tx(tr, TX_SR_START, rnd_rr(0, 127));
            rnd_tx(tr, TX_SR_INTV, 1);
            rnd_tx(tr, TX_SR_LOOP, rnd_pct(50) ? 0 : rnd_rr(6, 16));
        } else if (k < 85u) {                       /* (FALL's leaves) */
            rnd_tx(tr, TX_MODE, TESS_ENG_BOUNCE);
            rnd_tx(tr, TX_BNC_BALLS, 5);
            rnd_tx(tr, TX_BNC_SPEED, rnd_rr(4, 20));
            rnd_tx(tr, TX_BNC_ANGLE, rnd_rr(35, 55));
        } else {
            rnd_tx(tr, TX_MODE, TESS_ENG_TURING);
            rnd_tx(tr, TX_TUR_LEN, rnd_rr(4, 8));
            rnd_tx(tr, TX_TUR_PROB, rnd_rr(5, 20));
            rnd_tx(tr, TX_TUR_SPAN, 1);
        }
        rnd_tp(tr, P_SDIV, tessera.mode[tr] == TESS_ENG_BOUNCE ? (rnd_pct(60) ? 1 : 0) : rnd_pick(dv, dw, NELEM(dv)));   /* (the
                                                     * leaves fall sparsely: on 1/8 or 1/4, as FALL's) */
        rnd_tp(tr, P_SGATE, rnd_rr(80, 120));
        rnd_tx(tr, TX_TW_DROP, rnd_rr(0, 20));
        rnd_tx(tr, TX_EV_EVERY, rnd_pct(50) ? rnd_rr(4, 5) : 0);
        rnd_tx(tr, TX_EV_AMOUNT, rnd_rr(10, 25));
        rnd_tx(tr, TX_TW_HUMAN, rnd_rr(10, 40));
        rnd_tx(tr, TX_SUB_HUMAN, rnd_rr(10, 40));
        mdiv = (uint32_t)trk[tr].p[P_SDIV];
    }
    for (tr = 0; tr < TESSERA_TRACKS; tr++) {
        if (role[tr] != RND_ROLE_TINT && role[tr] != RND_ROLE_CANON) continue;
        rnd_tx(tr, TX_MODE, TESS_ENG_SERIES);       /* (one note a step: the pitch is the M-voice's) */
        rnd_tx(tr, TX_SR_START, 0);
        rnd_tx(tr, TX_SR_INTV, 1);
        rnd_tx(tr, TX_SR_LOOP, 1);
        rnd_tx(tr, TX_TW_FOLLOW, rnd_hl_m < NTRK ? (int32_t)rnd_hl_m + 1 : 0);
        rnd_tx(tr, TX_EV_EVERY, 0);
        if (role[tr] == RND_ROLE_TINT) {            /* on the M-voice's clock, onto the triad */
            rnd_tp(tr, P_SDIV, (int32_t)mdiv);
            if (mdiv == 1u) rnd_tp(tr, P_VOICE, V_MONO);   /* (on 1/8: one bell at a time) */
            rnd_tp(tr, P_SGATE, rnd_rr(50, 100));
            rnd_tp(tr, P_QUANT, Q_CHRD);
            rnd_tx(tr, TX_TW_DROP, rnd_rr(0, 30));
            rnd_tx(tr, TX_TW_HUMAN, rnd_rr(20, 50));
            rnd_tx(tr, TX_SUB_HUMAN, rnd_rr(10, 40));
        } else {                                    /* twice as slow, held */
            rnd_tp(tr, P_SDIV, mdiv == 1u ? 0 : mdiv == 0u ? 6 : 7);   /* 1/8 -> 1/4 -> 1/2 -> 1/1 */
            rnd_tp(tr, P_SGATE, 127);
            rnd_tx(tr, TX_TW_DROP, 0);
            rnd_tx(tr, TX_TW_HUMAN, rnd_rr(0, 20));
            rnd_tx(tr, TX_SUB_HUMAN, rnd_rr(0, 30));
        }
    }
}
/* the registers: the halo in the middle or below, the M-voice above it, the bells an octave over the line (or under),
 * the canon an octave under (two, from a high line); and the tempo: slow */
static void rnd_halo_registers(const uint8_t *role)
{
    int32_t halo = rnd_pct(60) ? 0 : -12, m = rnd_pct(60) ? 0 : 12, t = m ? 0 : rnd_pct(65) ? 12 : -12;   /* (the bells
                                                     * never past +12: higher, a glare) */
    int32_t c = m && rnd_pct(30) ? m - 24 : m - 12;   /* (never below -12: lower, a rumble) */
    uint32_t tr;
    for (tr = 0; tr < NTRK; tr++)
        rnd_tp(tr, P_TRANS, role[tr] == RND_ROLE_CHORDS ? halo : role[tr] == RND_ROLE_MELODY ? m :
                            role[tr] == RND_ROLE_TINT ? t : c);
    pid_set(PID_GLOBAL(G_BPM), 0, rnd_rr(60, 72));
}
/* the key: MIN and MAJ mostly (Pärt's), DOR LYD MIX now and then */
static void rnd_key_halo(void)
{
    static const uint8_t sc[] = {2, 1, 3, 9, 4}, sw[] = {7, 5, 2, 2, 2};   /* MIN MAJ DOR LYD MIX */
    int32_t scale = (int32_t)rnd_pick(sc, sw, NELEM(sc)), root = rnd_rr(0, 11);
    uint32_t tr;
    rnd_tx(0, TX_KEY_LINK, 1);
    for (tr = 0; tr < NTRK; tr++) {
        rnd_tp(tr, P_SCALE, scale);
        rnd_tp(tr, P_ROOT, root);
        rnd_tp(tr, P_QUANT, TP[P_QUANT].def);
    }
    scale_share(&trk[0]);
}
/* HALO's cords: slow wanderers only: the halo's colour, the line's colour, the bells' place */
static void rnd_cords_halo(const uint8_t *role)
{
    uint32_t i = 0, tr;
    cord_t c;
    for (tr = 0; tr < NTRK && i < CORD_LIVE; tr++) {
        memset(&c, 0, sizeof c);
        c.src_trk = c.dst_trk = (uint8_t)tr;
        c.src = role[tr] == RND_ROLE_CHORDS ? CS_DRIFT : role[tr] == RND_ROLE_TINT ? CS_TORPOR : CS_INERTIA;
        c.dst = role[tr] == RND_ROLE_TINT ? CD_PAN : CD_CUTOFF;
        c.amount = (int8_t)(rnd_pct(50) ? rnd_rr(10, 25) : -rnd_rr(10, 25));
        if (cord_set(i, &c)) i++;
    }
    for (memset(&c, 0, sizeof c); i < CORD_LIVE; i++) cord_set(i, &c);
}
/* the four: the halo, the M-voice, the T-voice, the canon, in a random order (a T-voice before its M-voice plays the
 * line a step behind: an echo) */
static void rnd_roles_halo(uint8_t *role)
{
    uint32_t k;
    role[0] = RND_ROLE_CHORDS; role[1] = RND_ROLE_MELODY; role[2] = RND_ROLE_TINT; role[3] = RND_ROLE_CANON;
    for (k = NTRK - 1u; k > 0u; k--) {
        uint32_t j = rnd_next() % (k + 1u);
        uint8_t v = role[k]; role[k] = role[j]; role[j] = v;
    }
}

/* ------------------------------------------------------------ MIRROR --- */
/* MIRROR: HALO's mirror (Pärt's Spiegel im Spiegel: the mirror in the mirror): its four parts, glassy and digital,
 * the rules loosened. 70 to 100 BPM; a key in twelve notes (LYD WHOLE PEN MAJ DOR DIMHW) or, two rolls in five, off
 * them (SLENDRO, PELOG, 19 and 22 EDO, HARM16, JI-MAJ, Bohlen-Pierce). The HALO (RND_ROLE_CHORDS): glass, held or
 * pulsing: in twelve notes a CHORD track (DRONE, TWO, CIRCLE, WALK, 50S; HOLD, BEAT or OFFBEAT), off them a cluster
 * (SERIES on two or three notes, held). The LINE (RND_ROLE_MELODY): a digital line (SID's keys and plucks, FM6's
 * mallets, TRIO's sharper waves), by step or not (SERIES, TURING, BOUNCE, MARKOV, AWAKE), 1/16 to 1/4. The MIRROR
 * (RND_ROLE_TINT): glass bells following the line (FOLLOW) onto the halo's chord (QNT CHRD: the tintinnabuli voice) or
 * in parallel, a few degrees off (SHIFT: off twelve notes always), on the line's clock or twice as fast, now and then
 * stuttering (ratchets). The CANON (RND_ROLE_CANON): the line again, low, half as fast or across it in triplets. Bright
 * and wide, no wow; S&H and LFO cords on the shapes and the bells' place. DRIFT moves it as HALO's (rnd_mirror_move) */
static int rnd_mr_micro(void)                       /* (off twelve notes: no chords to snap to) */
{
    return trk[0].p[P_SCALE] >= 16;
}
static void rnd_mirror_sound(uint32_t tr, uint32_t role, uint32_t *fm6s, uint32_t *phys, uint32_t *oracles)
{
    uint8_t nice = rnd_nice;
    int ph = !*phys && eng_ok(ENGI_PHYS), fm = !*fm6s && eng_ok(ENGI_FM6), orc = !*oracles && eng_ok(ENGI_ORACLE);
    int sid = eng_ok(3u);
    uint32_t e, k = (uint32_t)rnd_rr(0, 99);
    if (role == RND_ROLE_CHORDS) {                  /* glass */
        static const uint8_t sp[] = {16, 19, 18, 17}, sw[] = {3, 3, 2, 2};   /* PWM PAD, 8580 PAD, DREAM CHIP, SID STRINGS */
        if (sid && k < 65u) rnd_want_e = 3u, rnd_want_pi = rnd_pick(sp, sw, NELEM(sp));   /* (FM6's and ORACLE's pads:
                                                     * a chord of them near HALO's whole cost again, measured) */
        else rnd_want_e = 6u, rnd_want_pi = 8;      /* SOFT PAD, its wave made digital below (CHIP CHOIR: a hiss;
                                                     * SWEEP PAD: all but silent, measured) */
    } else if (role == RND_ROLE_MELODY) {           /* digital */
        static const uint8_t sp[] = {22, 24, 25, 26, 20, 15}, sw[] = {3, 3, 2, 2, 2, 1};   /* C64 KEYS, ARP PLUCK, COIN
                                                     * PLUCK, BELL RING, TRI ORGAN, 8580 LEAD */
        static const uint8_t fp[] = {8, 29, 21, 12}, fw[] = {3, 3, 2, 2};   /* DX TINE, MALLET, STEEL DRUM, DX MARIMBA */
        if (sid && k < 50u) rnd_want_e = 3u, rnd_want_pi = rnd_pick(sp, sw, NELEM(sp));
        else if (fm && k < 75u) rnd_want_e = ENGI_FM6, rnd_want_pi = rnd_pick(fp, fw, NELEM(fp));
        else rnd_want_e = 6u, rnd_want_pi = rnd_pct(50) ? 1 : 3;   /* ARP LEAD, RING BELL */
    } else if (role == RND_ROLE_TINT) {             /* glass bells */
        static const uint8_t fp[] = {11, 23, 33, 31, 1}, fw[] = {3, 2, 2, 2, 2};   /* BELLS, TUBULAR, FURIN, RIN, BELL */
        if (fm && k < 45u) rnd_want_e = ENGI_FM6, rnd_want_pi = rnd_pick(fp, fw, NELEM(fp));
        else if (ph && k < 70u) rnd_want_e = ENGI_PHYS, rnd_want_pi = 9;   /* VIBES (BELL TREE: a hiss, measured) */
        else if (orc && k < 82u) rnd_want_e = ENGI_ORACLE, rnd_want_pi = 10;   /* POLY BELLS */
        else if (sid) rnd_want_e = 3u, rnd_want_pi = 26;   /* BELL RING */
        else rnd_want_e = 6u, rnd_want_pi = 3;      /* RING BELL (no SID in the build) */
    } else {                                        /* the canon: digital, low */
        static const uint8_t sp[] = {20, 19, 4}, sw[] = {3, 2, 2};   /* TRI ORGAN, 8580 PAD, SUB PULSE */
        if (sid && k < 40u) rnd_want_e = 3u, rnd_want_pi = rnd_pick(sp, sw, NELEM(sp));
        else if (orc && k < 65u) rnd_want_e = ENGI_ORACLE, rnd_want_pi = rnd_pct(50) ? 14 : 19;   /* DRONE, SWEEP PAD */
        else rnd_want_e = 6u, rnd_want_pi = 8;      /* SOFT PAD, digital below (FM6's DEEP PAD, TRIO's FIFTH DRONE
                                                     * and SWEEP PAD: all but silent on the canon's notes, measured) */
    }
    rnd_nice = 1;                                   /* (the presets' kinds kept, their knobs pushed a little: pushed
                                                     * further, SID's, FM6's and TRIO's turned to a hiss, measured) */
    rnd_sound(tr, fm6s, phys, oracles, RND_ROLE_FREE);
    rnd_nice = nice;
    rnd_want_e = rnd_want_pi = 0xFFu;
    e = trk[tr].eng_req;
    rnd_tp(tr, P_ED_PIT, 0);
    rnd_tp(tr, P_ED_SHP, rnd_pct(30) ? rnd_rr(-30, 30) : 0);
    rnd_tp(tr, P_LD_PIT, 0);                        /* (no wow: glass) */
    rnd_tp(tr, P_LD_AMP, 0);
    rnd_tp(tr, P_LD_FLT, rnd_pct(40) ? rnd_rr(5, 25) : 0);
    rnd_tp(tr, P_LD_SHP, rnd_pct(50) ? rnd_rr(10, 40) : 0);   /* (the shape shimmering: PW, the wave) */
    rnd_tp(tr, P_LWAVE, rnd_pct(70) ? 0 : 3);       /* (a sine; or S&H: digital steps) */
    rnd_tp(tr, P_LSYNC, 0);
    rnd_tp(tr, P_LTRIG, 0);
    rnd_tp(tr, P_LFADE, 0);
    rnd_tp(tr, P_LRATE, rnd_rr(55, 90));
    rnd_tp(tr, P_GLIDE, role == RND_ROLE_MELODY && rnd_pct(20) ? rnd_rr(5, 30) : 0);
    rnd_tp(tr, P_CHRD, 0);
    rnd_tp(tr, P_DETUNE, rnd_rr(0, 30));
    rnd_tp(tr, P_SPRD, rnd_rr(80, 127));
    if (e == 6u && role != RND_ROLE_TINT) {         /* (TRIO: SST, PPT, PLS3, the sharper waves; the filter not wide
                                                     * open) */
        static const uint8_t wv[] = {3, 2, 1}, ww[] = {1, 1, 1};
        rnd_tp(tr, P_E0, rnd_pick(wv, ww, NELEM(wv)));
        rnd_tp(tr, P_E0 + 5u, rnd_rr(60, 85));
        rnd_tp(tr, P_E0 + 6u, rnd_rr(0, 40));
    }
    if (role == RND_ROLE_CHORDS) {
        rnd_tp(tr, P_ATK, rnd_rr(40, 80));          /* (slower: a chord gone before it swells, measured) */
        rnd_tp(tr, P_DEC, rnd_rr(100, 127));
        rnd_tp(tr, P_SUS, rnd_rr(100, 127));
        rnd_tp(tr, P_REL, rnd_rr(90, 120));
        rnd_tp(tr, P_VOICE, V_POLY);
        rnd_tp(tr, P_LEVEL, rnd_rr(56, 70));
        rnd_tp(tr, P_PAN, 0);
    } else if (role == RND_ROLE_MELODY) {
        static const uint8_t vv[] = {V_MONO, V_LEGATO, V_POLY}, vw[] = {4, 2, 3};
        rnd_tp(tr, P_ATK, rnd_rr(0, 10));
        rnd_tp(tr, P_DEC, rnd_rr(60, 100));
        rnd_tp(tr, P_SUS, rnd_rr(20, 80));
        rnd_tp(tr, P_REL, rnd_rr(50, 90));
        rnd_tp(tr, P_VOICE, rnd_pick(vv, vw, NELEM(vv)));
        rnd_tp(tr, P_LEVEL, rnd_rr(76, 90));
        rnd_tp(tr, P_PAN, rnd_rr(-25, 25));
        if (e == ENGI_FM6) rnd_tp(tr, P_E0 + 2u, rnd_rr(-25, 0));   /* (MLVL: the tines' clang kept in) */
    } else if (role == RND_ROLE_TINT) {
        rnd_tp(tr, P_ATK, rnd_rr(0, 3));
        rnd_tp(tr, P_DEC, rnd_rr(70, 110));
        rnd_tp(tr, P_SUS, rnd_rr(0, 20));
        rnd_tp(tr, P_REL, rnd_rr(60, 95));
        rnd_tp(tr, P_VOICE, V_POLY);
        rnd_tp(tr, P_LEVEL, rnd_rr(64, 80));
        rnd_tp(tr, P_PAN, rnd_pct(50) ? rnd_rr(30, 60) : -rnd_rr(30, 60));
        if (e == ENGI_FM6) rnd_tp(tr, P_E0 + 7u, rnd_rr(0, 14));
        if (e == ENGI_PHYS) { rnd_tp(tr, P_E0 + 6u, 0); rnd_tp(tr, P_LEVEL, rnd_rr(84, 98)); }
    } else {
        rnd_tp(tr, P_ATK, rnd_rr(30, 90));
        rnd_tp(tr, P_DEC, rnd_rr(100, 127));
        rnd_tp(tr, P_SUS, rnd_rr(90, 127));
        rnd_tp(tr, P_REL, rnd_rr(80, 115));
        rnd_tp(tr, P_VOICE, V_LEGATO);
        rnd_tp(tr, P_LEVEL, rnd_rr(68, 82));
        rnd_tp(tr, P_PAN, rnd_rr(-15, 15));
        if (e == ENGI_FM6) rnd_tp(tr, P_E0 + 7u, rnd_rr(10, 24));
    }
    if (e == 3u && trk[tr].p[P_E0 + 1u] == 3) rnd_tp(tr, P_E0 + 1u, 2);   /* (SID's NOISE wave pushed in: PULSE) */
    if (e == 3u || e == ENGI_ORACLE)                /* (SID, ORACLE: quieter than the others, measured: up) */
        rnd_tp(tr, P_LEVEL, (int32_t)trk[tr].p[P_LEVEL] + (e == 3u ? 22 : 12) > 118 ? 118 :
                            (int32_t)trk[tr].p[P_LEVEL] + (e == 3u ? 22 : 12));
}
/* the canon's division from the line's: half as fast, or across it (3:2 in triplets); "cross" 0 or 1 */
static int32_t rnd_mr_canon_div(int32_t d, int cross)
{
    switch (d) {
    case 2: return cross ? 4 : 1;                   /* 1/16: 1/8, or 8T (4:3 against it) */
    case 1: return cross ? 4 : 0;                   /* 1/8: 1/4, or 8T (3:2) */
    case 4: return cross ? 1 : 0;                   /* 8T: 1/4, or 1/8 (2:3) */
    case 0: return cross ? 4 : 6;                   /* 1/4: 1/2, or 8T (3:1 over it) */
    default: return 7;                              /* 1/2: 1/1 */
    }
}
static int rnd_mr_heavy(uint32_t tr)                /* (FM6, PHYS, ORACLE: never on a fast clock, rnd_cpu) */
{
    uint32_t e = trk[tr].eng_req;
    return e == ENGI_FM6 || e == ENGI_PHYS || e == ENGI_ORACLE;
}
/* the mirror's way of following the line: onto the halo's chord, or in parallel SHIFT degrees off it */
static void rnd_mr_tint_mode(uint32_t tr, int chord)
{
    static const uint8_t sh[] = {26, 28, 22, 21, 27, 29}, shw[] = {4, 3, 3, 2, 2, 1};   /* (+24: +2 +4 -2 -3 +3 +5) */
    if (chord && !rnd_mr_micro()) {
        rnd_tp(tr, P_QUANT, Q_CHRD);
        rnd_tx(tr, TX_TW_SHIFT, 0);
    } else {
        rnd_tp(tr, P_QUANT, TP[P_QUANT].def);
        rnd_tx(tr, TX_TW_SHIFT, (int32_t)rnd_pick(sh, shw, NELEM(sh)) - 24);
    }
}
static void rnd_mirror_generators(const uint8_t *role)
{
    static const uint8_t dv[] = {1, 0, 4, 2}, dw[] = {40, 30, 15, 15};   /* 1/8 1/4 8T 1/16 */
    uint32_t tr, chords = 1, k;
    int32_t mdiv = 0;
    for (tr = 0, rnd_hl_m = NTRK; tr < NTRK; tr++) if (role[tr] == RND_ROLE_MELODY) rnd_hl_m = (uint8_t)tr;
    for (tr = 0; tr < TESSERA_TRACKS; tr++) {       /* (the clean slate, as HALO's) */
        if (role[tr] == RND_ROLE_CHORDS && !rnd_mr_micro()) continue;
        rnd_generator(tr, &chords);
        rnd_tx(tr, TX_FM_FORM, 0);
        rnd_tx(tr, TX_TW_BRANCH, 0); rnd_tx(tr, TX_TW_BR_TO, 0); rnd_tx(tr, TX_TW_LOGIC, 0); rnd_tx(tr, TX_TW_COND, 0);
        rnd_tx(tr, TX_TW_SHIFT, 0);
        rnd_tx(tr, TX_TW_FOLLOW, 0);
        rnd_tx(tr, TX_SUB_RATCH, 0);
        rnd_tx(tr, TX_NL_MODE, 0);
        rnd_tp(tr, P_SSWING, 0);
        rnd_tp(tr, P_SLEN, 16);
    }
    for (tr = 0; tr < TESSERA_TRACKS; tr++) {
        if (role[tr] != RND_ROLE_CHORDS) continue;
        if (!rnd_mr_micro()) {                      /* the halo: chords, held or pulsing */
            static const uint8_t pg[] = {5, 4, 3, 6, 1}, pgw[] = {3, 3, 1, 1, 1};   /* DRONE TWO CIRCLE WALK 50S */
            static const uint8_t ht[] = {0, 1, 3}, htw[] = {6, 3, 1};               /* HOLD BEAT OFFBEAT */
            rnd_tx(tr, TX_MODE, TESS_ENG_CHORD);
            rnd_tx(tr, TX_CH_PROG, rnd_pick(pg, pgw, NELEM(pg)));
            rnd_tx(tr, TX_CH_BARS, rnd_rr(1, 3));
            rnd_tx(tr, TX_CH_HIT, rnd_pick(ht, htw, NELEM(ht)));
            rnd_tx(tr, TX_CH_OCT, 0);
            rnd_tx(tr, TX_CH_BREATH, rnd_rr(0, 40));
            rnd_tp(tr, P_SDIV, 0);
            if (pid_get(PID_TESS(TX_CH_HIT), tr)) { /* (pulsing: struck, short, as glass) */
                rnd_tp(tr, P_SGATE, rnd_rr(30, 80));
                rnd_tp(tr, P_ATK, rnd_rr(0, 20));
                rnd_tp(tr, P_REL, rnd_rr(50, 80));
            } else rnd_tp(tr, P_SGATE, 127);
        } else {                                    /* off twelve notes: a cluster, held, its notes taking turns */
            rnd_tx(tr, TX_MODE, TESS_ENG_SERIES);
            rnd_tx(tr, TX_SR_START, rnd_rr(0, 127));
            rnd_tx(tr, TX_SR_INTV, rnd_rr(1, 3));
            rnd_tx(tr, TX_SR_LOOP, rnd_rr(2, 3));
            rnd_tp(tr, P_SDIV, rnd_pct(50) ? 7 : 8);   /* 1/1, 2BAR */
            rnd_tp(tr, P_SGATE, 127);
        }
        rnd_tx(tr, TX_TW_DROP, 0);
        rnd_tx(tr, TX_EV_EVERY, 0);
        rnd_tx(tr, TX_SUB_HUMAN, 0);
    }
    if (rnd_hl_m < NTRK) {                          /* the line */
        tr = rnd_hl_m;
        k = (uint32_t)rnd_rr(0, 99);
        if (k < 35u) {
            rnd_tx(tr, TX_MODE, TESS_ENG_SERIES);
            rnd_tx(tr, TX_SR_START, rnd_rr(0, 127));
            rnd_tx(tr, TX_SR_INTV, rnd_rr(1, 2));
            rnd_tx(tr, TX_SR_LOOP, rnd_pct(50) ? 0 : rnd_rr(5, 16));
        } else if (k < 60u) {
            rnd_tx(tr, TX_MODE, TESS_ENG_TURING);
            rnd_tx(tr, TX_TUR_LEN, rnd_rr(5, 12));
            rnd_tx(tr, TX_TUR_PROB, rnd_rr(5, 30));
            rnd_tx(tr, TX_TUR_SPAN, rnd_rr(1, 2));
        } else if (k < 75u) {
            rnd_tx(tr, TX_MODE, TESS_ENG_BOUNCE);
            rnd_tx(tr, TX_BNC_BALLS, rnd_rr(3, 7));
            rnd_tx(tr, TX_BNC_SPEED, rnd_rr(10, 40));
            rnd_tx(tr, TX_BNC_ANGLE, rnd_rr(20, 70));
        } else if (k < 90u) {
            rnd_tx(tr, TX_MODE, TESS_ENG_MARKOV);
            rnd_tx(tr, TX_MKV_LEAP, rnd_rr(20, 70));
            rnd_tx(tr, TX_MKV_DENS, rnd_rr(50, 90));
            rnd_tx(tr, TX_MKV_SPAN, rnd_rr(1, 2));
        } else {
            rnd_tx(tr, TX_MODE, TESS_ENG_AWAKE);
            rnd_tx(tr, TX_AWK_LENA, rnd_rr(3, 8));
            rnd_tx(tr, TX_AWK_LENB, rnd_rr(3, 7));
            rnd_tx(tr, TX_AWK_PROB, rnd_rr(50, 90));
        }
        mdiv = (int32_t)rnd_pick(dv, dw, NELEM(dv));
        for (k = 0; k < NTRK; k++)                  /* (FM6 PHYS ORACLE never fast, rnd_cpu: the line, or the mirror
                                                     * on its clock, one of them: 1/8) */
            if ((k == tr || role[k] == RND_ROLE_TINT) && rnd_mr_heavy(k) && (mdiv == 2 || mdiv == 4)) mdiv = 1;
        rnd_tp(tr, P_SDIV, mdiv);
        rnd_tp(tr, P_SGATE, rnd_rr(30, 100));
        rnd_tx(tr, TX_TW_DROP, rnd_rr(0, 35));
        rnd_tx(tr, TX_EV_EVERY, rnd_pct(50) ? rnd_rr(3, 5) : 0);
        rnd_tx(tr, TX_EV_AMOUNT, rnd_rr(15, 40));
        rnd_tx(tr, TX_TW_HUMAN, rnd_rr(0, 30));
        rnd_tx(tr, TX_SUB_HUMAN, rnd_rr(0, 10));    /* (tight: digital) */
    }
    for (tr = 0; tr < TESSERA_TRACKS; tr++) {
        if (role[tr] != RND_ROLE_TINT && role[tr] != RND_ROLE_CANON) continue;
        rnd_tx(tr, TX_MODE, TESS_ENG_SERIES);       /* (one note a step: the pitch is the line's) */
        rnd_tx(tr, TX_SR_START, 0);
        rnd_tx(tr, TX_SR_INTV, 1);
        rnd_tx(tr, TX_SR_LOOP, 1);
        rnd_tx(tr, TX_TW_FOLLOW, rnd_hl_m < NTRK ? (int32_t)rnd_hl_m + 1 : 0);
        rnd_tx(tr, TX_EV_EVERY, 0);
        rnd_tx(tr, TX_SUB_HUMAN, rnd_rr(0, 10));
        if (role[tr] == RND_ROLE_TINT) {            /* on the line's clock, or twice as fast (light engines) */
            int32_t d = mdiv;
            if (rnd_pct(25) && !rnd_mr_heavy(tr) && (mdiv == 1 || mdiv == 0)) d = mdiv == 1 ? 2 : 1;
            rnd_tp(tr, P_SDIV, d);
            rnd_tp(tr, P_SGATE, rnd_rr(30, 90));
            rnd_tp(tr, P_VOICE, d == 2 || d == 4 ? V_MONO : V_POLY);
            rnd_tx(tr, TX_TW_DROP, rnd_rr(0, 40));
            rnd_tx(tr, TX_TW_HUMAN, rnd_rr(0, 30));
            if ((d == 0 || d == 6) && rnd_pct(30)) {   /* (on a slow clock: now and then a stutter) */
                rnd_tx(tr, TX_SUB_RATCH, rnd_rr(1, 2));
                rnd_tx(tr, TX_SUB_CHANCE, rnd_rr(20, 50));
                rnd_tx(tr, TX_SUB_SHAPE, 0);
            }
            rnd_mr_tint_mode(tr, rnd_pct(65));
        } else {                                    /* half as fast, or across in triplets */
            int32_t d = rnd_mr_canon_div(mdiv, rnd_pct(35));
            if (rnd_mr_heavy(tr) && (d == 2 || d == 4)) d = rnd_mr_canon_div(mdiv, 0);
            rnd_tp(tr, P_SDIV, d);
            rnd_tp(tr, P_SGATE, 127);
            rnd_tx(tr, TX_TW_DROP, 0);
            rnd_tx(tr, TX_TW_HUMAN, rnd_rr(0, 20));
        }
    }
}
/* the registers: as HALO's (a follower's TRANS from the line's note): the halo in the middle or below, the line in the
 * middle or above, the mirror an octave over it or under (beside a high line), the canon an octave under; the tempo */
static void rnd_mirror_registers(const uint8_t *role)
{
    int32_t halo = rnd_pct(50) ? 0 : -12, m = rnd_pct(50) ? 0 : 12, t = m ? (rnd_pct(50) ? 0 : -12) : rnd_pct(60) ? 12 : -12;
    uint32_t tr;
    for (tr = 0; tr < NTRK; tr++)
        rnd_tp(tr, P_TRANS, role[tr] == RND_ROLE_CHORDS ? halo : role[tr] == RND_ROLE_MELODY ? m :
                            role[tr] == RND_ROLE_TINT ? t : -12);
    pid_set(PID_GLOBAL(G_BPM), 0, rnd_rr(70, 100));
}
/* the key: twelve notes (LYD WHOLE PEN MAJ DOR DIMHW), or off them (SLENDRO PELOG 19EDO HARM16 JI-MAJ 22EDO BP-ET) */
static void rnd_key_mirror(void)
{
    static const uint8_t sc[] = {9, 13, 5, 1, 3, 14}, sw[] = {4, 2, 2, 2, 1, 1};
    static const uint8_t mc[] = {60, 61, 26, 46, 40, 29, 62}, mw[] = {3, 3, 2, 2, 2, 1, 1};
    int32_t scale = rnd_pct(60) ? (int32_t)rnd_pick(sc, sw, NELEM(sc)) : (int32_t)rnd_pick(mc, mw, NELEM(mc));
    int32_t root = rnd_rr(0, 11);
    uint32_t tr;
    rnd_tx(0, TX_KEY_LINK, 1);
    for (tr = 0; tr < NTRK; tr++) {
        rnd_tp(tr, P_SCALE, scale);
        rnd_tp(tr, P_ROOT, root);
        rnd_tp(tr, P_QUANT, TP[P_QUANT].def);
    }
    scale_share(&trk[0]);
}
/* MIRROR's cords: S&H on the line's shape (digital steps), an LFO on the bells' place, slow ones on the halo and the
 * canon's colour */
static void rnd_cords_mirror(const uint8_t *role)
{
    uint32_t i = 0, tr;
    cord_t c;
    for (tr = 0; tr < NTRK && i < CORD_LIVE; tr++) {
        memset(&c, 0, sizeof c);
        c.src_trk = c.dst_trk = (uint8_t)tr;
        if (role[tr] == RND_ROLE_MELODY) c.src = CS_SH, c.dst = CD_SHAPE;
        else if (role[tr] == RND_ROLE_TINT) c.src = CS_LFO, c.dst = CD_PAN;
        else c.src = rnd_pct(50) ? CS_DRIFT : CS_INERTIA, c.dst = CD_CUTOFF;
        c.amount = (int8_t)(rnd_pct(50) ? rnd_rr(10, 35) : -rnd_rr(10, 35));
        if (cord_set(i, &c)) i++;
    }
    for (memset(&c, 0, sizeof c); i < CORD_LIVE; i++) cord_set(i, &c);
}
static void rnd_roles_mirror(uint8_t *role)
{
    rnd_roles_halo(role);                           /* (the same four, in a random order) */
}

/* ------------------------------------------------------------ the roll: what the FM-1 can play --- */
/* BUDGET (GLO > GLOBAL, the voices' shared pool, 16 by default) during rolls: 12 leaves the FM-1 headroom (MANUAL "How
 * many voices at once"). Only ever lowered, and saved only when it changes: a BUDGET set lower stays */
#define RND_BUDGET 12
enum { RND_SLOW, RND_MID, RND_FAST };
static uint32_t rnd_speed(uint32_t div)            /* P_SDIV: 1/4 1/8 1/16 1/32 8T 16T 1/2 1/1 2BAR 4BAR */
{
    return div == 2u || div == 3u || div == 4u || div == 5u ? RND_FAST : div == 1u ? RND_MID : RND_SLOW;
}
static void rnd_cap(uint32_t tr, uint32_t id, int32_t top)
{
    if (trk[tr].p[id] > top) rnd_tp(tr, id, top);
}
/* a track's tail (how long a note rings after its gate), 0..127, wherever its engine keeps it: FM6 and ORACLE have
 * their own envelopes (ownenv) and leave the track's ENV RELEASE alone. FM6: AEG (+ slower carriers' release, -64..63);
 * ORACLE: its patch's amp release; the others: the track's RELEASE */
static int32_t rnd_tail(uint32_t tr)
{
    const track_t *t = &trk[tr];
    if (t->eng_req == ENGI_FM6) return t->p[P_E0 + 7u] + 64;
    if (t->eng_req == ENGI_ORACLE) return or_patch[tr][OR_RELEASE_AMP];
    return t->p[P_REL];
}
static void rnd_tail_cap(uint32_t tr, int32_t top)
{
    track_t *t = &trk[tr];
    if (t->eng_req == ENGI_FM6) {
        rnd_cap(tr, P_E0 + 7u, top - 64);
    } else if (t->eng_req == ENGI_ORACLE) {
        uint8_t *p = or_patch[tr];
        if (p[OR_RELEASE_AMP] > top) p[OR_RELEASE_AMP] = (uint8_t)top;
        if (p[OR_RELEASE_FILTER] > top) p[OR_RELEASE_FILTER] = (uint8_t)top;
    } else {
        rnd_cap(tr, P_REL, top);
    }
}
/* a track after its sound and generator: the faster it plays, the shorter its notes, so its voices are free again
 * before the next ones (a voice ringing in its release still holds a place in the pool) */
static void rnd_cpu(uint32_t tr)
{
    track_t *t = &trk[tr];
    uint32_t e = t->eng_req, sp;
    if (e == ENGI_FM6 && rnd_speed((uint32_t)t->p[P_SDIV]) == RND_FAST &&
        !(rnd_pulse && rnd_role_of[tr] == RND_ROLE_LOWPERC))   /* (FM6 is the heaviest: 1/8 at most (PULSE's kick on
                                                     * its 1/16 grid: one voice, four to eight hits a bar);
                                                                          * GLITCH's 1/8, a ratchet on one voice) */
        rnd_tp(tr, P_SDIV, rnd_pct(50) || rnd_perc(tr) ? 1 : 0);
    if (rnd_glitch && rnd_perc(tr) && e == ENGI_FM6 && t->p[P_SDIV] == 1 && rnd_pct(60) &&
        !pid_get(PID_TESS(TX_SUB_RATCH), tr)) {
        rnd_tx(tr, TX_SUB_RATCH, rnd_rr(2, 7));
        rnd_tx(tr, TX_SUB_CHANCE, rnd_rr(15, 60));
        rnd_tx(tr, TX_SUB_SHAPE, rnd_rr(0, 2));
        rnd_tp(tr, P_VOICE, V_MONO);
    }
    sp = rnd_speed((uint32_t)t->p[P_SDIV]);
    if (sp != RND_SLOW) {
        rnd_tail_cap(tr, sp == RND_FAST ? 55 : 85);
        rnd_cap(tr, P_SGATE, sp == RND_FAST ? 90 : 127);
        if (!((rnd_glitch || rnd_pulse) && rnd_perc(tr) && sp == RND_MID && t->p[P_VOICE] == V_MONO) &&
            !(rnd_pulse && rnd_role_of[tr] == RND_ROLE_NOISE && e == 6u && t->p[P_VOICE] == V_MONO))   /* (PULSE's
                                                     * hats: rolls on TRIO's one voice) */
            rnd_tx(tr, TX_SUB_RATCH, 0);            /* (ratchets only on a slow track; GLITCH's 1/8 on one voice: the
                                                     * hits as short as the step's eighth, one voice to steal) */
        if (t->p[P_CHRD]) rnd_tp(tr, P_CHRD, 0);    /* (a chord: three voices a note) */
        if (tessera.mode[tr] == TESS_ENG_CHORD && pid_get(PID_TESS(TX_CH_HIT), tr) == 2)
            rnd_tx(tr, TX_CH_HIT, 1);               /* (CHORD's hits: a beat's, not every step's) */
        if (sp == RND_FAST)
            rnd_tx(tr, TX_NL_MODE, 0);              /* (LENGTH: the step's, not long FIXED notes) */
        else if (pid_get(PID_TESS(TX_NL_LEN), tr) > 3)
            rnd_tx(tr, TX_NL_LEN, 3);               /* (FIXED at most a 1/4) */
    }
    if (tessera.mode[tr] == TESS_ENG_CHORD)          /* (a chord holds three or four voices: a shorter tail) */
        rnd_tail_cap(tr, 75);
    else if (sp == RND_FAST && tessera.mode[tr] != TESS_ENG_POLY && tessera.mode[tr] != TESS_ENG_QUAD && rnd_pct(50))
        rnd_tp(tr, P_VOICE, rnd_pct(50) || rnd_perc(tr) ? V_MONO : V_LEGATO);   /* (a line on one
                                                     * voice: nothing overlaps; GLITCH's hits each their attack) */
    if (e == ENGI_FM6) {
        rnd_tail_cap(tr, 90);                       /* (AEG at most +26: FM6's pads ring, but not for ever) */
        if (t->p[P_CHRD]) rnd_tp(tr, P_CHRD, 0);
    }
}
/* very long releases (past RND_REL_LONG) on two tracks at most: the rest of the pool stays free for new notes */
#define RND_REL_LONG 100
static void rnd_tails(void)
{
    uint32_t tr, n = 0;
    for (tr = 0; tr < NTRK; tr++) {
        track_t *t = &trk[tr];
        (void)t;
        if (rnd_tail(tr) <= RND_REL_LONG || ++n <= 2u) continue;
        rnd_tail_cap(tr, rnd_rr(50, RND_REL_LONG));
    }
}
static void rnd_budget(void)
{
    if (settings_budget <= RND_BUDGET) return;
    settings_budget = RND_BUDGET;
    settings_save();
}

/* one key and scale for all four: modes mostly, often something stranger (EDOs, JI, gamelan, maqam) */
static void rnd_key(void)
{
    static const uint8_t modal[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
    const param_desc_t *d = pid_desc(PID_TRACK(P_SCALE), 0);
    int32_t top = d ? d->max : 14, scale, root = rnd_rr(0, 11);
    uint32_t tr;
    scale = rnd_pct(55) ? modal[rnd_next() % NELEM(modal)] : rnd_rr(16, top);
    rnd_tx(0, TX_KEY_LINK, 1);
    for (tr = 0; tr < NTRK; tr++) {
        rnd_tp(tr, P_SCALE, scale);
        rnd_tp(tr, P_ROOT, root);
    }
    scale_share(&trk[0]);
}

/* each track an octave of its own (TRN), dealt at random whatever its role: four of -24 -12 0 +12 +24, the one left
 * out most often +24 (a high drone or pad shrill), shuffled over the tracks; now and then one a fifth up from its
 * octave. The voices spread over the registers and none covers another */
static void rnd_registers(void)
{
    static const uint8_t nice_w[] = {0, 0, 0, 0, 1};    /* (NICE: +24 never) */
    const uint8_t *drop_w = rnd_nice ? nice_w : (const uint8_t[]){2, 1, 1, 1, 5};
    static const uint8_t idx[] = {0, 1, 2, 3, 4};
    int8_t oct[5] = {-24, -12, 0, 12, 24}, deal[NTRK];
    uint32_t drop = rnd_pick(idx, drop_w, 5), k, n = 0;
    for (k = 0; k < 5u; k++) if (k != drop && n < NTRK) deal[n++] = oct[k];
    for (k = NTRK - 1u; k > 0u; k--) {
        uint32_t j = rnd_next() % (k + 1u);
        int8_t v = deal[k]; deal[k] = deal[j]; deal[j] = v;
    }
    for (k = 0; k < NTRK; k++) rnd_tp(k, P_TRANS, deal[k] + (rnd_pct(10) && deal[k] < 24 ? 7 : 0));
}

/* AMBIENT's key: modes mostly (MIN DOR MIX PEN MPEN PHRY LYD MAJ), now and then a stranger one; QNT CHRD on every
 * track, so the drone and the melodies follow the chords */
static void rnd_key_ambient(void)
{
    static const uint8_t sc[] = {2, 3, 4, 5, 6, 8, 9, 1}, sw[] = {4, 5, 3, 3, 4, 2, 3, 2};
    const param_desc_t *d = pid_desc(PID_TRACK(P_SCALE), 0);
    int32_t top = d ? d->max : 14, scale = rnd_nice || rnd_pct(80) ? (int32_t)rnd_pick(sc, sw, NELEM(sc)) : rnd_rr(16, top);
    int32_t root = rnd_rr(0, 11);                       /* (NICE: the modes always, no stranger tuning) */
    uint32_t tr;
    rnd_tx(0, TX_KEY_LINK, 1);
    for (tr = 0; tr < NTRK; tr++) {
        rnd_tp(tr, P_SCALE, scale);
        rnd_tp(tr, P_ROOT, root);
        rnd_tp(tr, P_QUANT, Q_CHRD);
    }
    scale_share(&trk[0]);
}
/* LOOPS' key: the bright modes (MAJ LYD PEN MIX DOR MPEN MIN); the generators' own notes in it (QNT as by default) */
static void rnd_key_loops(void)
{
    static const uint8_t sc[] = {1, 9, 5, 4, 3, 6, 2}, sw[] = {4, 4, 4, 2, 2, 2, 1};
    int32_t scale = (int32_t)rnd_pick(sc, sw, NELEM(sc)), root = rnd_rr(0, 11);
    uint32_t tr;
    rnd_tx(0, TX_KEY_LINK, 1);
    for (tr = 0; tr < NTRK; tr++) {
        rnd_tp(tr, P_SCALE, scale);
        rnd_tp(tr, P_ROOT, root);
        rnd_tp(tr, P_QUANT, TP[P_QUANT].def);
    }
    scale_share(&trk[0]);
}
/* LOOPS (after Eno's Music for Airports): each track a EUCLID loop of a length of its own (7 9 11 13 15 17 19 23: none
 * a multiple of another but 9 15), one to three notes in it, all on one slow DIV, so the loops drift against each other
 * and seldom line up the same way; the seed's pitches, the loop as it is (EVOLVE now and then, every 16 bars, a little);
 * nothing dropped, no ratchet, swing or FORM. The first loop starts on its first step (no long silence) */
static void rnd_loops_generators(void)
{
    static const uint8_t lens[] = {7, 9, 11, 13, 15, 17, 19, 23};
    static const uint8_t dv[] = {0, 1, 6}, dw[] = {5, 3, 3};   /* 1/4 1/8 1/2 */
    uint8_t pool[NELEM(lens)];
    uint32_t tr, k, n = NELEM(lens), div = rnd_pick(dv, dw, NELEM(dv));
    memcpy(pool, lens, sizeof pool);
    for (tr = 0; tr < TESSERA_TRACKS; tr++) {
        int32_t len;
        k = rnd_next() % n;
        len = pool[k];
        pool[k] = pool[--n];
        rnd_tx(tr, TX_MODE, TESS_ENG_EUCLID);
        rnd_tx(tr, TX_EUC_STEPS, len);
        rnd_tx(tr, TX_EUC_PULSES, rnd_rr(1, 3));
        rnd_tx(tr, TX_EUC_ROT, tr == 0u ? 0 : rnd_rr(0, len - 1));
        rnd_tx(tr, TX_EV_SEED, rnd_rr(1, (int32_t)TESS_SEED_MAX));
        rnd_tx(tr, TX_EV_EVERY, rnd_pct(30) ? 5 : 0);
        rnd_tx(tr, TX_EV_AMOUNT, rnd_rr(10, 20));
        rnd_tx(tr, TX_FM_FORM, 0);
        rnd_tx(tr, TX_TW_SHIFT, 0);
        rnd_tx(tr, TX_TW_DROP, 0);
        rnd_tx(tr, TX_TW_HUMAN, rnd_rr(0, 20));
        rnd_tx(tr, TX_TW_FOLLOW, 0);
        rnd_tx(tr, TX_TW_BRANCH, 0); rnd_tx(tr, TX_TW_BR_TO, 0); rnd_tx(tr, TX_TW_LOGIC, 0); rnd_tx(tr, TX_TW_COND, 0);
        rnd_tx(tr, TX_NL_MODE, 0);
        rnd_tx(tr, TX_SUB_RATCH, 0);
        rnd_tx(tr, TX_SUB_HUMAN, rnd_rr(0, 15));
        rnd_tp(tr, P_SDIV, (int32_t)div);
        rnd_tp(tr, P_SGATE, rnd_rr(100, 127));
        rnd_tp(tr, P_SSWING, 0);
    }
}
/* ORBIT: BOUNCE or CONSTELLATION on each track, at a middling density (busier than LOOPS): BOUNCE two or three balls at
 * a moderate SPEED (one in five FALL's leaves instead), CONSTELLATION a middling sky, its ship steered by AUTO; 1/8
 * mostly; EVOLVE now and then, a little dropped now and then, no ratchet, swing or FORM */
static void rnd_orbit_generators(void)
{
    static const uint8_t dv[] = {1, 2, 0}, dw[] = {6, 3, 1};   /* 1/8 1/16 1/4 */
    uint32_t tr;
    for (tr = 0; tr < TESSERA_TRACKS; tr++) {
        if (rnd_pct(50)) {
            rnd_tx(tr, TX_MODE, TESS_ENG_BOUNCE);
            rnd_tx(tr, TX_BNC_BALLS, rnd_pct(20) ? 5 : rnd_rr(2, 3));
            rnd_tx(tr, TX_BNC_SPEED, rnd_rr(16, 40));
            rnd_tx(tr, TX_BNC_ANGLE, rnd_rr(15, 75));
        } else {
            rnd_tx(tr, TX_MODE, TESS_ENG_CONSTEL);
            rnd_tx(tr, TX_CS_Y, rnd_rr(3, 11));
            rnd_tx(tr, TX_CS_DENS, rnd_rr(45, 75));
            rnd_tx(tr, TX_CS_SIZE, rnd_rr(3, 6));
            rnd_tx(tr, TX_CS_AUTO, rnd_rr(1, 10));
            rnd_tx(tr, TX_CS_LEN, rnd_rr(8, 16));
            rnd_tx(tr, TX_CS_FILL, rnd_pct(25));
        }
        rnd_tx(tr, TX_EV_SEED, rnd_rr(1, (int32_t)TESS_SEED_MAX));
        rnd_tx(tr, TX_EV_EVERY, rnd_pct(50) ? rnd_rr(2, 4) : 0);
        rnd_tx(tr, TX_EV_AMOUNT, rnd_rr(10, 30));
        rnd_tx(tr, TX_EV_LOCK, rnd_rr(40, 90));
        rnd_tx(tr, TX_FM_FORM, 0);
        rnd_tx(tr, TX_TW_SHIFT, 0);
        rnd_tx(tr, TX_TW_DROP, rnd_pct(20) ? rnd_rr(5, 10) : 0);
        rnd_tx(tr, TX_TW_HUMAN, rnd_rr(0, 25));
        rnd_tx(tr, TX_TW_FOLLOW, 0);
        rnd_tx(tr, TX_TW_BRANCH, 0); rnd_tx(tr, TX_TW_BR_TO, 0); rnd_tx(tr, TX_TW_LOGIC, 0); rnd_tx(tr, TX_TW_COND, 0);
        rnd_tx(tr, TX_NL_MODE, 0);
        rnd_tx(tr, TX_SUB_RATCH, 0);
        rnd_tx(tr, TX_SUB_HUMAN, rnd_rr(0, 15));
        rnd_tp(tr, P_SDIV, (int32_t)rnd_pick(dv, dw, NELEM(dv)));
        rnd_tp(tr, P_SGATE, rnd_rr(80, 127));
        rnd_tp(tr, P_SSWING, 0);
    }
}

/* LOOPS' voices on top of NICE's: a soft onset and a long tail (the loops' notes overlap), POLY, no glide */
static void rnd_loops_sound(uint32_t tr)
{
    track_t *t = &trk[tr];
    rnd_tp(tr, P_ATK, rnd_rr(10, 60));
    rnd_tp(tr, P_DEC, rnd_rr(60, 127));
    rnd_tp(tr, P_SUS, rnd_rr(40, 110));
    rnd_tp(tr, P_REL, rnd_rr(90, 127));
    rnd_tp(tr, P_VOICE, V_POLY);
    rnd_tp(tr, P_GLIDE, 0);
    rnd_tp(tr, P_LEVEL, rnd_rr(80, 96));
    if (t->eng_req == ENGI_ORACLE) {
        uint8_t *p = or_patch[tr];
        p[OR_ATTACK_AMP] = (uint8_t)rnd_rr(10, 60);
        p[OR_RELEASE_AMP] = (uint8_t)rnd_rr(90, 127);
        p[OR_RELEASE_ON] = 1;
    }
}

/* The effects of the modes after RND (AMBIENT .. PULSE), the light ones only (rnd_fx_light): send effect 1 always,
 * send 2 most rolls (SPLIT: a send to each; the two never alike), send 3 off, then FX 1 on two tracks at most (the
 * tracks in a random order) while points are left, each of a kind by the mode's weights among those its points allow.
 * The points as RND's (rnd_fx_points, a send's bus counted), the mode's by what its engines leave (rnd_fx_mode_points:
 * AMBIENT and NICE heavy before any effect, PULSE and GLITCH light; the three sends and four inserts at 14 to 20 points
 * had taken the FM-1 to 70 or 80 %); FX 2 and 3 off. Cords modulate them: about half the inserts and up to two of the
 * send effects, a slow source in the gentle modes, a fast one (S&H, the gate, the envelope, fast chaos) in GLITCH and
 * PULSE. The guard (rnd_guard) trims them as RND's if the FM-1 runs hot. */
#define RND_FX_MODE_POINTS 13u                     /* (the most: WOZZA's) */
static uint32_t rnd_fx_mode_points(void)
{
    return rnd_wozza ? 13u : rnd_pulse ? 11u : rnd_glitch || rnd_fall ? 10u : rnd_loops || rnd_orbit ? 9u : 8u;   /* (AMBIENT,
                                                     * NICE: 8; WOZZA: its own, rnd_fx_wozza) */
}
static int rnd_cord_add(const cord_t *c)           /* (the first free cord) */
{
    uint32_t i;
    for (i = 0; i < CORD_LIVE; i++)
        if (!cord_get(i)->amount && cord_get(i)->dst == CD_OFF) return cord_set(i, c);
    return 0;
}
static void rnd_fx_mode(void)
{
    static const uint8_t fx[] = {FX_ECHO, FX_ROOM, FX_TAPE, FX_CHORUS, FX_FILTER};
    /*                                   ECH ROO TAP CHO FIL */
    static const uint8_t gs[5] = {8, 10, 3, 4, 0}, gi[5] = {5, 0, 6, 6, 6};    /* gentle */
    static const uint8_t es[5] = {10, 6, 3, 1, 3}, ei[5] = {6, 0, 4, 2, 10};   /* GLITCH PULSE */
    static const uint8_t ssrc[] = {CS_DRIFT, CS_TORPOR, CS_APATHY, CS_INERTIA, CS_LFO, CS_OCHD1, CS_OCHD3};
    static const uint8_t fsrc[] = {CS_SH, CS_LFO, CS_GATE, CS_ENV, CS_OCHD5, CS_OCHD7, CS_OCHD8, CS_NOTE};
    int el = rnd_glitch || rnd_pulse;
    const uint8_t *ws = el ? es : gs, *wi = el ? ei : gi;
    uint8_t w[5], tracks[NTRK] = {0, 1, 2, 3};
    uint32_t used = 0, s, k, tr, n_sendmods = 0, n_ins = 0, pts = rnd_fx_mode_points();
    pid_set(PID_GLOBAL(G_FXMODE), 0, 1);
    for (s = 0; s < 3u; s++) {                      /* the send effects */
        uint32_t base = G_MFXTYPE + s * 4u, type = FX_OFF, j;
        if (s == 0u || (s == 1u && rnd_pct(70))) {
            for (k = 0, j = 0; k < 5u; k++) {
                w[k] = ws[k];
                if (s && song.g[G_MFXTYPE] == fx[k]) w[k] = 0;   /* (never alike) */
                if (used + rnd_fx_points(fx[k], 1) > pts) w[k] = 0;
                j += w[k];
            }
            type = j ? rnd_pick(fx, w, 5u) : FX_OFF;   /* (nothing the points allow: off) */
        }
        pid_set(PID_GLOBAL(base), 0, (int32_t)type);
        pid_set(PID_GLOBAL(base + 1u), 0, rnd_rr(60, 120));
        pid_set(PID_GLOBAL(base + 2u), 0, rnd_rr(0, 127));
        pid_set(PID_GLOBAL(base + 3u), 0, rnd_rr(20, 120));
        used += rnd_fx_points(type, 1);
        if (type != FX_OFF && n_sendmods < 2u && rnd_pct(50)) {   /* (a cord on it) */
            cord_t c;
            memset(&c, 0, sizeof c);
            c.src_trk = c.dst_trk = (uint8_t)(rnd_next() % NTRK);
            c.src = el ? fsrc[rnd_next() % NELEM(fsrc)] : ssrc[rnd_next() % NELEM(ssrc)];
            c.dst = (uint8_t)(CD_SF1_MIX + s * 3u + (uint32_t)rnd_rr(0, 2));
            c.amount = (int8_t)(rnd_pct(50) ? rnd_rr(el ? 25 : 15, el ? 70 : 45) : -rnd_rr(el ? 25 : 15, el ? 70 : 45));
            if (rnd_cord_add(&c)) n_sendmods++;
        }
    }
    pid_set(PID_GLOBAL(G_FXRETURN), 0, rnd_rr(60, 100));
    for (k = NTRK - 1u; k > 0u; k--) {              /* (the inserts: the tracks in a random order) */
        uint32_t j = rnd_next() % (k + 1u);
        uint8_t v = tracks[k]; tracks[k] = tracks[j]; tracks[j] = v;
    }
    for (tr = 0; tr < NTRK; tr++) {
        uint32_t t = tracks[tr], type, j;
        for (k = 0, j = 0; k < 5u; k++) {
            w[k] = used + rnd_fx_points(fx[k], 0) <= pts ? wi[k] : 0u;
            j += w[k];
        }
        type = j && n_ins < RND_INSERT_TRACKS ? rnd_pick(fx, w, 5u) : FX_OFF;   /* (two tracks at most, as RND's) */
        used += rnd_fx_points(type, 0);
        n_ins += type != FX_OFF;
        for (s = 0; s < 3u; s++) rnd_tp(t, P_FXTYPE + s * 4u, s == 0u ? (int32_t)type : FX_OFF);
        if (type != FX_OFF) {
            rnd_tp(t, P_FXMIX, el ? rnd_rr(40, 110) : rnd_rr(30, 90));
            rnd_tp(t, P_FXRATE, rnd_rr(0, 127));
            rnd_tp(t, P_FXCHAR, rnd_rr(0, 127));
            if (rnd_pct(50)) {                      /* (a cord on it) */
                cord_t c;
                memset(&c, 0, sizeof c);
                c.src_trk = c.dst_trk = (uint8_t)t;
                c.src = el ? fsrc[rnd_next() % NELEM(fsrc)] : ssrc[rnd_next() % NELEM(ssrc)];
                c.dst = (uint8_t)(CD_FX1_MIX + (uint32_t)rnd_rr(0, 2));
                c.amount = (int8_t)(rnd_pct(50) ? rnd_rr(el ? 25 : 15, el ? 70 : 45) : -rnd_rr(el ? 25 : 15, el ? 70 : 45));
                rnd_cord_add(&c);
            }
        }
        rnd_tp(t, P_FXSEND, rnd_rr(el ? 10 : 30, el ? 80 : 110));
        rnd_tp(t, P_FXSEND2, song.g[G_MFX2TYPE] != FX_OFF ? rnd_rr(0, el ? 90 : 80) : 0);
        rnd_tp(t, P_FXSEND3, 0);
    }
}
/* WOZZA's effects, the light ones: a ROOM always (send 1, every track in it), an ECHO now and then (send 2, slow, low),
 * TAPE on the bed or the ostinato (its colour wandering), a low-pass FILTER on the melody (dark: the references hold
 * little above 2 kHz); within rnd_fx_mode_points' 13. FX 2 and 3 off */
static void rnd_fx_wozza(const uint8_t *role)
{
    uint32_t tr, s, tape = NTRK, mel = NTRK, echo = (uint32_t)rnd_pct(45);
    pid_set(PID_GLOBAL(G_FXMODE), 0, 1);
    for (s = 0; s < 3u; s++) {
        uint32_t base = G_MFXTYPE + s * 4u;
        pid_set(PID_GLOBAL(base), 0, s == 0u ? FX_ROOM : s == 1u && echo ? FX_ECHO : FX_OFF);
        pid_set(PID_GLOBAL(base + 1u), 0, s == 0u ? rnd_rr(70, 110) : rnd_rr(40, 75));
        pid_set(PID_GLOBAL(base + 2u), 0, s == 0u ? rnd_rr(60, 115) : rnd_rr(20, 70));
        pid_set(PID_GLOBAL(base + 3u), 0, s == 0u ? rnd_rr(40, 100) : rnd_rr(20, 60));
    }
    pid_set(PID_GLOBAL(G_FXRETURN), 0, rnd_rr(70, 100));
    for (tr = 0; tr < NTRK; tr++) {
        if (role[tr] == RND_ROLE_MELODY && mel == NTRK) mel = tr;
        if ((role[tr] == RND_ROLE_DRONE || role[tr] == RND_ROLE_OSTI) && (tape == NTRK || rnd_pct(50))) tape = tr;
    }
    for (tr = 0; tr < NTRK; tr++) {
        uint32_t type = tr == tape ? FX_TAPE : tr == mel ? FX_FILTER : FX_OFF;
        for (s = 0; s < 3u; s++) rnd_tp(tr, P_FXTYPE + s * 4u, s == 0u ? (int32_t)type : FX_OFF);
        if (type == FX_TAPE) {
            rnd_tp(tr, P_FXMIX, rnd_rr(80, 127));
            rnd_tp(tr, P_FXRATE, rnd_rr(30, 75));
            rnd_tp(tr, P_FXCHAR, rnd_rr(50, 110));
        } else if (type == FX_FILTER) {
            rnd_tp(tr, P_FXMIX, 127);
            rnd_tp(tr, P_FXRATE, rnd_rr(74, 90));   /* (1.2 to 2.8 kHz) */
            rnd_tp(tr, P_FXCHAR, rnd_rr(0, 30));
        }
        rnd_tp(tr, P_FXSEND, role[tr] == RND_ROLE_NOISE ? rnd_rr(15, 45) : rnd_rr(50, 110));
        rnd_tp(tr, P_FXSEND2, echo && role[tr] == RND_ROLE_MELODY ? rnd_rr(30, 80) : echo ? rnd_rr(0, 30) : 0);
        rnd_tp(tr, P_FXSEND3, 0);
    }
}
/* WOZZA's cords: slow wanderers on the bed's and the texture's colour, the melody's pan; now and then the melody's level
 * dipping at random (a tape's dropouts), the tape's colour wandering */
static void rnd_cords_wozza(const uint8_t *role)
{
    uint32_t i = 0, tr;
    cord_t c;
    for (tr = 0; tr < NTRK && i < CORD_LIVE; tr++) {
        memset(&c, 0, sizeof c);
        c.src_trk = c.dst_trk = (uint8_t)tr;
        if (role[tr] == RND_ROLE_DRONE || role[tr] == RND_ROLE_NOISE) {
            c.src = rnd_pct(60) ? CS_DRIFT : CS_TORPOR;
            c.dst = CD_CUTOFF;
            c.amount = (int8_t)(rnd_pct(50) ? rnd_rr(10, 30) : -rnd_rr(10, 30));
        } else if (role[tr] == RND_ROLE_MELODY) {
            c.src = CS_TORPOR;
            c.dst = CD_PAN;
            c.amount = (int8_t)rnd_rr(10, 30);
        } else continue;
        if (cord_set(i, &c)) i++;
        if (role[tr] == RND_ROLE_MELODY && rnd_pct(35) && i < CORD_LIVE) {
            memset(&c, 0, sizeof c);
            c.src_trk = c.dst_trk = (uint8_t)tr;
            c.src = CS_SH;
            c.dst = CD_GAIN;
            c.amount = (int8_t)-rnd_rr(15, 35);
            c.uni = 1;
            if (cord_set(i, &c)) i++;
        }
        if (trk[tr].p[P_FXTYPE] == FX_TAPE && i < CORD_LIVE) {
            memset(&c, 0, sizeof c);
            c.src_trk = c.dst_trk = (uint8_t)tr;
            c.src = CS_DRIFT;
            c.dst = CD_FX1_CHAR;
            c.amount = (int8_t)(rnd_pct(50) ? rnd_rr(15, 40) : -rnd_rr(15, 40));
            if (cord_set(i, &c)) i++;
        }
    }
    for (memset(&c, 0, sizeof c); i < CORD_LIVE; i++) cord_set(i, &c);
}
static uint32_t rnd_gen_dest(uint32_t tr);
/* AMBIENT's cords: two to five, slow sources only, into the sound (never its pitch) and the melodies' generators */
static void rnd_cords_ambient(void)
{
    static const uint8_t src[] = {CS_LFO, CS_DRIFT, CS_TORPOR, CS_APATHY, CS_INERTIA, CS_OCHD1, CS_OCHD3, CS_SLEW};
    static const uint8_t sw[] = {6, 12, 6, 6, 6, 5, 5, 4};
    static const uint8_t dst[] = {CD_CUTOFF, CD_SHAPE, CD_PAN, CD_SPREAD, CD_LFO_DEPTH, CD_E1, CD_E2, CD_E3, CD_E4, CD_E5,
                                  CD_E6, 0xFF};
    static const uint8_t dw[] = {14, 10, 6, 4, 4, 5, 5, 5, 5, 4, 4, 8};
    static const uint8_t nw[] = {14, 10, 6, 4, 4, 0, 0, 0, 0, 0, 0, 8};   /* NICE: not the engines' own eight */
    uint32_t i, n = (uint32_t)rnd_rr(2, 5);
    for (i = 0; i < CORD_LIVE; i++) {
        cord_t c;
        memset(&c, 0, sizeof c);
        if (i < n) {
            c.src = (uint8_t)rnd_pick(src, sw, NELEM(src));
            c.src_trk = (uint8_t)rnd_rr(0, NTRK - 1);
            c.dst_trk = (uint8_t)(rnd_pct(60) ? c.src_trk : (uint32_t)rnd_rr(0, NTRK - 1));
            c.dst = (uint8_t)rnd_pick(dst, rnd_nice ? nw : dw, NELEM(dst) - (rnd_loops ? 1u : 0u));   /* (LOOPS: the
                                                     * loops left as they are: not the generators') */
            if (c.dst == 0xFFu) c.dst = (uint8_t)(c.dst_trk >= 2u ? rnd_gen_dest(c.dst_trk) : CD_CUTOFF);
            c.amount = (int8_t)(rnd_pct(50) ? rnd_rr(10, rnd_nice ? 30 : 50) : -rnd_rr(10, rnd_nice ? 30 : 50));
            c.uni = (uint8_t)rnd_pct(30);
        }
        if (!cord_set(i, &c)) { memset(&c, 0, sizeof c); cord_set(i, &c); }
    }
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
    uint32_t i, n = (uint32_t)rnd_rr(3, 9);
    for (i = 0; i < CORD_LIVE; i++) {
        cord_t c;
        memset(&c, 0, sizeof c);
        if (i < n) {
            c.src = (uint8_t)rnd_pick(src, sw, NELEM(src));
            c.src_trk = (uint8_t)rnd_rr(0, NTRK - 1);
            c.dst_trk = (uint8_t)(rnd_pct(60) ? c.src_trk : (uint32_t)rnd_rr(0, NTRK - 1));
            c.dst = (uint8_t)rnd_pick(dst, dw, NELEM(dst));
            if (c.dst == 0xFFu) c.dst = (uint8_t)rnd_gen_dest(c.dst_trk);
            c.amount = (int8_t)(c.dst == CD_PITCH ? (rnd_pct(50) ? rnd_rr(2, 12) : -rnd_rr(2, 12)) : rnd_rr(-70, 70));
            c.uni = (uint8_t)rnd_pct(30);
            if (!c.amount) c.amount = 20;
        }
        if (!cord_set(i, &c)) { memset(&c, 0, sizeof c); cord_set(i, &c); }
    }
}

/* ------------------------------------------------------------ the guard: a roll that is too heavy lightens itself --- */
/* The FM-1 knows its own load (audio.c song.cpu_q8: the audio's share of the time, 1/256, over ~93 ms). Past ~80 % it
 * sheds voices (a stutter) and past 100 % blocks are late (a digital crunch), and what a roll costs is only known once it
 * plays: so while a roll plays, a load over RND_GUARD_HOT for RND_GUARD_MS, or over RND_GUARD_PEAK at all, takes one
 * step off it (rnd_trim), then waits RND_GUARD_GAP to see. Only a roll's own sound: off once the hold swaps back. */
#define RND_GUARD_HOT 156u                          /* 61 % (tessera-rnd-19: from 72; the FM-1 clicked past 70) */
#define RND_GUARD_PEAK 179u                         /* 70 % (from 85) */
#define RND_GUARD_MS 300u
#define RND_GUARD_GAP 1500u
#define RND_GUARD_GRACE 600u                        /* (after a roll: the engines' switch and its fades) */
static struct {
    uint8_t on, step, hot;
    uint32_t hot_since, wait_until;
} rnd_guard_s;

static void rnd_guard_start(void)
{
    rnd_guard_s.on = 1;
    rnd_guard_s.step = 0;
    rnd_guard_s.hot = 0;
    rnd_guard_s.wait_until = fm1_ms + RND_GUARD_GRACE;
}

/* the insert (FX 1) costing most, on any track; NTRK: none */
static uint32_t rnd_dearest_insert(void)
{
    uint32_t tr, best = NTRK, pts = 0;
    for (tr = 0; tr < NTRK; tr++) {
        uint32_t p = rnd_fx_points((uint32_t)trk[tr].p[P_FXTYPE], 0);
        if (p > pts) pts = p, best = tr;
    }
    return best;
}
/* the POLY track with the most voices sounding now (one voice a line from then on); NTRK: none */
static uint32_t rnd_busiest_poly(void)
{
    uint32_t tr, i, best = NTRK, most = 0, pass;
    for (pass = 0; pass < 2u && best == NTRK; pass++)   /* (a CHORD track's chords last: they need their voices) */
    for (tr = 0; tr < NTRK; tr++) {
        uint32_t n = 0;
        if (!pass && tessera.mode[tr] == TESS_ENG_CHORD) continue;
        if (trk[tr].p[P_VOICE] != V_POLY) continue;
        for (i = 0; i < NVOICE; i++) n += trk[tr].v[i].active;
        if (n >= most) most = n, best = tr;
    }
    return best;
}
/* one step lighter, the cheapest to hear first: the reverb a ROOM, the dearest inserts off, the echo off, then the
 * busiest POLY tracks MONO. 0: nothing left to take */
static int rnd_trim(void)
{
    uint32_t tr;
    if (song.g[G_MFXTYPE] == FX_REVERB) {
        pid_set(PID_GLOBAL(G_MFXTYPE), 0, FX_ROOM);
        return 1;
    }
    if ((tr = rnd_dearest_insert()) < NTRK) {
        rnd_tp(tr, P_FXTYPE, FX_OFF);
        return 1;
    }
    if (song.g[G_MFX2TYPE] != FX_OFF) {
        pid_set(PID_GLOBAL(G_MFX2TYPE), 0, FX_OFF);
        return 1;
    }
    if ((tr = rnd_busiest_poly()) < NTRK) {
        rnd_tp(tr, P_VOICE, V_MONO);
        return 1;
    }
    return 0;
}

/* each UI frame (ui_input) in RND mode */
static void rnd_guard(void)
{
    uint32_t load = song.cpu_q8;
    if (!rnd_guard_s.on || !song.playing) {
        rnd_guard_s.hot = 0;
        return;
    }
    if ((int32_t)(fm1_ms - rnd_guard_s.wait_until) < 0) return;
    if (load <= RND_GUARD_HOT) {
        rnd_guard_s.hot = 0;
        return;
    }
    if (!rnd_guard_s.hot) {
        rnd_guard_s.hot = 1;
        rnd_guard_s.hot_since = fm1_ms;
    }
    if (load <= RND_GUARD_PEAK && fm1_ms - rnd_guard_s.hot_since < RND_GUARD_MS) return;
    rnd_guard_s.hot = 0;
    rnd_guard_s.wait_until = fm1_ms + RND_GUARD_GAP;
    if (rnd_trim()) {
        rnd_guard_s.step++;
        ui_message("LIGHTER");
        ui.force = 1;
    } else {
        rnd_guard_s.on = 0;                         /* (nothing left to take: as it is) */
    }
}

/* ------------------------------------------------------------ DRIFT: MENU > DRIFT --- */
/* While a roll plays (not before it, held), DRIFT changes it a little at a time, within its mode's own rules, so the
 * piece turns into another over the minutes without a new roll: SLOW one change every 4 to 8 bars, FAST every 1 or 2;
 * OFF (the default) none. A change: one of a track's eight engine knobs nudged (never a kind, an enum), its envelope,
 * filter, LFO or pan nudged, a knob of its generator nudged (not LOOPS': its loops as they are), a cord's depth nudged;
 * RND and GLITCH also now and then a new generator or a new sound for one track (the CPU's rules again, the guard
 * watching), and GLITCH's slow two swap their registers (the drone up, the melody down, or back). NICE, FALL, LOOPS and
 * ORBIT nudge by less. settings_rnd_drift: panel.c, saved with REC BUTTON's */
enum { RND_DRIFT_OFF, RND_DRIFT_SLOW, RND_DRIFT_FAST, RND_DRIFTS };
static struct { uint32_t bar, left, steps, moves; } rnd_drift_s;   /* (moves: HALO's movements made) */
static uint32_t rnd_hl_wait;                       /* HALO: bars to its next movement (rnd_halo_move) */
static uint32_t rnd_drift_wait(void)               /* (the bars to wait after the next one) */
{
    return settings_rnd_drift == RND_DRIFT_FAST ? (uint32_t)rnd_rr(0, 1) : (uint32_t)rnd_rr(3, 7);
}
/* HALO's movements, apart from the nudges: SLOW about every 16 to 32 bars (a minute or two at its tempo), FAST every
 * 4 to 8 */
static uint32_t rnd_halo_wait(void)
{
    return settings_rnd_drift == RND_DRIFT_FAST ? (uint32_t)rnd_rr(4, 8) : (uint32_t)rnd_rr(16, 32);
}
static void rnd_drift_start(void)
{
    rnd_drift_s.bar = bar_n;
    rnd_drift_s.left = rnd_drift_wait();
    rnd_hl_wait = rnd_halo || rnd_mirror ? rnd_halo_wait() : 0u;   /* (only HALO draws: the other modes' rolls as they were) */
}
/* a MIRROR movement (DRIFT, on HALO's clock), a quarter each: the line's pace a step (1/2 1/4 1/8, 1/16 when light;
 * the mirror then on its clock, the canon at half), the canon across the line or not, the mirror onto the chord or in
 * parallel (off twelve notes: another parallel), or the mirror to the line's other side */
static void rnd_mirror_move(void)
{
    static const uint8_t pace[] = {6, 0, 1, 2};     /* 1/2 1/4 1/8 1/16: slow to fast */
    uint32_t tr, m = NTRK, t = NTRK, c = NTRK, i, hi, kind = rnd_next() % 4u;
    int32_t md;
    for (tr = 0; tr < NTRK; tr++)
        if (rnd_role_of[tr] == RND_ROLE_MELODY) m = tr;
        else if (rnd_role_of[tr] == RND_ROLE_TINT) t = tr;
        else if (rnd_role_of[tr] == RND_ROLE_CANON) c = tr;
    if (m == NTRK || t == NTRK || c == NTRK) return;
    rnd_drift_s.moves++;
    md = trk[m].p[P_SDIV];
    if (kind == 0u) {                               /* the pace */
        hi = rnd_mr_heavy(m) || rnd_mr_heavy(t) ? 3u : 4u;   /* (FM6 PHYS ORACLE: never fast; the canon half as fast) */
        for (i = 0; i < 4u && pace[i] != (uint8_t)md; i++) {}
        if (i >= 4u) i = 2u;                        /* (8T: as 1/8) */
        if (i == 0u) i = 1u;
        else if (i > hi - 1u) i = hi - 1u;
        else if (i == hi - 1u) i--;
        else i = rnd_pct(50) ? i + 1u : i - 1u;
        md = pace[i];
        rnd_tp(m, P_SDIV, md);
        rnd_tp(t, P_SDIV, md);
        rnd_tp(t, P_VOICE, md == 2 ? V_MONO : V_POLY);
        rnd_tp(c, P_SDIV, rnd_mr_canon_div(md, 0));
    } else if (kind == 1u) {                        /* the canon: across the line, or back to half */
        int32_t d = trk[c].p[P_SDIV] == rnd_mr_canon_div(md, 0) ? rnd_mr_canon_div(md, 1) : rnd_mr_canon_div(md, 0);
        if (rnd_mr_heavy(c) && (d == 2 || d == 4)) d = rnd_mr_canon_div(md, 0);
        rnd_tp(c, P_SDIV, d);
    } else if (kind == 2u) {                        /* the mirror: onto the chord, or in parallel */
        rnd_mr_tint_mode(t, trk[t].p[P_QUANT] != Q_CHRD);
    } else if (trk[m].p[P_TRANS] > 0) {             /* the mirror's side */
        rnd_tp(t, P_TRANS, trk[t].p[P_TRANS] < 0 ? 0 : -12);
    } else {
        rnd_tp(t, P_TRANS, trk[t].p[P_TRANS] > 0 ? -12 : 12);
    }
}
/* a HALO movement, half the time each: the M-voice's pace a step slower or faster (1/2, 1/4, 1/8; FALL's leaves 1/4
 * or 1/8), the T-voice on its clock still, the canon at half; or the bells to the line's other side (an octave above
 * it or below; a high line: beside it or below), Pärt's voices changing places as Für Alina's do. The bells (FOLLOW)
 * sound the line's own note, so their TRANS is from the line's */
static void rnd_halo_move(void)
{
    static const uint8_t pace[] = {6, 0, 1};        /* 1/2 1/4 1/8: slow to fast */
    uint32_t tr, m = NTRK, t = NTRK, c = NTRK, i, lo, mdiv;
    for (tr = 0; tr < NTRK; tr++)
        if (rnd_role_of[tr] == RND_ROLE_MELODY) m = tr;
        else if (rnd_role_of[tr] == RND_ROLE_TINT) t = tr;
        else if (rnd_role_of[tr] == RND_ROLE_CANON) c = tr;
    if (m == NTRK || t == NTRK) return;
    rnd_drift_s.moves++;
    if (rnd_pct(50)) {                              /* the pace */
        for (i = 0; i < 2u && pace[i] != (uint8_t)trk[m].p[P_SDIV]; i++) {}
        lo = tessera.mode[m] == TESS_ENG_BOUNCE ? 1u : 0u;   /* (the leaves never on 1/2) */
        if (i < lo) i = lo;
        else if (i == lo) i++;
        else if (i == 2u) i--;
        else i = rnd_pct(50) ? i + 1u : i - 1u;
        mdiv = pace[i];
        rnd_tp(m, P_SDIV, (int32_t)mdiv);
        rnd_tp(t, P_SDIV, (int32_t)mdiv);
        rnd_tp(t, P_VOICE, mdiv == 1u ? V_MONO : V_POLY);   /* (on 1/8: one bell at a time) */
        if (c < NTRK) rnd_tp(c, P_SDIV, mdiv == 1u ? 0 : mdiv == 0u ? 6 : 7);
        return;
    }
    if (trk[m].p[P_TRANS] > 0)                      /* the bells: a high line's beside it or below */
        rnd_tp(t, P_TRANS, trk[t].p[P_TRANS] < 0 ? 0 : -12);
    else
        rnd_tp(t, P_TRANS, trk[t].p[P_TRANS] > 0 ? -12 : 12);
}
static int32_t rnd_nudge(int32_t v, int32_t lo, int32_t hi, int32_t small, int32_t big)
{
    int32_t d = rnd_rr(small, big);
    v += rnd_pct(50) ? d : -d;
    return clamp(v, lo, hi);
}
/* the generator knobs DRIFT may nudge, by generator */
static uint32_t rnd_drift_gen_id(uint32_t tr)
{
    static const uint8_t tur[] = {TX_TUR_LEN, TX_TUR_PROB}, euc[] = {TX_EUC_PULSES, TX_EUC_ROT, TX_EUC_STEPS},
                         mkv[] = {TX_MKV_LEAP, TX_MKV_DENS}, bnc[] = {TX_BNC_SPEED, TX_BNC_ANGLE},
                         awk[] = {TX_AWK_PROB, TX_AWK_LENA}, pol[] = {TX_POL_DIV1, TX_POL_DIV2}, qd[] = {TX_QD_PROB, TX_QD_DIR},
                         cs[] = {TX_CS_DENS, TX_CS_Y}, sr[] = {TX_SR_START, TX_SR_INTV}, ev[] = {TX_EV_AMOUNT, TX_TW_DROP};
    const uint8_t *l = ev;
    uint32_t n = 2;
    switch (tessera.mode[tr]) {
    case TESS_ENG_TURING: l = tur; break;
    case TESS_ENG_EUCLID: l = euc; n = 3; break;
    case TESS_ENG_MARKOV: l = mkv; break;
    case TESS_ENG_MORPH: return TX_MRP_POS;
    case TESS_ENG_BOUNCE: l = bnc; break;
    case TESS_ENG_AWAKE: l = awk; break;
    case TESS_ENG_POLY: l = pol; break;
    case TESS_ENG_QUAD: l = qd; break;
    case TESS_ENG_CONSTEL: l = cs; break;
    case TESS_ENG_SERIES: l = sr; break;
    case TESS_ENG_CHORD: return TX_CH_PROG;
    default: break;
    }
    return l[rnd_next() % n];
}
static void rnd_drift_step(void)
{
    static const uint8_t kinds[] = {0, 1, 2, 3, 4, 5, 6};
    static const uint8_t shape[] = {P_DEC, P_ED_FLT, P_LD_FLT, P_LRATE, P_PAN, P_DETUNE, P_LD_SHP, P_ED_SHP};
    uint32_t tr = rnd_next() % NTRK, k, wild = rnd_glitch || rnd_pulse || rnd_setting() == RND_FREE;
    int32_t small = rnd_nice ? 3 : 6, big = rnd_nice ? 10 : 22;
    track_t *t = &trk[tr];
    uint8_t kw[7] = {30, 20, 25, 15, 0, 0, 0};
    if (rnd_loops) kw[2] = 0;                       /* (LOOPS: its loops as they are) */
    if (rnd_glitch) kw[4] = 4;                      /* (PULSE: its bass stays low) */
    if (wild) kw[5] = settings_rnd_drift == RND_DRIFT_FAST ? 6 : 4, kw[6] = 3;
    rnd_drift_s.steps++;
    switch (rnd_pick(kinds, kw, NELEM(kinds))) {
    case 0:                                         /* one of the engine's eight */
        for (k = 0; k < 8u; k++) {
            uint32_t e = (uint32_t)rnd_next() % 8u;
            const param_desc_t *d = pid_desc(PID_TRACK(P_E0 + e), tr);
            if (!d || d->fmt == F_ENUM || d->max <= d->min) continue;
            if ((rnd_fall || rnd_halo) && t->eng_req == 6u && (e == 1u || e == 2u)) continue;   /* (FALL, HALO: TRIO's
                                                     * intervals kept) */
            rnd_tp(tr, P_E0 + e, rnd_nudge(t->p[P_E0 + e], d->min, d->max, small, big));
            return;
        }
        return;
    case 1: {                                       /* the envelope, filter, LFO or pan; or its effect's */
        uint32_t id = shape[rnd_next() % NELEM(shape)];
        const param_desc_t *d;
        if (t->p[P_FXTYPE] != FX_OFF && rnd_pct(30)) {
            static const uint8_t fxp[] = {P_FXMIX, P_FXRATE, P_FXCHAR};
            id = fxp[rnd_next() % NELEM(fxp)];
            d = pid_desc(PID_TRACK(id), tr);
            if (d && d->max > d->min) rnd_tp(tr, id, rnd_nudge(t->p[id], d->min, d->max, small, big));
            if (t->p[P_FXTYPE] == FX_SPIT) rnd_cap(tr, P_FXCHAR, 70);   /* (SPIT's BITE not flat out) */
            return;
        }
        d = pid_desc(PID_TRACK(id), tr);
        if ((rnd_glitch || rnd_pulse) && rnd_perc(tr) && rnd_pct(25)) id = P_ED_PIT, d = pid_desc(PID_TRACK(id), tr);
        if (d && d->max > d->min) rnd_tp(tr, id, rnd_nudge(t->p[id], d->min, d->max, small, big));
        if (id == P_DEC && rnd_glitch && rnd_role_of[tr] == RND_ROLE_GLITCH) rnd_cap(tr, P_DEC, 40);
        if (id == P_DEC && rnd_pulse && rnd_perc(tr)) rnd_cap(tr, P_DEC, 70);
        return;
    }
    case 2: {                                       /* a knob of its generator */
        uint32_t id;
        const param_desc_t *d;
        if (tessera.mode[tr] == TESS_ENG_OFF) return;
        id = rnd_drift_gen_id(tr);
        if ((rnd_halo || rnd_mirror) && (tessera.mode[tr] == TESS_ENG_CHORD || (id == TX_SR_INTV && rnd_halo))) return;   /* (HALO: the triad held,
                                                     * the line by step) */
        d = pid_desc(PID_TESS(id), tr);
        if (!d || d->max <= d->min) return;
        if (d->max - d->min <= 40) rnd_tx(tr, id, rnd_nudge(pid_get(PID_TESS(id), tr), d->min, d->max, 1, 2));
        else rnd_tx(tr, id, rnd_nudge(pid_get(PID_TESS(id), tr), d->min, d->max, small, big));
        return;
    }
    case 3:                                         /* a cord's depth */
        for (k = 0; k < 4u; k++) {
            uint32_t i = (uint32_t)rnd_next() % CORD_LIVE;
            cord_t c = *cord_get(i);
            if (!c.amount) continue;
            c.amount = (int8_t)rnd_nudge(c.amount, rnd_nice ? -50 : -100, rnd_nice ? 50 : 100, small, big);
            if (!c.amount) c.amount = 5;
            if (!cord_set(i, &c)) continue;
            return;
        }
        return;
    case 4: {                                       /* GLITCH: the slow two swap their registers */
        uint32_t a = NTRK, b = NTRK;
        int16_t v;
        for (k = 0; k < NTRK; k++)
            if (rnd_role_of[k] == RND_ROLE_DRONE) a = k;
            else if (rnd_role_of[k] == RND_ROLE_MELODY) b = k;
        if (a == NTRK || b == NTRK) return;
        v = trk[a].p[P_TRANS];
        rnd_tp(a, P_TRANS, trk[b].p[P_TRANS]);
        rnd_tp(b, P_TRANS, v);
        return;
    }
    case 5: {                                       /* RND, GLITCH: a new generator for one track */
        uint32_t chords = 0;
        for (k = 0; k < NTRK; k++) chords += k != tr && tessera.mode[k] == TESS_ENG_CHORD;
        if (tessera.mode[tr] == TESS_ENG_CHORD) return;   /* (the chords the others follow: kept) */
        if (rnd_glitch) rnd_glitch_generator(tr, rnd_role_of[tr], &chords);
        else if (rnd_pulse) rnd_pulse_generator(tr, rnd_role_of[tr], &chords);
        else if (rnd_wozza) rnd_wozza_generator(tr, rnd_role_of[tr], &chords);
        else if (rnd_halo || rnd_mirror) {          /* (HALO, MIRROR: the line anew, the two that follow it with it) */
            if (rnd_halo) rnd_halo_generators(rnd_role_of);
            else rnd_mirror_generators(rnd_role_of);
            for (k = 0; k < TESSERA_TRACKS; k++) rnd_cpu(k);
            return;
        } else rnd_generator(tr, &chords);
        rnd_cpu(tr);
        return;
    }
    default: {                                      /* RND, GLITCH: a new sound for one track */
        uint32_t fm6s = 0, phys = 0, oracles = 0;
        int16_t keep[4];
        static const uint8_t kept[4] = {P_TRANS, P_SCALE, P_ROOT, P_QUANT};
        for (k = 0; k < NTRK; k++) {
            if (k == tr) continue;
            fm6s += trk[k].eng_req == ENGI_FM6;
            phys += trk[k].eng_req == ENGI_PHYS;
            oracles += trk[k].eng_req == ENGI_ORACLE;
        }
        for (k = 0; k < 4u; k++) keep[k] = t->p[kept[k]];
        undo_depth++;
        if (rnd_pulse) rnd_pulse_sound(tr, rnd_role_of[tr], &fm6s, &phys, &oracles);
        else if (rnd_wozza) rnd_wozza_sound(tr, rnd_role_of[tr], &fm6s, &phys, &oracles);
        else if (rnd_halo) rnd_halo_sound(tr, rnd_role_of[tr], &fm6s, &phys, &oracles);
        else if (rnd_mirror) rnd_mirror_sound(tr, rnd_role_of[tr], &fm6s, &phys, &oracles);
        else rnd_sound(tr, &fm6s, &phys, &oracles, rnd_role_of[tr]);
        undo_depth--;
        undo_clear();
        for (k = 0; k < 4u; k++) rnd_tp(tr, kept[k], keep[k]);
        rnd_cpu(tr);
        rnd_tails();
        rnd_guard_start();                          /* (the guard watches the new sound) */
        return;
    }
    }
}
/* each UI frame in an RND mode (ui_input): a bar on, the next change when its wait is over */
static void rnd_drift(void)
{
    if (!settings_rnd_drift || !rnd_snap.valid || rnd_backed || !song.playing) {
        rnd_drift_s.bar = bar_n;
        return;
    }
    if (bar_n == rnd_drift_s.bar) return;
    rnd_drift_s.bar = bar_n;
    if (rnd_halo || rnd_mirror) {                   /* (HALO, MIRROR: their movements on their own, slower clock) */
        if (rnd_hl_wait) rnd_hl_wait--;
        else {
            rnd_hl_wait = rnd_halo_wait();
            if (rnd_halo) rnd_halo_move();
            else rnd_mirror_move();
        }
    }
    if (rnd_drift_s.left) {
        rnd_drift_s.left--;
        return;
    }
    rnd_drift_s.left = rnd_drift_wait();
    rnd_drift_step();
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

/* the roles of the four tracks: RND free on each; AMBIENT a drone, chords and two melodies, in a random order (so the
 * drone is not always track 1) */
static void rnd_roles(uint8_t *role, uint32_t ambient)
{
    uint32_t k;
    role[0] = RND_ROLE_DRONE; role[1] = RND_ROLE_CHORDS; role[2] = role[3] = RND_ROLE_MELODY;
    if (!ambient) {
        for (k = 0; k < NTRK; k++) role[k] = RND_ROLE_FREE;
        return;
    }
    for (k = NTRK - 1u; k > 0u; k--) {
        uint32_t j = rnd_next() % (k + 1u);
        uint8_t v = role[k]; role[k] = role[j]; role[j] = v;
    }
}

/* GLITCH's: a drone, a melody and two glitch tracks, in a random order */
static void rnd_roles_glitch(uint8_t *role)
{
    uint32_t k;
    role[0] = RND_ROLE_DRONE; role[1] = RND_ROLE_MELODY; role[2] = role[3] = RND_ROLE_GLITCH;
    for (k = NTRK - 1u; k > 0u; k--) {
        uint32_t j = rnd_next() % (k + 1u);
        uint8_t v = role[k]; role[k] = role[j]; role[j] = v;
    }
}

static void rnd_drift_start(void);
static void rnd_roll(void)
{
    uint32_t tr, fm6s = 0, phys = 0, oracles = 0, chords = 0;
    uint32_t ambient = rnd_setting() >= RND_AMBIENT && rnd_setting() != RND_GLITCH && rnd_setting() != RND_PULSE &&
                       rnd_setting() != RND_WOZZA && rnd_setting() != RND_HALO && rnd_setting() != RND_MIRROR;
    uint8_t role[NTRK];
    momentary_restore();
    voice_soft_fade = 1;                            /* (ui_input sets it each frame too: the budget's gentle fades) */
    rnd_seed = rnd_next() ^ fm1_ticks() ^ (fm1_ms << 7);
    if (!rnd_seed) rnd_seed = 0x2545F491u;
    rnd_state = rnd_seed;
    rnd_take();                                     /* (the hold swaps back to this) */
    rnd_backed = 0;
    rnd_nice = (uint8_t)(rnd_setting() == RND_NICE || rnd_setting() == RND_FALL);   /* (NICE: AMBIENT's roles, the
                                                     * nicer sounds; FALL too, on TRIO's triangles) */
    rnd_fall = (uint8_t)(rnd_setting() == RND_FALL);
    rnd_loops = (uint8_t)(rnd_setting() == RND_LOOPS);
    rnd_orbit = (uint8_t)(rnd_setting() == RND_ORBIT);
    rnd_glitch = (uint8_t)(rnd_setting() == RND_GLITCH);
    rnd_pulse = (uint8_t)(rnd_setting() == RND_PULSE);
    rnd_wozza = (uint8_t)(rnd_setting() == RND_WOZZA);
    rnd_halo = (uint8_t)(rnd_setting() == RND_HALO);
    rnd_mirror = (uint8_t)(rnd_setting() == RND_MIRROR);
    if (rnd_wozza || rnd_halo || rnd_mirror) rnd_nice = 1;   /* (WOZZA, HALO, MIRROR: NICE's gentle drift) */
    if (rnd_loops || rnd_orbit) rnd_nice = 1;       /* (LOOPS, ORBIT: NICE's gentle voices too) */
    rnd_roles(role, ambient && !rnd_loops && !rnd_orbit);
    if (rnd_loops || rnd_orbit)                     /* (LOOPS, ORBIT: four voices alike) */
        for (tr = 0; tr < NTRK; tr++) role[tr] = RND_ROLE_MELODY;
    if (rnd_glitch) rnd_roles_glitch(role);
    if (rnd_pulse) rnd_roles_pulse(role);
    if (rnd_wozza) rnd_roles_wozza(role);
    if (rnd_halo) rnd_roles_halo(role);
    if (rnd_mirror) rnd_roles_mirror(role);
    memcpy(rnd_role_of, role, sizeof rnd_role_of);
    undo_depth++;                                   /* (a roll is not four sound loads to undo one by one) */
    {                                               /* (the drone and the chords pick their engines first) */
        static const uint8_t order[] = {RND_ROLE_LOWPERC, RND_ROLE_DRONE, RND_ROLE_CHORDS, RND_ROLE_MELODY,
                                        RND_ROLE_GLITCH, RND_ROLE_NOISE, RND_ROLE_OSTI, RND_ROLE_TINT,
                                        RND_ROLE_CANON, RND_ROLE_FREE};   /* (PULSE:
                                                     * FM6 first) */
        uint32_t o;
        for (o = 0; o < NELEM(order); o++)
            for (tr = 0; tr < NTRK; tr++)
                if (role[tr] != order[o]) continue;
                else if (rnd_pulse) rnd_pulse_sound(tr, role[tr], &fm6s, &phys, &oracles);
                else if (rnd_wozza) rnd_wozza_sound(tr, role[tr], &fm6s, &phys, &oracles);
                else if (rnd_halo) rnd_halo_sound(tr, role[tr], &fm6s, &phys, &oracles);
                else if (rnd_mirror) rnd_mirror_sound(tr, role[tr], &fm6s, &phys, &oracles);
                else rnd_sound(tr, &fm6s, &phys, &oracles, role[tr]);
    }
    undo_depth--;
    undo_clear();
    if (rnd_glitch) rnd_glitch_registers(role);
    else if (rnd_pulse) rnd_pulse_registers(role);
    else if (rnd_wozza) rnd_wozza_registers(role);
    else if (rnd_halo) rnd_halo_registers(role);
    else if (rnd_mirror) rnd_mirror_registers(role);
    else rnd_registers();
    if (rnd_glitch) rnd_key_glitch();
    else if (rnd_pulse) rnd_key_pulse();
    else if (rnd_wozza) rnd_key_wozza();
    else if (rnd_halo) rnd_key_halo();
    else if (rnd_mirror) rnd_key_mirror();
    else if (rnd_loops || rnd_orbit) rnd_key_loops();
    else if (ambient) rnd_key_ambient();
    else rnd_key();
    if (rnd_glitch) {
        for (tr = 0; tr < TESSERA_TRACKS; tr++) {
            rnd_glitch_generator(tr, role[tr], &chords);
            rnd_cpu(tr);
        }
        rnd_glitch_links(role);
    } else if (rnd_pulse) {
        for (tr = 0; tr < TESSERA_TRACKS; tr++) {
            rnd_pulse_generator(tr, role[tr], &chords);
            rnd_cpu(tr);
        }
        rnd_pulse_links(role);
    } else if (rnd_wozza) {
        for (tr = 0; tr < TESSERA_TRACKS; tr++) {
            rnd_wozza_generator(tr, role[tr], &chords);
            rnd_cpu(tr);
        }
    } else if (rnd_halo || rnd_mirror) {
        if (rnd_halo) rnd_halo_generators(role);
        else rnd_mirror_generators(role);
        for (tr = 0; tr < TESSERA_TRACKS; tr++) rnd_cpu(tr);
    } else if (rnd_loops || rnd_orbit) {
        if (rnd_loops) rnd_loops_generators();
        else rnd_orbit_generators();
        for (tr = 0; tr < TESSERA_TRACKS; tr++) {
            rnd_loops_sound(tr);
            rnd_cpu(tr);
        }
    } else {
        for (tr = 0; tr < TESSERA_TRACKS; tr++) {
            rnd_role_generator(tr, role[tr], &chords);
            rnd_cpu(tr);
        }
    }
    rnd_tails();
    rnd_budget();
    if (rnd_halo || rnd_mirror) {
        rnd_fx_dry();
        if (rnd_halo) rnd_cords_halo(role);
        else rnd_cords_mirror(role);
    } else if (rnd_wozza) {                         /* (its own effects, then its cords: one on the TAPE) */
        if (RND_FX_ROLLS) rnd_fx_wozza(role);
        else rnd_fx_dry();
        rnd_cords_wozza(role);
    } else if (rnd_glitch || rnd_pulse) {           /* (the cords, then an effect in every slot, theirs too) */
        if (rnd_glitch) rnd_cords_glitch(role);
        else rnd_cords_pulse(role);
        if (RND_FX_ROLLS) rnd_fx_mode();
        else rnd_fx_dry();
    } else if (ambient) {
        rnd_cords_ambient();
        if (RND_FX_ROLLS) rnd_fx_mode();
        else rnd_fx_dry();
    } else {
        if (RND_FX_ROLLS) rnd_inserts(rnd_sends());
        else rnd_fx_dry();
        rnd_cords();
    }
    sync_reload = 1;
    rnd_say(rnd_mirror ? "MIRROR " : rnd_halo ? "HALO " : rnd_wozza ? "WOZZA " : rnd_pulse ? "PULSE " : rnd_glitch ? "GLITCH " : rnd_orbit ? "ORBIT " : rnd_loops ? "LOOPS " : rnd_fall ? "FALL " : rnd_nice ? "NICE " :
            ambient ? "AMB " : "RND ", rnd_seed);
    if (!song.playing && !seq_counting() && !chain_busy()) transport_req = 1;   /* (it plays at once) */
    rnd_guard_start();
    rnd_drift_start();
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
    if (rnd_backed) {
        rnd_guard_s.on = 0;                         /* (what was there before: not the guard's to change) */
        ui_message("BEFORE THE ROLL");
    } else {
        rnd_say(rnd_mirror ? "MIRROR " : rnd_halo ? "HALO " : rnd_wozza ? "WOZZA " : rnd_pulse ? "PULSE " : rnd_glitch ? "GLITCH " : rnd_orbit ? "ORBIT " : rnd_loops ? "LOOPS " : rnd_fall ? "FALL " : rnd_nice ? "NICE " :
                rnd_setting() == RND_AMBIENT ? "AMB " : "RND ", rnd_seed);
        rnd_guard_start();
    }
}
