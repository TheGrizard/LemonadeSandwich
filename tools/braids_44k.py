#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Braids at the FM-1's 44.1 kHz (firmware/third_party/mi/braids, MIT; written for 96 kHz): its tables that depend on
the sample rate made again, in resources.cc, with the formulas of Braids' own resources/lookup_tables.py:

  lut_oscillator_increments, lut_oscillator_delays   the pitch (every model's phase increments)
  lut_svf_cutoff, lut_svf_damp, lut_svf_scale        its state-variable filters (noise, VOWEL FOF, the Z filters)
  lut_granular_envelope_rate                         GRANULAR CLOUD's grains as long as at 96 kHz

  python3 tools/braids_44k.py            write them
  python3 tools/braids_44k.py --check    fail if resources.cc differs from what this writes (tests/run_tests.sh)"""
import re
import sys
from pathlib import Path

import numpy

RES = Path(__file__).resolve().parent.parent / "firmware/third_party/mi/braids/resources.cc"
SR, SR_BRAIDS = 44100.0, 96000.0


def tables():
    t = {}
    excursion = 65536 * 65536.0
    notes = numpy.arange(128 * 128.0, (128 + 12) * 128.0 + 16, 16)
    pitches = 440.0 * 2 ** ((notes - 69 * 128) / (128 * 12))
    t["lut_oscillator_increments"] = (excursion / SR * pitches).astype(int)
    t["lut_oscillator_delays"] = (SR / pitches * 65536 * 4096).astype(int)
    cutoff = 440.0 * 2 ** ((numpy.arange(0, 257) - 69) / 12.0)
    f = cutoff / SR
    f[f > 1 / 8.0] = 1 / 8.0
    f = 2 * numpy.sin(numpy.pi * f)
    resonance = numpy.arange(0, 257) / 260.0
    damp = numpy.minimum(2 * (1 - resonance ** 0.25), numpy.minimum(2, 2 / f - f * 0.5))
    t["lut_svf_cutoff"] = (f * 32767.0).astype(int)
    t["lut_svf_damp"] = (damp * 32767.0).astype(int)
    t["lut_svf_scale"] = (((damp / 2) ** 0.5) * 32767.0).astype(int)
    rate = 2 ** (numpy.arange(0, 257) / 64.0) * (1 << 14) / 8 * (SR_BRAIDS / SR)
    t["lut_granular_envelope_rate"] = numpy.minimum(rate, 65535).astype(int)
    return t


def write(src):
    for name, v in tables().items():
        m = re.search(r"(const (?:uint16_t|uint32_t) %s\[\] = \{\n)(.*?)(\n\};)" % name, src, re.S)
        assert m, name
        w = 11 if "uint32" in m.group(1) else 6
        rows = [", ".join("%*d" % (w, x) for x in v[i:i + 4]) for i in range(0, len(v), 4)]
        body = ",\n".join("  " + r for r in rows) + ","
        src = src[:m.start(2)] + body + src[m.end(2):]
    return src


def main():
    src = RES.read_text()
    out = write(src)
    if "--check" in sys.argv:
        if out != src:
            print("braids resources.cc: not at 44.1 kHz as tools/braids_44k.py writes it: run it")
            return 1
        print("braids resources.cc: at 44.1 kHz")
        return 0
    RES.write_text(out)
    print("%s: 44.1 kHz tables written" % RES)
    return 0


if __name__ == "__main__":
    sys.exit(main())
