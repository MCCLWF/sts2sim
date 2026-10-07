#!/usr/bin/env python3
"""Oracle-diff Regent pool relic and potion effects with raw two-sided traces."""
import json
import io
import os
import sys
from types import SimpleNamespace

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tests", "l3"))
import l3_combat


ENCOUNTER = "BATTLEWORN_DUMMY_EVENT_V1_ENCOUNTER"
RELIC_CASES = [
    ("DivineRight", None, "ASTRAL_PULSE", 12, 50),
    ("FencingManual", "FENCING_MANUAL", "STRIKE_REGENT", 12, 50),
    ("GalacticDust", "GALACTIC_DUST", "ASTRAL_PULSE", 12, 50),
    ("LunarPastry", "LUNAR_PASTRY", "STRIKE_REGENT", 12, 50),
    ("MiniRegent", "MINI_REGENT", "ASTRAL_PULSE", 12, 50),
    ("OrangeDough", "ORANGE_DOUGH", "STRIKE_REGENT", 12, 50),
    ("Regalite", "REGALITE", "CRASH_LANDING", 12, 50),
    ("VitruvianMinion", "VITRUVIAN_MINION", "MINION_STRIKE", 12, 50),
]
POTION_CASES = [
    ("CosmicConcoction", "COSMIC_CONCOCTION", "STRIKE_REGENT", 10, 8),
    ("KingsCourage", "KINGS_COURAGE", "STRIKE_REGENT", 10, 8),
    ("StarPotion", "STAR_POTION", "STRIKE_REGENT", 10, 8),
]


def verify_cases(kind, cases, report_dir):
    oracle_path = os.path.join(report_dir, f"regent-{kind}-pool-oracle.jsonl")
    sim_path = os.path.join(report_dir, f"regent-{kind}-pool-sim.jsonl")
    results_path = os.path.join(report_dir, f"regent-{kind}-pool-results.json")
    raw, sim_raw = [], []
    results = []
    total_states = matched_states = total_selections = matched_selections = 0
    for index, (model, item_id, card, energy, stars) in enumerate(cases, 1):
        prefix = "REGENTRELIC" if kind == "relic" else "REGENTPOTION"
        seed = f"{prefix}{index:02d}"
        deck = ",".join([card] * 5)
        oracle_args = SimpleNamespace(
            seeds=seed, characters="REGENT", policy_seeds="1", max_actions=100,
            ascension=0, encounter=ENCOUNTER, deck=deck,
            relics=item_id if kind == "relic" else None,
            potion=item_id if kind == "potion" else None,
            starting_energy=energy, starting_stars=stars, take_special_rewards=False,
        )
        lines = l3_combat.run_oracle(oracle_args)
        raw.extend(lines)
        runs = l3_combat.parse_runs(lines)
        if len(runs) != 1:
            raise RuntimeError(f"{model}: expected one Oracle run, got {len(runs)}")
        run = runs[0]
        sim_raw.append(json.dumps({"event": "case", "model": model, "seed": seed,
                                  "item": item_id, "deck": deck}, separators=(",", ":")))
        sim_trace = io.StringIO()
        states, decisions, end, error = l3_combat.replay(
            os.path.join(ROOT, "build", "sts2sim_cli"), run, 0,
            deck=deck, relics=item_id if kind == "relic" else None,
            potion=item_id if kind == "potion" else None,
            starting_energy=energy, starting_stars=stars,
            raw_output=sim_trace,
        )
        sim_raw.append(sim_trace.getvalue().rstrip("\n"))
        difference = None
        case_matched_states = 0
        for i, expected in enumerate(run["states"]):
            total_states += 1
            if i >= len(states):
                difference = {"path": f"state[{i}]", "oracle": "present", "sim": "stopped", "error": error}
                break
            found = l3_combat.first_diff(expected, states[i])
            if found:
                difference = {"path": f"state[{i}].{found[0]}", "oracle": found[1], "sim": found[2]}
                break
            matched_states += 1
            case_matched_states += 1
        if difference is None:
            for i, expected in enumerate(run["decisions"]):
                total_selections += 1
                if i >= len(decisions):
                    difference = {"path": f"card_select[{i}]", "oracle": "present", "sim": "missing"}
                    break
                expected = json.loads(json.dumps(expected))
                found = json.loads(json.dumps(decisions[i]))
                expected["options"].pop("source", None)
                found["options"].pop("source", None)
                diff = l3_combat.first_diff(expected, found)
                if diff:
                    difference = {"path": f"card_select[{i}].{diff[0]}", "oracle": diff[1], "sim": diff[2]}
                    break
                matched_selections += 1
        if difference is None and run["end"] is not None:
            if end is None:
                difference = {"path": "end", "oracle": "present", "sim": "missing", "error": error}
            else:
                found = l3_combat.first_diff(run["end"], end)
                if found:
                    difference = {"path": f"end.{found[0]}", "oracle": found[1], "sim": found[2]}
        result = {"model": model, "item_id": item_id, "seed": seed,
                  "identical": difference is None, "states": len(run["states"]),
                  "matched_states": case_matched_states,
                  "selections": len(run["decisions"]), "difference": difference}
        results.append(result)
        print(f"{'ok' if result['identical'] else 'DIFF'} {model}: "
              f"{result['matched_states']}/{result['states']} states, "
              f"{result['selections']} selections" + (f" {difference}" if difference else ""), flush=True)
    with open(oracle_path, "w", encoding="utf-8") as out:
        out.write("\n".join(raw) + "\n")
    with open(sim_path, "w", encoding="utf-8") as out:
        out.write("\n".join(sim_raw) + "\n")
    manifest = {
        "source": "Sts2Oracle", "scope": f"REGENT_{kind.upper()}_POOL",
        "encounter": ENCOUNTER, "oracle_trace": os.path.basename(oracle_path),
        "sim_trace": os.path.basename(sim_path), "cases": results,
        "totals": {"models": len(results), "identical": sum(r["identical"] for r in results),
                   "states": total_states, "matched_states": matched_states,
                   "selections": total_selections, "matched_selections": matched_selections},
    }
    with open(results_path, "w", encoding="utf-8") as out:
        json.dump(manifest, out, ensure_ascii=False, indent=2)
        out.write("\n")
    return all(result["identical"] for result in results)


def main():
    report_dir = os.path.join(ROOT, "docs", "reports", "regent-v0110")
    os.makedirs(report_dir, exist_ok=True)
    ok = verify_cases("relic", RELIC_CASES, report_dir)
    ok = verify_cases("potion", POTION_CASES, report_dir) and ok
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
