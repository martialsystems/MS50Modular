#!/usr/bin/env python3
# Copyright (c) 2026 Martial Systems LLC. All rights reserved.

"""Play a stereo sine through the MS-50 Modular VST3 and compare it to the input.

Mix stays at the plugin default, which is 0. This script does not install packages.
"""

import argparse
import math
import sys
from pathlib import Path


def skip() -> int:
    print("SINE_HOST SKIP")
    return 0


def fail(max_error=None) -> int:
    if max_error is None:
        print("SINE_HOST FAIL")
    else:
        print(f"SINE_HOST FAIL max error {max_error:.3e}")
    return 1


def main() -> int:
    parser = argparse.ArgumentParser(description="Sine through the MS-50 Modular VST3 at mix 0.")
    parser.add_argument("--vst3", required=True, help="Path to the MS-50 Modular.vst3 bundle")
    parser.add_argument("--sr", type=float, default=48000.0)
    parser.add_argument("--seconds", type=float, default=0.25)
    parser.add_argument("--hz", type=float, default=220.0)
    parser.add_argument("--dbfs", type=float, default=-12.0)
    args = parser.parse_args()

    try:
        import numpy as np
        from pedalboard import load_plugin
    except Exception:
        return skip()

    bundle = Path(args.vst3)
    if not bundle.exists():
        return skip()

    sample_count = int(round(args.seconds * args.sr))
    if sample_count < 1 or args.sr <= 0.0:
        return fail()

    amplitude = 10.0 ** (args.dbfs / 20.0)
    phase = 2.0 * math.pi * args.hz * np.arange(sample_count, dtype=np.float64) / args.sr
    left = (amplitude * np.sin(phase)).astype(np.float32)
    right = (amplitude * np.sin(phase + (math.pi / 2.0))).astype(np.float32)
    audio = np.vstack([left, right])

    try:
        plugin = load_plugin(str(bundle))
        if not bool(plugin.is_effect):
            return fail()
        rendered = plugin(audio, args.sr)
        latency = int(plugin.reported_latency_samples)
    except Exception:
        return fail()

    if latency != 0:
        return fail()
    if rendered.shape != audio.shape:
        return fail()

    error = float(np.max(np.abs(rendered.astype(np.float64) - audio.astype(np.float64))))
    if not math.isfinite(error) or error >= 1.0e-5:
        return fail(error)

    print("SINE_HOST PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
