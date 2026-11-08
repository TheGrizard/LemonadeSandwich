/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Theo Dainis (melodee-tess) */
/* MACRO with Braids (eng_braids.c, LEMONADE SANDWICH): built with -DMELODEE_BRAIDS=1 and linked with Braids compiled
 * for this computer (run_tests.sh braids_tests, c++). Every model plays, finite and within full scale; a held CSAW at
 * its pitch (44.1 kHz: tools/braids_44k.py); four voices a part, a chord; each setting reaches the sound; its memory
 * from the audio arena; the cost against Plaits' VA (4 units) and SID's on this computer (a report). */
#if !MELODEE_BRAIDS
#error "braids_test: build it with -DMELODEE_BRAIDS=1"
#endif
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <time.h>

static int bad;
static void check(const char *what, int ok)
{
    printf("braids: %-75s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static double now_s(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec * 1e-9; }

/* track 0 alone (the others silent): seconds of it, note held for hold of them; peak, rms, zero crossings a second
 * in the held part, the rms of its last quarter; 0: not finite / past full scale */
static int play(uint32_t note, double secs, double hold, double *peak, double *rms, double *zc, double *tail)
{
    int32_t o[2 * CTL];
    uint32_t f, i, n = (uint32_t)(secs * FS), nh = (uint32_t)(hold * FS), cross = 0, ncount = 0;
    double sum = 0, tsum = 0, last = 0;
    uint32_t tn = 0;
    *peak = 0;
    trk_note_on(&trk[0], note, 100);
    for (f = 0; f < n; f += CTL) {
        if (f >= nh && f < nh + CTL) trk_note_off(&trk[0], note);
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            double x = o[2 * i];
            if (x != x || x > 2147483647.0 || x < -2147483647.0) return 0;
            if (x > *peak) *peak = x;
            if (-x > *peak) *peak = -x;
            sum += x * x;
            if (f > FS / 4u && f + i < nh) {          /* (after the start, while held) */
                if ((x >= 0) != (last >= 0)) cross++;
                ncount++;
            }
            if (f + i >= n - n / 4u) { tsum += x * x; tn++; }
            last = x;
        }
    }
    trk_note_off(&trk[0], note);
    *rms = sqrt(sum / n);
    *zc = ncount ? cross * (double)FS / ncount / 2.0 : 0;
    *tail = tn ? sqrt(tsum / tn) : 0;
    return 1;
}

static void sound(uint32_t preset)
{
    uint32_t k;
    host_tracks_init();
    for (k = 1; k < NPART; k++) trk[k].p[P_LEVEL] = 0;
    host_preset(&trk[0], ENGI_MACRO, preset);
    trk[0].engine = trk[0].eng_req;
}

int main(void)
{
    uint32_t k, ok = 1, models;
    double peak, rms, zc, tail, base;
    check("MACRO offered, at engine 21, with Braids' presets", eng_ok(ENGI_MACRO) && ENGINES[ENGI_MACRO]->npresets == 19u);
    check("its memory: four oscillators a part (no delay lines: BRAIDS_LEAN), from the audio arena",
          bg_size() < 1024u && mc_bytes() < 4096u && ENGINES[ENGI_MACRO]->poly == 4u);
    models = (uint32_t)ENGINES[ENGI_MACRO]->edit[0].max + 1u;
    for (k = 0, ok = 1; k < models; k++) {            /* every model, from INIT CSAW's settings */
        sound(0);
        trk[0].p[P_E0] = (int16_t)k;
        if (!play(48, 1.0, 0.5, &peak, &rms, &zc, &tail) || rms < 50.0 || peak > 0.95 * 2147483647.0) {
            ok = 0;
            printf("    model %u (%s): peak %.0f rms %.0f\n", k, ENGINES[ENGI_MACRO]->edit[0].names[k], peak, rms);
        }
    }
    check("every model plays (19: textures, voices, FM, wavetables, CSAW FOLD SWARM TOY): sound, finite, in range",
          ok && models == 19u);
    for (k = 0, ok = 1; k < NELEM(MC_PRESETS); k++) {
        sound(k);
        if (!play(60, 0.8, 0.5, &peak, &rms, &zc, &tail) || rms < 50.0 || peak > 0.95 * 2147483647.0) {
            ok = 0;
            printf("    preset %s: peak %.0f rms %.0f\n", MC_PRESETS[k].name, peak, rms);
        }
    }
    check("every preset plays: sound, finite, in range", ok);

    sound(0);                                         /* INIT CSAW: a saw held, at its pitch */
    ok = play(57, 1.5, 1.4, &peak, &rms, &zc, &tail);
    printf("    INIT CSAW, A3 held: %.1f crossings a second (220 Hz wanted)\n", zc);
    check("a held CSAW at its pitch (A3: 220 Hz within 3 %: the tables at 44.1 kHz)", ok && zc > 213.4 && zc < 226.6);
    sound(0);
    ok = play(69, 1.5, 1.4, &peak, &rms, &zc, &tail);
    check("...an octave up (A4: 440 Hz within 3 %)", ok && zc > 426.8 && zc < 453.2);
    {                                                 /* four voices: a chord sounds louder than one note */
        double one, four;
        int32_t o[2 * CTL];
        uint32_t f, i;
        double sum = 0;
        sound(2);
        play(48, 0.6, 0.6, &peak, &one, &zc, &tail);
        sound(2);
        for (k = 0; k < 4u; k++) trk_note_on(&trk[0], 48u + 7u * k, 100);
        for (f = 0; f < (uint32_t)(0.6 * FS); f += CTL) {
            mix_block(o, CTL);
            for (i = 0; i < CTL; i++) sum += (double)o[2 * i] * o[2 * i];
        }
        four = sqrt(sum / (0.6 * FS));
        check("four voices a part: a chord of four (SAW SWARM) louder than one note", four > 1.4 * one);
    }

    sound(0);                                         /* the settings reach the sound: each moved, the sound differs */
    play(60, 0.5, 0.45, &peak, &base, &zc, &tail);
    for (k = 0, ok = 1; k < 8u; k++) {
        double r1, r2;
        if (k == 6u) continue;                        /* (STRIK: the struck models) */
        sound(k == 1u || k == 2u ? 10 : 0);           /* (TIMBR and COLOR on WIND: a filter's frequency and its Q) */
        trk[0].p[P_E0 + k] = (int16_t)(k == 0u ? 7 : k == 3u ? 20 : k == 4u ? 6 : k == 5u ? 6 : 10);
        play(60, 0.5, 0.45, &peak, &r1, &zc, &tail);
        sound(k == 1u || k == 2u ? 10 : 0);
        trk[0].p[P_E0 + k] = (int16_t)(k == 0u ? 10 : k == 3u ? 127 : k == 4u ? 0 : k == 5u ? 0 : 120);
        play(60, 0.5, 0.45, &peak, &r2, &zc, &tail);
        if (fabs(r1 - r2) < 1e-6 * (r1 + r2)) { ok = 0; printf("    E%u: no change\n", k + 1u); }
    }
    check("MODEL, TIMBR, COLOR, TONE, BITS, RATE, DRIVE: each moves the sound", ok);
    {                                                 /* the cost, one note, on this computer: Braids' worst models */
        double t0, br, sid, idle;
        int32_t o[2 * CTL];
        uint32_t f;
        host_tracks_init();
        for (k = 0; k < NPART; k++) trk[k].p[P_LEVEL] = 0;
        t0 = now_s();
        for (f = 0; f < 4u * FS; f += CTL) mix_block(o, CTL);
        idle = now_s() - t0;
        sound(8);                                     /* (FB GROWL: FBFM, of the dearest) */
        trk_note_on(&trk[0], 60, 100);
        t0 = now_s();
        for (f = 0; f < 4u * FS; f += CTL) mix_block(o, CTL);
        br = now_s() - t0 - idle;
        host_tracks_init();
        host_preset(&trk[0], ENGI_SID, 0);
        trk[0].engine = trk[0].eng_req;
        trk_note_on(&trk[0], 60, 100);
        t0 = now_s();
        for (f = 0; f < 4u * FS; f += CTL) mix_block(o, CTL);
        sid = now_s() - t0 - idle;
        printf("braids: cost on this computer, one note: MACRO (FBFM) %.0f ns a sample, SID %.0f (%.1f x; a report)\n",
               br * 1e9 / (4.0 * FS), sid * 1e9 / (4.0 * FS), sid > 0 ? br / sid : 0);
    }
    printf(bad ? "braids: %d FAILED\n" : "braids: all passed\n", bad);
    return bad != 0;
}
