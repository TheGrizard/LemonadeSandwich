#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Thomas Bradley (tessera-rnd)
# The Plaits build of tessera-rnd on the host: Plaits whole (no PLAITS_LEAN) at half rate (22.05 kHz), the other
# instrument engines left out, as .github/workflows/rnd-plaits.yml builds the device's. Runs MACRO's own test
# (tests/macro_test.c: every model plays, at its pitch, HIT, knobs, no denormal tails) and RND's (tests/ui_test.c
# test_rnd: rolls, the swap, Plaits' units within the budget, the guard). Run from the repo root after ./build.sh
# (or with build/gen made).
set -e
OUT=build/host/plaits
M=firmware/third_party/mi
PF="-DMELODEE_FM6=0 -DMELODEE_ORACLE=0 -DMELODEE_TRIO=0 -DMELODEE_PHYS=0 -DMELODEE_SID=0 -DMELODEE_SID_KIT=0
    -DMELODEE_PLAITS=1 -DMELODEE_PLAITS_FULL=1 -DMELODEE_PLAITS_HALF=1"
mkdir -p "$OUT"
objs=
for f in $M/plaits/dsp/*.cc $M/plaits/dsp/*/*.cc $M/plaits/resources.cc $M/stmlib/dsp/*.cc $M/stmlib/utils/*.cc \
    firmware/src/macro_glue.cc; do
    o=$OUT/$(echo "$f" | tr / _).o
    c++ -O2 -std=c++11 -fno-exceptions -fno-rtti -fno-threadsafe-statics -w -DMACRO_HOST -DTEST \
        -DPLAITS_SAMPLE_RATE=22050.0f -DPLAITS_SAMPLE_RATE_EXACT -DPLAITS_BLOCK_SIZE=16 -include stdio.h \
        -I$M -Ifirmware/src -c "$f" -o "$o"
    objs="$objs $o"
done
cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src $PF -c tests/macro_test.c -o "$OUT/macro_test.o"
c++ -o "$OUT/macro_test" "$OUT/macro_test.o" $objs -lm
"$OUT/macro_test"
cc -O1 -ffp-contract=off -w -DMELODEE_USB_AUDIO=1 -DMELODEE_LOOK=1 $PF -Ibuild/gen -Ifirmware/src \
    -c tests/ui_test.c -o "$OUT/ui_test.o"
c++ -o "$OUT/ui_test" "$OUT/ui_test.o" $objs -lm
UI_ONLY_RND=1 "$OUT/ui_test"
