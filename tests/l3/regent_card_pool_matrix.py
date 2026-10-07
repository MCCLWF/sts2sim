#!/usr/bin/env python3
"""Run focused Sts2Oracle L3 replays for cards in the Regent pool.

The checked-in port checklist is the inventory source. Each case uses five
copies of the same card with ample energy and stars, so the seeded Oracle
policy can exercise repeated plays and card-selection branches. Raw Oracle
JSONL and a per-model result manifest are written for review.
"""
import argparse
import json
import os
import re
import sys
from types import SimpleNamespace

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tests", "l3"))
import l3_combat


def card_ids_from_checklist(inventory="pool"):
    path = os.path.join(ROOT, "docs", "audits", "regent-v0110-port-checklist.md")
    text = open(path, encoding="utf-8").read()
    if inventory == "pool":
        heading, expected = "## RegentCardPool 全部卡牌", 91
    elif inventory == "derived":
        heading, expected = "## 显式生成/转换及派生卡依赖", 5
    else:
        raise ValueError(inventory)
    section = text.split(heading, 1)[1].split("## ", 1)[0]
    names = re.findall(r"^\|\s*`([A-Za-z0-9]+)`\s*\| `Cards/", section, re.M)
    if len(names) != expected or len(set(names)) != expected:
        raise SystemExit(f"checklist must contain {expected} unique {inventory} cards; found {len(names)}")
    result = []
    for name in names:
        snake = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1_\2", name)
        snake = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", snake).upper()
        result.append((name, snake))
    return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cli", default=os.path.join(ROOT, "build", "sts2sim_cli"))
    ap.add_argument("--report-dir", default=os.path.join(ROOT, "docs", "reports", "regent-v0110"))
    ap.add_argument("--starting-energy", type=int, default=12)
    ap.add_argument("--starting-stars", type=int, default=50)
    ap.add_argument("--upgrade", type=int, choices=(0, 1, 2), default=0,
                    help="start every tested card at this upgrade level")
    ap.add_argument("--inventory", choices=("pool", "derived"), default="pool",
                    help="audit the full Regent pool or its generated/transform dependency cards")
    ap.add_argument("--only", help="comma-separated model names for a focused post-change rerun")
    ap.add_argument("--max-actions", type=int, default=100)
    args = ap.parse_args()
    os.makedirs(args.report_dir, exist_ok=True)
    scope = "card-pool" if args.inventory == "pool" else "derived-card-dependencies"
    suffix = "" if args.upgrade == 0 else f"-upgraded-{args.upgrade}"
    if args.only:
        only = {name.strip() for name in args.only.split(",") if name.strip()}
        pool = card_ids_from_checklist(args.inventory)
        selected = [(model, card_id) for model, card_id in pool if model in only or card_id in only]
        found = {model for model, _ in selected} | {card_id for _, card_id in selected}
        if not selected or not only <= found:
            raise SystemExit(f"unknown --only entries: {', '.join(sorted(only - found))}")
        scope += "-only-" + "-".join(sorted(model.lower() for model, _ in selected))
    else:
        selected = card_ids_from_checklist(args.inventory)
    raw_path = os.path.join(args.report_dir, f"regent-{scope}{suffix}-oracle.jsonl")
    sim_path = os.path.join(args.report_dir, f"regent-{scope}{suffix}-sim.jsonl")
    summary_path = os.path.join(args.report_dir, f"regent-{scope}{suffix}-results.json")
    cases, total_states, matched_states = [], 0, 0
    with open(raw_path, "w", encoding="utf-8") as raw, open(sim_path, "w", encoding="utf-8") as sim_raw:
        for number, (model, card_id) in enumerate(selected, 1):
            seed = f"REGENTCARD{number:02d}"
            card_spec = card_id if args.upgrade == 0 else f"{card_id}+{args.upgrade}"
            deck = ",".join([card_spec] * 5)
            oracle_args = SimpleNamespace(
                seeds=seed, characters="REGENT", policy_seeds="1", max_actions=args.max_actions,
                ascension=0, encounter="BATTLEWORN_DUMMY_EVENT_V1_ENCOUNTER", deck=deck,
                relics=None, potion=None, starting_energy=args.starting_energy,
                starting_stars=args.starting_stars, take_special_rewards=False,
            )
            try:
                lines = l3_combat.run_oracle(oracle_args)
            except Exception as error:
                stdout = getattr(error, "stdout", "") or ""
                stderr = getattr(error, "stderr", "") or str(error)
                if stdout:
                    raw.write(stdout)
                    if not stdout.endswith("\n"):
                        raw.write("\n")
                case = {"model": model, "card_id": card_id, "seed": seed, "identical": False,
                        "states": 0, "matched_states": 0, "card_select_decisions": 0,
                        "difference": {"path": "oracle", "error": stderr[-2000:]}}
                cases.append(case)
                print(f"ORACLE_ERROR {model}: {stderr.strip()[-300:]}", flush=True)
                continue
            raw.write("\n".join(lines) + "\n")
            runs = l3_combat.parse_runs(lines)
            case = {"model": model, "card_id": card_id, "seed": seed, "identical": False,
                    "states": 0, "matched_states": 0, "card_select_decisions": 0, "difference": None}
            sim_raw.write(json.dumps({"event": "case", "model": model, "seed": seed,
                                      "upgrade": args.upgrade}, separators=(",", ":")) + "\n")
            if len(runs) != 1:
                case["difference"] = f"expected one Oracle run, got {len(runs)}"
                cases.append(case)
                continue
            run = runs[0]
            states, decisions, end, error = l3_combat.replay(
                args.cli, run, 0, deck, None, None, args.starting_energy,
                args.starting_stars, False, sim_raw)
            case["states"] = len(run["states"])
            case["card_select_decisions"] = len(run["decisions"])
            total_states += len(run["states"])
            difference = None
            for i, want in enumerate(run["states"]):
                if i >= len(states):
                    difference = {"path": f"state[{i}]", "oracle": "present", "sim": "stopped",
                                  "error": error}
                    break
                found = l3_combat.first_diff(want, states[i])
                if found:
                    difference = {"path": f"state[{i}].{found[0]}", "oracle": found[1], "sim": found[2]}
                    break
                case["matched_states"] += 1
            matched_states += case["matched_states"]
            if difference is None:
                for i, want in enumerate(run["decisions"]):
                    if i >= len(decisions):
                        difference = {"path": f"card_select[{i}]", "oracle": "present", "sim": "missing"}
                        break
                    want = json.loads(json.dumps(want))
                    got = json.loads(json.dumps(decisions[i]))
                    want["options"].pop("source", None)
                    got["options"].pop("source", None)
                    found = l3_combat.first_diff(want, got)
                    if found:
                        difference = {"path": f"card_select[{i}].{found[0]}",
                                      "oracle": found[1], "sim": found[2]}
                        break
            if difference is None and run["end"] is not None:
                if end is None:
                    difference = {"path": "end", "oracle": "present", "sim": "missing", "error": error}
                else:
                    found = l3_combat.first_diff(run["end"], end)
                    if found:
                        difference = {"path": f"end.{found[0]}", "oracle": found[1], "sim": found[2]}
            case["identical"] = difference is None
            case["difference"] = difference
            cases.append(case)
            print(f"{'ok' if case['identical'] else 'DIFF'} {model}: "
                  f"{case['matched_states']}/{case['states']} states, "
                  f"{case['card_select_decisions']} selections"
                  + (f" {difference}" if difference else ""), flush=True)
    manifest = {"source": "Sts2Oracle", "scope": f"{scope} ({len(cases)} cards), upgrade {args.upgrade}",
                "encounter": "BATTLEWORN_DUMMY_EVENT_V1_ENCOUNTER", "starting_energy": args.starting_energy,
                "starting_stars": args.starting_stars, "max_actions": args.max_actions, "upgrade": args.upgrade,
                "oracle_trace": os.path.basename(raw_path), "sim_trace": os.path.basename(sim_path),
                "cases": cases, "totals": {"models": len(cases), "identical": sum(c["identical"] for c in cases),
                "states": total_states, "matched_states": matched_states}}
    with open(summary_path, "w", encoding="utf-8") as out:
        json.dump(manifest, out, indent=2, ensure_ascii=False)
        out.write("\n")
    totals = manifest["totals"]
    print(f"L3 Regent {scope}: {totals['identical']}/{totals['models']} models identical, "
          f"{totals['matched_states']}/{totals['states']} states matched")
    return 0 if totals["identical"] == totals["models"] else 1


if __name__ == "__main__":
    sys.exit(main())
