#!/usr/bin/env python3
"""Replay the full v0.111.0 Defect card pool against the original game.

Each case replaces the deck with five copies of one pool card, runs two seeded
Oracle policies, and replays every decision in sts2sim. Choices, decision
snapshots, combat state, and final RNG counters are compared by the existing L3
helpers. Cards which the single-player test context cannot legally play are
reported separately instead of silently counted as exercised.
"""
from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tests" / "l3"))
import l3_combat  # noqa: E402


def pool_cards() -> list[tuple[str, dict]]:
    content = json.loads((ROOT / "data" / "content.json").read_text())
    return sorted(
        (model["id"].split(".", 1)[1], model)
        for model in content["models"]
        if model.get("kind") == "CardModel"
        and model.get("props", {}).get("Pool") == "CARD_POOL.DEFECT_CARD_POOL"
    )


def deck_for(card: str, model: dict, overrides: dict[str, list[str]], upgrade_all: bool) -> str:
    def upgrade(spec: str) -> str:
        if not upgrade_all or "+" in spec:
            return spec
        name = spec.split("+", 1)[0]
        entry = name if name.startswith("CARD.") else "CARD." + name
        content = json.loads((ROOT / "data" / "content.json").read_text())
        target = next((m for m in content["models"] if m["id"] == entry), None)
        level = int((target or {}).get("props", {}).get("MaxUpgradeLevel", 0))
        return f"{spec}+{level}" if level else spec

    if card in overrides:
        specs = [upgrade(spec) for spec in overrides[card]]
    else:
        spec = card
        level = int(model.get("props", {}).get("MaxUpgradeLevel", 0))
        if upgrade_all and level:
            spec += f"+{level}"
        specs = [spec] * 5
    return ",".join(specs)


def normalize_decision(value: dict) -> dict:
    result = json.loads(json.dumps(value))
    result["options"].pop("source", None)
    return result


