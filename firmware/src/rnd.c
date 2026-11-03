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
/* RND_OFF RECORD (REC as upstream), RND_FREE RND, RND_AMBIENT AMBIENT (the roles: rnd_roll). Saved in
 * favorites.factory[15][24] bits 6 (on) and 7 (AMBIENT) (free: settings_test.c), as the panel gamma's bits are, so it
 * comes back at power-on with the other device settings */
#define RND_SET_BIT 0x40u
#define RND_AMB_BIT 0x80u
enum { RND_OFF, RND_FREE, RND_AMBIENT, RND_MODES };
static uint32_t rnd_mode(void) { return (favorites.factory[15][24] & RND_SET_BIT) != 0u; }
static uint32_t rnd_setting(void)
{
    uint8_t b = favorites.factory[15][24];
    return !(b & RND_SET_BIT) ? RND_OFF : (b & RND_AMB_BIT) ? RND_AMBIENT : RND_FREE;
}
static void rnd_mode_put(uint32_t m)
{
    uint8_t *b = &favorites.factory[15][24];
    uint8_t v = (uint8_t)((*b & ~(RND_SET_BIT | RND_AMB_BIT)) | (m != RND_OFF ? RND_SET_BIT : 0u) |
                          (m == RND_AMBIENT ? RND_AMB_BIT : 0u));
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
static void rnd_tp(uint32_t tr, uint32_t id, int32_t v) { pid_set(PID_TRACK(id), tr, v); }
static void rnd_tx(uint32_t tr, uint32_t id, int32_t v) { pid_set(PID_TESS(id), tr, v); }

/* the engines a roll may pick (built, offered, never DRUM): FM6, ORACLE and PHYS once each at most, and two of them
 * at most a roll (RND_HEAVY): each costs about twice TRIO, and on the FM-1 the rolls with all three were the ones past
 * 80 % CPU (host profile, 80 rolls: all of the heaviest held the three; the lightest two TRIO tracks) */
#define RND_HEAVY 2u
/* the roles of AMBIENT's four tracks (RND: RND_ROLE_FREE on each) */
enum { RND_ROLE_FREE, RND_ROLE_DRONE, RND_ROLE_CHORDS, RND_ROLE_MELODY };
/* an engine's weight for a role (FM6 ORACLE PHYS TRIO; 2 for any other): a drone noisy (ORACLE's noise, TRIO's noise
 * waves, PHYS bowed), chords a pad, a melody struck */
static uint32_t rnd_engine_weight(uint32_t e, uint32_t role)
{
    static const uint8_t W[4][4] = {{3, 3, 2, 2}, {2, 4, 3, 3}, {4, 3, 1, 3}, {3, 2, 3, 3}};
    uint32_t i = e == ENGI_FM6 ? 0u : e == ENGI_ORACLE ? 1u : e == ENGI_PHYS ? 2u : e == 6u ? 3u : 4u;
    return i < 4u ? W[role & 3u][i] : 2u;
}
static uint32_t rnd_engine(uint32_t *fm6s, uint32_t *phys, uint32_t *oracles, uint32_t role)
{
    uint8_t code[NENG_SHOWN], w[NENG_SHOWN];
    uint32_t n = 0, k, full = *fm6s + *phys + *oracles >= RND_HEAVY;
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
    uint32_t e = rnd_engine(fm6s, phys, oracles, role), wild = (uint32_t)rnd_rr(40, 100), k;
    const engine_t *en = ENGINES[e];
    engine_load_of(t, e, (uint32_t)rnd_rr(0, (int32_t)en->npresets - 1));
    for (k = 0; k < 8u; k++) {                      /* the engine's eight, pushed from the preset's */
        const param_desc_t *d = pid_desc(PID_TRACK(P_E0 + k), tr);
        if (!d || d->max <= d->min) continue;
        int32_t lo = d->min, hi = d->max;
        if (e == ENGI_ORACLE && k >= 6u) lo = -20;  /* (LVL A, LVL B: not both shut) */
        rnd_tp(tr, P_E0 + k, d->fmt == F_ENUM ? (rnd_pct(wild) ? rnd_rr(lo, hi) : t->p[P_E0 + k])
                                          : rnd_push(t->p[P_E0 + k], lo, hi, wild));
    }
    if (e == ENGI_ORACLE) rnd_oracle(tr, wild * 2u / 3u);
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
            p[OR_NOISE] = (uint8_t)rnd_rr(50, 127);
            p[OR_ATTACK_AMP] = (uint8_t)rnd_rr(60, 127);
            p[OR_SUSTAIN_AMP] = (uint8_t)rnd_rr(100, 127);
            p[OR_RELEASE_AMP] = (uint8_t)rnd_rr(90, 127);
            p[OR_RELEASE_ON] = 1;
        } else if (e == 6u) {
            rnd_tp(tr, P_E0, (int32_t)rnd_pick(tw, tww, NELEM(tw)));   /* (TRIO's WAVE) */
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
    rnd_tp(tr, P_LD_AMP, 0);
}

/* ------------------------------------------------------------ the roll: effects within a CPU budget --- */
/* What each effect costs, in points (measured on the host, tests/ui_test.c test_rnd: four tracks rolled, one insert of
 * a kind on each, or one send effect; a point is about a twentieth of what a roll's engines cost). On the FM-1 a roll's
 * effects had cost as much again as its engines (80 to 105 % CPU, the overload: late blocks are a digital crunch and the
 * voices it sheds a stutter), so a roll's effects share RND_FX_POINTS. Inserts: FX 1 only, on two tracks at most. */
static uint32_t rnd_fx_points(uint32_t type, int send)
{
    switch (type) {
    case FX_OFF: return 0u;
    case FX_ECHO: case FX_FILTER: return 1u;
    case FX_ROOM: return 3u;
    case FX_SPIT: case FX_TAPE: case FX_CHORUS: return 4u;
    case FX_PHASER: return 5u;
    case FX_WIDTH: return 7u;
    case FX_FORMANT: return 9u;
    case FX_REVERB: return send ? 11u : 14u;
    default: return 6u;
    }
}
#define RND_FX_POINTS 18u
#define RND_INSERT_TRACKS 2u

/* the send effects (SPLIT: a send to each): a space (REVERB, or the cheaper ROOM), now and then an echo; the third
 * off. -> the points they use */
static uint32_t rnd_sends(void)
{
    uint32_t used = 0, s;
    uint32_t type[3];
    type[0] = rnd_pct(60) ? FX_REVERB : FX_ROOM;
    type[1] = rnd_pct(55) ? FX_ECHO : FX_OFF;
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

/* the inserts: FX 1 on RND_INSERT_TRACKS tracks at most, each kind if the points left allow it; FX 2 and 3 off. SPIT
 * (its drive: a hard digital edge) rare */
static void rnd_inserts(uint32_t used)
{
    static const uint8_t fx[] = {FX_FILTER, FX_ECHO, FX_TAPE, FX_CHORUS, FX_PHASER, FX_SPIT, FX_WIDTH, FX_FORMANT};
    static const uint8_t fw[] = {18, 12, 16, 12, 10, 3, 4, 6};
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
        rnd_tp(t, P_FXSEND2, rnd_rr(0, 100));
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
    if (e == ENGI_FM6 && rnd_speed((uint32_t)t->p[P_SDIV]) == RND_FAST)   /* (FM6 is the heaviest: 1/8 at most) */
        rnd_tp(tr, P_SDIV, rnd_pct(50) ? 1 : 0);
    sp = rnd_speed((uint32_t)t->p[P_SDIV]);
    if (sp != RND_SLOW) {
        rnd_tail_cap(tr, sp == RND_FAST ? 55 : 85);
        rnd_cap(tr, P_SGATE, sp == RND_FAST ? 90 : 127);
        rnd_tx(tr, TX_SUB_RATCH, 0);                /* (ratchets only on a slow track) */
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
        rnd_tp(tr, P_VOICE, rnd_pct(50) ? V_MONO : V_LEGATO);   /* (a line on one voice: nothing overlaps) */
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
    static const uint8_t drop_w[] = {2, 1, 1, 1, 5};
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
    int32_t top = d ? d->max : 14, scale = rnd_pct(80) ? (int32_t)rnd_pick(sc, sw, NELEM(sc)) : rnd_rr(16, top);
    int32_t root = rnd_rr(0, 11);
    uint32_t tr;
    rnd_tx(0, TX_KEY_LINK, 1);
    for (tr = 0; tr < NTRK; tr++) {
        rnd_tp(tr, P_SCALE, scale);
        rnd_tp(tr, P_ROOT, root);
        rnd_tp(tr, P_QUANT, Q_CHRD);
    }
    scale_share(&trk[0]);
}
/* AMBIENT: no effects of its own (the sound goes out to the outboard's): every insert, send and send effect off */
static void rnd_fx_off(void)
{
    uint32_t tr, s;
    for (tr = 0; tr < NTRK; tr++) {
        for (s = 0; s < 3u; s++) rnd_tp(tr, P_FXTYPE + s * 4u, FX_OFF);
        rnd_tp(tr, P_FXSEND, 0);
        rnd_tp(tr, P_FXSEND2, 0);
        rnd_tp(tr, P_FXSEND3, 0);
    }
    for (s = 0; s < 3u; s++) pid_set(PID_GLOBAL(G_MFXTYPE + s * 4u), 0, FX_OFF);
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
    uint32_t i, n = (uint32_t)rnd_rr(2, 5);
    for (i = 0; i < CORD_LIVE; i++) {
        cord_t c;
        memset(&c, 0, sizeof c);
        if (i < n) {
            c.src = (uint8_t)rnd_pick(src, sw, NELEM(src));
            c.src_trk = (uint8_t)rnd_rr(0, NTRK - 1);
            c.dst_trk = (uint8_t)(rnd_pct(60) ? c.src_trk : (uint32_t)rnd_rr(0, NTRK - 1));
            c.dst = (uint8_t)rnd_pick(dst, dw, NELEM(dst));
            if (c.dst == 0xFFu) c.dst = (uint8_t)(c.dst_trk >= 2u ? rnd_gen_dest(c.dst_trk) : CD_CUTOFF);
            c.amount = (int8_t)(rnd_pct(50) ? rnd_rr(10, 50) : -rnd_rr(10, 50));
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
#define RND_GUARD_HOT 184u                          /* 72 % */
#define RND_GUARD_PEAK 218u                         /* 85 % */
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

static void rnd_roll(void)
{
    uint32_t tr, fm6s = 0, phys = 0, oracles = 0, chords = 0, ambient = rnd_setting() == RND_AMBIENT;
    uint8_t role[NTRK];
    momentary_restore();
    rnd_seed = rnd_next() ^ fm1_ticks() ^ (fm1_ms << 7);
    if (!rnd_seed) rnd_seed = 0x2545F491u;
    rnd_state = rnd_seed;
    rnd_take();                                     /* (the hold swaps back to this) */
    rnd_backed = 0;
    rnd_roles(role, ambient);
    undo_depth++;                                   /* (a roll is not four sound loads to undo one by one) */
    {                                               /* (the drone and the chords pick their engines first) */
        static const uint8_t order[] = {RND_ROLE_DRONE, RND_ROLE_CHORDS, RND_ROLE_MELODY, RND_ROLE_FREE};
        uint32_t o;
        for (o = 0; o < NELEM(order); o++)
            for (tr = 0; tr < NTRK; tr++)
                if (role[tr] == order[o]) rnd_sound(tr, &fm6s, &phys, &oracles, role[tr]);
    }
    undo_depth--;
    undo_clear();
    rnd_registers();
    if (ambient) rnd_key_ambient();
    else rnd_key();
    for (tr = 0; tr < TESSERA_TRACKS; tr++) {
        rnd_role_generator(tr, role[tr], &chords);
        rnd_cpu(tr);
    }
    rnd_tails();
    rnd_budget();
    if (ambient) {
        rnd_fx_off();
        rnd_cords_ambient();
    } else {
        rnd_inserts(rnd_sends());
        rnd_cords();
    }
    sync_reload = 1;
    rnd_say(ambient ? "AMB " : "RND ", rnd_seed);
    if (!song.playing && !seq_counting() && !chain_busy()) transport_req = 1;   /* (it plays at once) */
    rnd_guard_start();
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
        rnd_say(rnd_setting() == RND_AMBIENT ? "AMB " : "RND ", rnd_seed);
        rnd_guard_start();
    }
}
