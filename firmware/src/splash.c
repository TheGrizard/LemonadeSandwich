/* SPDX-License-Identifier: GPL-3.0-only */
/* The boot screen (tessera-rnd: LEMONADE SANDWICH): a slice of lemon sandwiched between two slices of bread, in
 * colour (splash_art.h: tools/gen_splash_art.py), over the name, on two lines (SPL_NAME), and a line.
 * While the firmware starts, the picture's shadow shows (in the grout's colour, T_LINE); when it is ready,
 * splash_lay() puts the lemon in, then the bread drops onto it and settles, in the wait the boot already makes
 * before the first frame, then holds the picture a moment (SPL_HOLD_MS; a button or key pressed ends it sooner).
 * The picture's colours are fixed (it is the logo), the rest the palette's. Drawn with lcd_fill and draw_text_box
 * only. */

#include "splash_art.h"
#define SPL_CX 120
#define SPL_CY 66                               /* the picture's centre */
#define SPL_AX (SPL_CX - SPL_ART_W)             /* its top left, px (cells of 2) */
#define SPL_AY (SPL_CY - SPL_ART_H)
static const char *const SPL_NAME[2] = {"LEMONADE", "SANDWICH"};   /* (AF_L: Mono's font pixels of 4, 8 letters at most) */
#define SPL_NAME_Y 108                          /* the name's first box (AF_L; the next SPL_NAME_DY under it) */
#define SPL_NAME_DY 34
#define SPL_LINE_Y 184                          /* the line under it (AF_S) */
#define SPL_STEP_MS 60u
#define SPL_HOLD_MS 800u                        /* the finished picture held (a press ends it sooner) */
#ifndef SPL_FRAME_HOOK
#define SPL_FRAME_HOOK(f) ((void)0)             /* (tests: each frame of the outro) */
#endif
/* the crust, the crumb; the lemon's rind, pith and flesh (splash_art.h's colours 1 to 5) */
static const uint16_t SPL_PAL[6] = {0, RGB(170, 104, 44), RGB(246, 224, 172), RGB(236, 186, 24), RGB(252, 246, 206),
                                    RGB(252, 214, 64)};

/* the picture dy px down: how 0 its shadow (T_LINE), 1 the lemon alone, 2 all of it, the background too (whatever
 * was in its box before, gone) */
static void spl_art(uint32_t how, int32_t dy)
{
    const uint8_t *r = SPL_ART;
    uint32_t x, y, n, c;
    for (y = 0; y < SPL_ART_H; y++)
        for (x = 0; x < SPL_ART_W; x += n, r++) {
            c = *r >> 5;
            n = (*r & 31u) + 1u;
            if (how == 2u || (c && (how == 0u || c >= 3u)))
                lcd_fill(SPL_AX + 2u * x, (uint32_t)(SPL_AY + dy) + 2u * y, 2u * n, 2u,
                         !how ? T_LINE : c ? SPL_PAL[c] : T_BG);
        }
}

#if MELODEE_LOOK
static const char *spl_line;                    /* splash()'s line (the outro sweeps it too) */
#endif
static void splash(const char *line)          /* the boot screen: the picture's shadow, the name, then a line */
{
    uint32_t i;
#if MELODEE_LOOK
    spl_line = line;
#endif
    lcd_fill(0, 0, 240, 240, T_BG);
    spl_art(0, 0);
    for (i = 0; i < 2u; i++)
        draw_text_box(0, SPL_NAME_Y + i * SPL_NAME_DY, 240, &AF_L, SPL_NAME[i], T_TEXT, 1);
    draw_text_box(0, SPL_LINE_Y, 240, &AF_S, line, T_MID, 1);
}

static void spl_wait(uint32_t ms)              /* (the boot's wait, the watchdog fed) */
{
    while (ms--) {
        fm1_delay_ms(1);
        if (!(ms & 31u)) fm1_wdt_feed();
    }
}

