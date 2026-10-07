#!/usr/bin/env python3
"""The native forward pass against the PyTorch model, on real observations.

  PYTHONPATH=python .venv/bin/python tools/nosl/check_policy_net.py --checkpoint artifacts/nosl-q2/s1/checkpoint.pt

Plays runs with the checkpoint's own sampled actions and, at every decision,
computes the logits and the value both ways (PyTorch in float32 on the CPU,
and nosl/policy_net.cpp). Passes when no logit or value differs by more than
the tolerance and the native pass never prefers an action PyTorch scores
lower by more than the tolerance (exact ties between identical rows aside).
It also times the native pass. Exit code 1 otherwise.
"""
import argparse
import ctypes
import json
import sys
import tempfile
import time
from pathlib import Path

import numpy as np
import torch

from nosl import native
from nosl.export import export
from nosl.model import load


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--checkpoint", required=True)
    p.add_argument("--runs", type=int, default=16)
    p.add_argument("--tolerance", type=float, default=2e-3)
    args = p.parse_args()

    lib = native._LIB
    lib.nosl_policy_load.argtypes = [ctypes.c_char_p]
    lib.nosl_policy_load.restype = ctypes.c_void_p
    lib.nosl_policy_last_error.restype = ctypes.c_char_p
    lib.nosl_batch_policy_forward.argtypes = [ctypes.c_void_p, ctypes.c_int32, ctypes.c_void_p,
                                              ctypes.POINTER(ctypes.c_float), ctypes.c_int32, ctypes.POINTER(ctypes.c_float)]
    model, _ = load(args.checkpoint, "cpu")
    model.eval()
    with tempfile.TemporaryDirectory() as temp:
        weights = str(Path(temp) / "policy.bin")
        exported = export(args.checkpoint, weights)
        policy = lib.nosl_policy_load(weights.encode())
        if not policy:
            raise RuntimeError("native load failed: " + (lib.nosl_policy_last_error() or b"").decode())

    n, T, A = args.runs, 192, 160
    batch = native.Batch(n, T, A, 30000)
    g = np.zeros((n, native.GLOBAL_WIDTH), np.int32); tok = np.zeros((n, T, native.TOKEN_WIDTH), np.int32)
    act = np.zeros((n, A, native.ACTION_WIDTH), np.int32); cnt = np.zeros((n, 2), np.int32)
    status = np.zeros(n, np.int32); steps = np.zeros(n, np.int32); idle = np.full(n, -1, np.int32)
    for j in range(n):
        batch.reset(j, f"NTPN{j:04d}", 10)
    batch.step(idle, g, tok, act, cnt, status, steps)
    torch.manual_seed(1)
    out = (ctypes.c_float * A)()
    value = ctypes.c_float()
    decisions = argmax_differs = 0
    worst_logit = worst_value = 0.0
    native_ms = []
    while (status == native.RUNNING).any():
        live = np.nonzero(status == native.RUNNING)[0]
        tokens_used, actions_used = int(cnt[live, 0].max()), int(cnt[live, 1].max())
        with torch.no_grad():
            logits, values = model(torch.from_numpy(g[live]), torch.from_numpy(tok[live, :tokens_used]),
                                   torch.from_numpy(act[live, :actions_used]), torch.from_numpy(cnt[live]))
        choice = np.full(n, -1, np.int32)
        choice[live] = torch.distributions.Categorical(logits=logits).sample().numpy()
        for row, j in enumerate(live):
            t0 = time.perf_counter()
            count = lib.nosl_batch_policy_forward(batch._handle, int(j), policy, out, A, ctypes.byref(value))
            native_ms.append((time.perf_counter() - t0) * 1000)
            if count != cnt[j, 1]:
                raise RuntimeError("native action count differs from the observation")
            mine = np.frombuffer(out, dtype=np.float32, count=count)
            theirs = logits[row, :count].numpy()
            worst_logit = max(worst_logit, float(np.abs(mine - theirs).max()))
            worst_value = max(worst_value, abs(float(value.value) - float(values[row])))
            # Rows with the same content (two copies of a card) tie exactly in
            # exact arithmetic; either may come out on top. A different pick
            # only counts when PyTorch scores it lower by more than the tolerance.
            argmax_differs += int(theirs[mine.argmax()] < theirs.max() - args.tolerance)
            decisions += 1
        batch.step(choice, g, tok, act, cnt, status, steps)
    native_ms.sort()
    ok = argmax_differs == 0 and worst_logit <= args.tolerance and worst_value <= args.tolerance
    print(json.dumps({"decisions": decisions, "tensors": exported["tensors"], "parameters": exported["parameters"],
                      "argmax_differs": argmax_differs, "max_logit_difference": round(worst_logit, 6),
                      "max_value_difference": round(worst_value, 6), "tolerance": args.tolerance,
                      "native_ms_median": round(native_ms[len(native_ms) // 2], 3),
                      "native_ms_p95": round(native_ms[int(.95 * len(native_ms))], 3),
                      "native_ms_max": round(native_ms[-1], 3), "passed": ok}))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
