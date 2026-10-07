#!/usr/bin/env python3
"""Trial DoubleDown against shipped production callbacks; each action tests clone/restore.

Injected Silent A10 Act 3 event roots are distinguished from the optional four
saved NoSL trajectories, which are replayed in the sampler without injection.
"""
from __future__ import annotations

import argparse
import ctypes
import json
import os
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "python"))
import rf_operations as rf
if "--nosl-only" not in sys.argv:
    from sts2sim_env import Action, Env


def capture(env):
    q = env.export_run_queries()
    result = rf.state(env, q)
    ev = q["event"]
    result.update(description=ev["description_key"], event_finished=ev["finished"],
                  event_options=[{"key": o["text_key"], **{k: o[k] for k in ("locked", "proceed", "chosen", "will_kill_player")}} for o in ev["options"]],
                  is_abandoned=q["is_abandoned"], game_over=q["decision_name"] == "ended", victory=q["is_victory"])
    choice = q["choice"]
    result["event_page_index"] = env.export_snapshot()["state"]["run"]["event_synchronizer"]["page_index"]
    if choice:
        rf.equal("option", choice["kind"], "choice.kind")
        rf.equal([0, 1], choice["legal_answers"], "choice.legal_answers")
        rf.equal((1, 1, False, False), (choice["min_select"], choice["max_select"], choice["subset"], choice["can_skip"]), "choice.bounds")
        result["choice"] = {"source": choice["source"], "prompt": choice["prompt"], "keys": [o["key"] for o in choice["options"]]}
    else:
        result["choice"] = None
    return result


def compare(expected, env, path):
    expected = dict(expected)
    end_calls = expected.pop("end_calls")
    rf.equal(int(expected["game_over"]), end_calls, path + ".native_OnEnded_calls")
    rf.equal(expected, capture(env), path)


def injected_cases(out, seeds):
    oracle = ROOT / "tools/Sts2Oracle/bin/Release/net9.0/Sts2Oracle.dll"
    with (out / "oracle.jsonl").open("w") as stdout, (out / "oracle.stderr").open("w") as stderr:
        subprocess.run([os.environ.get("DOTNET", str(Path.home() / ".dotnet/dotnet")), str(oracle),
                        "trial-confirmation", "--seeds", seeds], stdout=stdout, stderr=stderr, check=True, timeout=120)
    cases = [json.loads(line) for line in (out / "oracle.jsonl").read_text().splitlines()]
    rf.equal(len(seeds.split(",")) * 6, len(cases), "oracle.case_count")
    fixture = ctypes.CDLL(str(ROOT / "build/libenv_api_trial_fixture.so"))
    fixture.sts2_test_trial_fixture.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_void_p)]
    prefixes = 0
    for number, case in enumerate(cases):
        rf.equal(False, case["test_mode"], "oracle.production_mode")
        data = json.dumps({"seed": case["seed"], "prevent_death": case["prevent_death"]}).encode()
        diagnostics = Env()
        output = ctypes.c_void_p()
        try:
            status = fixture.sts2_test_trial_fixture(diagnostics._handle, data, len(data), ctypes.byref(output))
            rf.equal(0, status, "fixture.status")
        finally:
            diagnostics.__del__()
        env = Env.__new__(Env)
        env._handle = output.value
        try:
            compare(case["states"][0], env, f"case[{number}].source")
            recorded = [capture(env)]
            for index, operation in enumerate(case["sequence"].split(","), 1):
                kind, answer = operation.split(":")
                action = Action(6 if kind == "event" else 12, index=int(answer))
                before = env.export_snapshot()
                clone = env.clone()
                restored = Env.restore_snapshot(before)
                try:
                    for branch in (clone, restored):
                        branch.step(action)
                        compare(case["states"][index], branch, f"case[{number}].step[{index}]")
                    rf.equal(before, env.export_snapshot(), "source_unchanged")
                    env.step(action)
                    rf.equal(env.export_snapshot(), clone.export_snapshot(), "clone.successor")
                    rf.equal(env.export_snapshot(), restored.export_snapshot(), "restore.successor")
                finally:
                    clone.__del__(); restored.__del__()
                recorded.append(capture(env))
                prefixes += 1
            final = Env.restore_snapshot(env.export_snapshot())
            try:
                rf.equal(capture(env), capture(final), "final.restore")
            finally:
                final.__del__()
            rf.write_json(out / f"native-{number:02d}.json", recorded)
        finally:
            env.__del__()
    return {"cases": len(cases), "transitions": prefixes, "clone_restore": True, "oracle_test_mode": False}