/* the finished mark held ms, the watchdog fed; a button or key pressed since (not one held from power-on) ends it
 * (returns 1) */
static int spl_hold(uint32_t ms)
{
    uint32_t held = fm1_in.buttons | fm1_in.notes;
    while (ms--) {
        fm1_delay_ms(1);
        if (!(ms & 31u)) fm1_wdt_feed();
        if ((fm1_in.buttons | fm1_in.notes) & ~held) return 1;
    }
    return 0;
}

#if MELODEE_LOOK
/* The Mono outro: the held mark breaks into grains of 2 px (a cell of the mark's grid is one when half of it is
 * ink), they shiver loose, then a wind from the left sweeps them off, column by column, each grain speeding up,
 * drifting and dimming to a pixel. Nothing kept per grain: frame f is drawn from the mark itself (spl_ink), in the
 * band the mark covers, through the canvas (the diff sends what changed: the SPI sets the pace, ~40 ms a frame). */
#define SPL_Y0 24                               /* the band: rows SPL_Y0 .. SPL_Y0 + 124 + SPL_H1 - 1 */
#define SPL_H1 52u                              /* (its foot, on the small canvas: the name's second line, the line) */
#define SPL_X0 24                               /* (its columns: SPL_X0 .. 240 - SPL_X0) */
#define SPL_LOOSE 2                             /* frames the grains shiver before the wind */
#define SPL_FRONT 20                            /* the wind's front, px a frame */
#define SPL_LIFE 8                              /* frames a grain flies */
#define SPL_FRAME_MS 30u                        /* (at least) */
static uint16_t spl_ink(int32_t x, int32_t y)   /* the mark's colour at x, y (0: none) */
{
    uint32_t i, n;
    int32_t t = x - SPL_AX, u = y - SPL_AY;
    if (t >= 0 && u >= 0 && t < 2 * SPL_ART_W && u < 2 * SPL_ART_H) {   /* the picture: its row's runs, to the cell */
        const uint8_t *r = SPL_ART;
        for (i = 0; i < (uint32_t)u / 2u * SPL_ART_W + (uint32_t)t / 2u; r++)   /* (rows fill: SPL_ART_W cells each) */
            if ((i += (*r & 31u) + 1u) > (uint32_t)u / 2u * SPL_ART_W + (uint32_t)t / 2u)
                break;
        return SPL_PAL[*r >> 5];
    }
    for (i = 0; i < 2u; i++) {                  /* the name (AF_L: font pixels of 4, each line centred in its box) */
        int32_t y0 = SPL_NAME_Y + (int32_t)i * SPL_NAME_DY + 4;
        n = str_len(SPL_NAME[i]);
        t = x - (240 - 24 * (int32_t)n) / 2;
        if (y >= y0 && y < y0 + 28 && t >= 0 && t < 24 * (int32_t)n && t % 24 < 20)
            return MO_FONT[mono_ch((uint8_t)SPL_NAME[i][t / 24]) - 32u][(y - y0) / 4] & (16u >> (t % 24 / 4)) ? T_TEXT : 0;
    }
    n = str_len(spl_line);                      /* the line (AF_S: of 2) */
    t = x - (240 - 12 * (int32_t)n) / 2;
    if (y >= SPL_LINE_Y && y < SPL_LINE_Y + 14 && t >= 0 && t < 12 * (int32_t)n && t % 12 < 10)
        return MO_FONT[mono_ch((uint8_t)spl_line[t / 12]) - 32u][(y - SPL_LINE_Y) / 2] & (16u >> (t % 12 / 2)) ? T_MID : 0;
    return 0;
}
static uint32_t spl_frame(int32_t f)            /* frame f of the outro, band by band; the grains still flying */
{
    uint32_t part, left = 0;
    for (part = 0; part < 2u; part++) {
        int32_t oy = SPL_Y0 + (part ? 124 : 0), cx, cy;
        cv_begin(240, part ? SPL_H1 : 124u, T_BG);
        for (cy = SPL_Y0; cy < SPL_Y0 + 124 + (int32_t)SPL_H1; cy += 2)
            for (cx = SPL_X0; cx < 240 - SPL_X0; cx += 2) {
                uint32_t h = ((uint32_t)cx * 73856093u ^ (uint32_t)cy * 19349663u) * 2654435761u;
                int32_t x = cx, y = cy, s = 2, a = f - SPL_LOOSE - (cx - SPL_X0) / SPL_FRONT - (int32_t)(h >> 30);
                uint16_t c = spl_ink(cx, cy), c1 = spl_ink(cx + 1, cy + 1);
                if (!c && !c1)
                    continue;
                c = c ? c : c1;
                x += ((int32_t)(h >> 4 & 3u) - 1) * (f > 0) * (1 + (f > 1));   /* loose: a pixel or two off */
                y += ((int32_t)(h >> 6 & 3u) - 1) * (f > 1);
                if (a >= 0) {                   /* swept: faster each frame, drifting, dimming, smaller */
                    x += a * (a + 3) * (int32_t)(4u + (h >> 8 & 7u)) / 4;
                    y += ((int32_t)(h >> 12 & 7u) - 4) * a * (a + 1) / 6;
                    if (a >= 3)
                        c = T_MID;
                    if (a >= 5)
                        s = 1;
                    if (a >= SPL_LIFE - (int32_t)(h >> 16 & 3u))
                        continue;
                }
                if (x < 240 && y + s > oy && y < oy + (int32_t)cv_h) {
                    cv_rect(x, y - oy, s, s, c);
                    left++;
                }
            }
        cv_blit(0, (uint32_t)oy);
    }
    return left;
}
static void spl_outro(void)
{
    uint32_t f, n, t0 = fm1_ms, held = fm1_in.buttons | fm1_in.notes;
    for (f = 1; f < 40u; f++) {                 /* (until the last grain is gone: ~20 frames) */
        if (!spl_frame((int32_t)f))
            break;
        SPL_FRAME_HOOK(f);
        for (n = 0; fm1_ms - t0 < (f + 1u) * SPL_FRAME_MS && n < SPL_FRAME_MS; n++)
            spl_wait(1);
        if ((fm1_in.buttons | fm1_in.notes) & ~held)
            break;
    }
    df_damage(0, 0, 240, 240, DF_N);            /* (main clears the screen past the diff's places) */
}
#endif

