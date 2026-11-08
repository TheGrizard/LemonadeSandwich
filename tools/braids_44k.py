#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Braids at the FM-1's 44.1 kHz (firmware/third_party/mi/braids, MIT; written for 96 kHz): its tables that depend on
the sample rate made again, in resources.cc, with the formulas of Braids' own resources/lookup_tables.py:

  lut_oscillator_increments, lut_oscillator_delays   the pitch (every model's phase increments)
  lut_svf_cutoff, lut_svf_damp, lut_svf_scale        its state-variable filters (noise, VOWEL FOF, the Z filters)
  lut_granular_envelope_rate                         GRANULAR CLOUD's grains as long as at 96 kHz

  python3 tools/braids_44k.py            write them
  python3 tools/braids_44k.py --check    fail if resources.cc differs from what this writes (tests/run_tests.sh)"""
import math
import re
import sys
from pathlib import Path

RES = Path(__file__).resolve().parent.parent / "firmware/third_party/mi/braids/resources.cc"
SR, SR_BRAIDS = 44100.0, 96000.0


def tables():
    """(plain Python, not numpy as Braids' own script: the CI's Python has none)"""
    t = {}
    excursion = 65536 * 65536.0
    notes = [128 * 128.0 + 16 * i for i in range((12 * 128 + 16) // 16)]
    pitches = [440.0 * 2 ** ((n - 69 * 128) / (128 * 12)) for n in notes]
    t["lut_oscillator_increments"] = [int(excursion / SR * p) for p in pitches]
    t["lut_oscillator_delays"] = [int(SR / p * 65536 * 4096) for p in pitches]
    f = [min(440.0 * 2 ** ((i - 69) / 12.0) / SR, 1 / 8.0) for i in range(257)]
    f = [2 * math.sin(math.pi * x) for x in f]
    damp = [min(2 * (1 - (i / 260.0) ** 0.25), min(2, 2 / x - x * 0.5)) for i, x in enumerate(f)]
    t["lut_svf_cutoff"] = [int(x * 32767.0) for x in f]
    t["lut_svf_damp"] = [int(d * 32767.0) for d in damp]
    t["lut_svf_scale"] = [int(((d / 2) ** 0.5) * 32767.0) for d in damp]
    t["lut_granular_envelope_rate"] = [int(min(2 ** (i / 64.0) * (1 << 14) / 8 * (SR_BRAIDS / SR), 65535))
                                       for i in range(257)]
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