def saved_failures(path):
    import numpy as np
    from nosl import native
    rows = [json.loads(line) for line in path.read_text().splitlines()]
    results = []
    for row in rows:
        for answer in (0, 1):
            batch = native.Batch(1, 2048, 1024, 10000)
            batch.set_filter(1)
            arrays = [np.zeros(shape, dtype=np.int32) for shape in
                      ((1,), (1, native.GLOBAL_WIDTH), (1, 2048, native.TOKEN_WIDTH),
                       (1, 1024, native.ACTION_WIDTH), (1, 2), (1,), (1,))]
            def step(choice):
                arrays[0][0] = choice
                batch.step(*arrays)
                if int(arrays[5][0]) not in (native.RUNNING, native.DEATH):
                    raise AssertionError(f"{row['seed']}: {native.STATUS_NAMES[int(arrays[5][0])]}: {batch.last_error(0)}")
            try:
                rf.equal(native.RUNNING, batch.reset(0, row["seed"], 10), "failure.reset")
                step(-1)
                for action in row["action_rows"]:
                    rf.equal(native.RUNNING, int(arrays[5][0]), "failure.prefix_running")
                    step(action)
                rf.equal(7, int(arrays[1][0, native.G_DECISION]), "failure.confirmation_choice")
                rf.equal(2, int(arrays[4][0, 1]), "failure.confirmation_options")
                before = batch.run_info(0)
                for key in ("act", "floor", "hp", "max_hp", "gold", "deck", "relics"):
                    rf.equal(row[key], before[key], "failure." + key)
                step(answer)
                rf.equal(native.DEATH if answer == 0 else native.RUNNING, int(arrays[5][0]), "failure.answer_status")
                if answer == 1:
                    rf.equal(before, batch.run_info(0), "failure.cancel_no_effect")
                    rf.equal(3, int(arrays[1][0, native.G_DECISION]), "failure.cancel_returns_event")
                results.append({"seed": row["seed"], "answer": answer, "status": native.STATUS_NAMES[int(arrays[5][0])],
                                "steps": int(arrays[6][0]), "before": before, "after": batch.run_info(0)})
            finally:
                batch.__del__()
    return results


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--seeds", default="TRIALCONFIRM01,TRIALCONFIRM02")
    parser.add_argument("--failures", type=Path)
    parser.add_argument("--nosl-only", action="store_true", help=argparse.SUPPRESS)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    summary = {} if args.nosl_only else injected_cases(args.out, args.seeds)
    if args.failures:
        if args.nosl_only:
            summary["saved_nosl_failures"] = saved_failures(args.failures)
        else:
            # Env and sampler DSOs each own a callback registry. Exercise the
            # sampler in its normal separate process, avoiding ELF symbol
            # interposition between two independent statically linked engines.
            subprocess.run([sys.executable, __file__, "--nosl-only", "--out", str(args.out / "nosl"),
                            "--failures", str(args.failures)], check=True, timeout=120)
            summary.update(json.loads((args.out / "nosl/summary.json").read_text()))
    rf.write_json(args.out / "summary.json", summary)
    print(json.dumps(summary, ensure_ascii=False))


if __name__ == "__main__":
    main()
