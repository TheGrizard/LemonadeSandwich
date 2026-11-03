#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Thomas Bradley (tessera-rnd)
# tessera-rnd's Plaits (MACRO) on the host as the device builds it: Plaits' lean set and PLAITS_RND's five (waveshaper,
# grains, string machine, chiptune, noise) at half rate (22.05 kHz). Runs MACRO's own test (tests/macro_test.c: every
# model plays, pitch, HIT, knobs, no denormal tails, each model's cost and units) and RND's (tests/ui_test.c test_rnd:
# the PLAITS mode's rolls). Run from the repo root after ./build.sh (or with build/gen made); run_tests.sh runs it.
set -e
OUT=build/host/rnd_plaits
M=firmware/third_party/mi
PF="-DMELODEE_PLAITS=1 -DMELODEE_PLAITS_RND=1 -DMELODEE_PLAITS_HALF=1"
mkdir -p "$OUT"
objs=
for f in $M/plaits/dsp/*.cc $M/plaits/dsp/*/*.cc $M/plaits/resources.cc $M/stmlib/dsp/*.cc $M/stmlib/utils/*.cc \
    firmware/src/macro_glue.cc; do
    o=$OUT/$(echo "$f" | tr / _).o
    if [ ! -f "$o" ] || [ "$f" -nt "$o" ] || [ $M/plaits/dsp/voice.h -nt "$o" ]; then
        c++ -O2 -std=c++11 -fno-exceptions -fno-rtti -fno-threadsafe-statics -w -DMACRO_HOST -DTEST -DPLAITS_LEAN \
            -DPLAITS_RND -DPLAITS_SAMPLE_RATE=22050.0f -DPLAITS_SAMPLE_RATE_EXACT -DPLAITS_BLOCK_SIZE=16 \
            -include stdio.h -I$M -Ifirmware/src -c "$f" -o "$o"
    fi
    objs="$objs $o"
done
cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src $PF -c tests/macro_test.c -o "$OUT/macro_test.o"
c++ -o "$OUT/macro_test" "$OUT/macro_test.o" $objs -lm
"$OUT/macro_test"
cc -O1 -ffp-contract=off -w -DMELODEE_USB_AUDIO=1 -DMELODEE_LOOK=0 $PF -Ibuild/gen -Ifirmware/src \
    -c tests/ui_test.c -o "$OUT/ui_test.o"
c++ -o "$OUT/ui_test" "$OUT/ui_test.o" $objs -lm
UI_ONLY_RND=1 "$OUT/ui_test"