def compare_case(lines: list[str], args: argparse.Namespace) -> tuple[str, int, int, int, int, str]:
    runs = l3_combat.parse_runs(lines)
    played = 0
    for line in lines:
        if not line.startswith("{"):
            continue
        item = json.loads(line)
        if item.get("event") == "action" and item.get("type") == "play":
            played += item.get("card") == args.card

    total_states = matched_states = 0
    for run in runs:
        states, decisions, end, error = l3_combat.replay(
            args.cli, run, args.ascension, deck=args.deck,
            starting_energy=args.starting_energy,
        )
        total_states += len(run["states"])
        for index, expected in enumerate(run["states"]):
            if index >= len(states):
                return "DIFF", played, matched_states, total_states, sum(map(len, (r["decisions"] for r in runs))), "sim stopped at state %d: %s" % (
                    index, error.get("message", "no state") if error else "no state"
                )
            diff = l3_combat.first_diff(expected, states[index])
            if diff:
                return "DIFF", played, matched_states, total_states, sum(map(len, (r["decisions"] for r in runs))), "state %d %s oracle=%s sim=%s" % (
                    index, diff[0], json.dumps(diff[1]), json.dumps(diff[2])
                )
            matched_states += 1
        if len(decisions) != len(run["decisions"]):
            return "DIFF", played, matched_states, total_states, sum(map(len, (r["decisions"] for r in runs))), "card-select decision count oracle=%d sim=%d" % (
                len(run["decisions"]), len(decisions)
            )
        for index, (expected, actual) in enumerate(zip(run["decisions"], decisions)):
            diff = l3_combat.first_diff(normalize_decision(expected), normalize_decision(actual))
            if diff:
                return "DIFF", played, matched_states, total_states, sum(map(len, (r["decisions"] for r in runs))), "decision %d %s oracle=%s sim=%s" % (
                    index, diff[0], json.dumps(diff[1]), json.dumps(diff[2])
                )
        if run["end"] is not None:
            if end is None:
                return "DIFF", played, matched_states, total_states, sum(map(len, (r["decisions"] for r in runs))), "sim did not reach Oracle end"
            diff = l3_combat.first_diff(run["end"], end)
            if diff:
                return "DIFF", played, matched_states, total_states, sum(map(len, (r["decisions"] for r in runs))), "end %s oracle=%s sim=%s" % (
                    diff[0], json.dumps(diff[1]), json.dumps(diff[2])
                )

    if played:
        return "PASS", played, matched_states, total_states, sum(map(len, (r["decisions"] for r in runs))), ""
    return "UNEXERCISED", played, matched_states, total_states, sum(map(len, (r["decisions"] for r in runs))), "no policy played this card"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cards", help="comma-separated pool entry names (default: all 91)")
    parser.add_argument("--seed-prefix", default="DFPOOL1110")
    parser.add_argument("--policy-seeds", default="1,2")
    parser.add_argument("--ascension", type=int, default=0)
    parser.add_argument("--encounter", default="SLIMES_WEAK")
    parser.add_argument("--max-actions", type=int, default=180)
    parser.add_argument("--upgrade-all", action="store_true",
                        help="start each pool card at its maximum source upgrade level")
    parser.add_argument("--deck-overrides", default=str(ROOT / "tests/l3/defect_card_deck_overrides.json"),
                        help="JSON object mapping selected card IDs to legal test decks; pass - to disable")
    parser.add_argument("--cli", default=str(ROOT / "build" / "sts2sim_cli"))
    parser.add_argument("--raw-jsonl", help="save concatenated original Oracle JSONL traces")
    parser.add_argument("--results", help="save the exact per-card comparison output")
    opts = parser.parse_args()

    all_cards = dict(pool_cards())
    overrides = {}
    if opts.deck_overrides != "-":
        overrides = json.loads(Path(opts.deck_overrides).read_text())
    selected = opts.cards.split(",") if opts.cards else sorted(all_cards)
    unknown = sorted(set(selected) - all_cards.keys())
    if unknown:
        parser.error("not in DEFECT_CARD_POOL: " + ",".join(unknown))

    raw_file = open(opts.raw_jsonl, "w", encoding="utf-8") if opts.raw_jsonl else None
    results_file = open(opts.results, "w", encoding="utf-8") if opts.results else None
    counts = {key: 0 for key in ("PASS", "MULTIPLAYER_ONLY", "UNEXERCISED", "DIFF")}
    total_states = total_matched_states = total_choices = total_played = 0
    try:
        for index, card in enumerate(selected):
            seed = "%s_%04d" % (opts.seed_prefix, index)
            deck = deck_for(card, all_cards[card], overrides, opts.upgrade_all)
            oracle_args = argparse.Namespace(
                seeds=seed,
                characters="DEFECT",
                policy_seeds=opts.policy_seeds,
                max_actions=opts.max_actions,
                ascension=opts.ascension,
                encounter=opts.encounter,
                deck=deck,
                relics=None,
                potion=None,
                starting_energy=10 if card == "METEOR_STRIKE" else None,
                starting_stars=None,
                take_special_rewards=False,
            )
            lines = l3_combat.run_oracle(oracle_args)
            if raw_file:
                for line in lines:
                    raw_file.write(line + "\n")
            label, played, matched, oracle_states, choices, detail = compare_case(
                lines,
                argparse.Namespace(
                    cli=opts.cli,
                    ascension=opts.ascension,
                    deck=oracle_args.deck,
                    card=card,
                    starting_energy=oracle_args.starting_energy,
                ),
            )
            if label == "UNEXERCISED" and all_cards[card].get("props", {}).get("MultiplayerConstraint") == "MultiplayerOnly":
                # The headless Oracle runs a single-player combat. Keep these
                # pool entries visible and require their direct C++ model tests.
                label = "MULTIPLAYER_ONLY"
                detail = "source MultiplayerConstraint=MultiplayerOnly; single-player Oracle cannot play it"
            counts[label] += 1
            total_states += oracle_states
            total_matched_states += matched
            total_choices += choices
            total_played += played
            suffix = " " + detail if detail else ""
            label_card = card + ("+max" if opts.upgrade_all else "")
            line = "%s %s played=%d states=%d%s" % (label, label_card, played, matched, suffix)
            print(line, flush=True)
            if results_file:
                results_file.write(line + "\n")
                results_file.flush()
    finally:
        if raw_file:
            raw_file.close()
        if results_file:
            results_file.close()
    summary = ("Defect card pool: %d/%d single-player cases matched; %d MultiplayerOnly direct-test cases; "
               "played_actions=%d; choices=%d; states=%d/%d matched; %d unclassified unexercised; %d divergent" % (
                   counts["PASS"], len(selected) - counts["MULTIPLAYER_ONLY"], counts["MULTIPLAYER_ONLY"],
                   total_played, total_choices, total_matched_states, total_states,
                   counts["UNEXERCISED"], counts["DIFF"]
               ))
    print(summary)
    if opts.results:
        with open(opts.results, "a", encoding="utf-8") as stream:
            stream.write(summary + "\n")
    return 1 if counts["DIFF"] or counts["UNEXERCISED"] else 0


if __name__ == "__main__":
    sys.exit(main())