/* the picture made, then at least `ms` gone in all (the boot's own wait); anim 0: the wait alone */
static void splash_lay(uint32_t ms, int anim)
{
    static const int8_t DROP[] = {-10, -4, 1, 0};   /* the bread falling onto the lemon, a bounce, settled */
    uint32_t k, used = 0;
    int32_t was = 0, dy;
    if (anim) {
        spl_art(1, 0);                          /* the lemon */
        spl_wait(4u * SPL_STEP_MS);
        for (k = 0; k < NELEM(DROP); k++) {
            dy = DROP[k];
            if (dy > was)                       /* (the rows the last draw covered and this one does not: the
                                                 * shadow's, first) */
                lcd_fill(SPL_AX, (uint32_t)(SPL_AY + was), 2u * SPL_ART_W, (uint32_t)(dy - was), T_BG);
            else if (dy < was)
                lcd_fill(SPL_AX, (uint32_t)(SPL_AY + dy + 2 * SPL_ART_H), 2u * SPL_ART_W, (uint32_t)(was - dy), T_BG);
            spl_art(2, dy);
            was = dy;
            spl_wait(SPL_STEP_MS);
        }
        used = (4u + NELEM(DROP)) * SPL_STEP_MS;
        if (!spl_hold(SPL_HOLD_MS))
            LOOK((void)0, spl_outro());
    }
    if (used < ms)
        spl_wait(ms - used);
}
