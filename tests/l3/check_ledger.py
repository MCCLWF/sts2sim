#!/usr/bin/env python3
"""Check RNG ledger counter deltas between adjacent combat states.

The ledger intentionally emits one row for an outermost public Rng call.  A
Gaussian call can consume several underlying NextDouble calls, so the check
uses the counter difference between successive ledger rows as the number of
draws represented by that row.
"""

from __future__ import annotations

import argparse
import json
import sys
from collections import defaultdict
from typing import Iterable, TextIO


def state_counters(state: dict) -> dict[str, int]:
    counters = {str(k): int(v) for k, v in state.get("rng", {}).items()}
    for enemy in state.get("enemies", []):
        combat_id = enemy.get("combat_id")
        counter = enemy.get("rng_counter")
        if combat_id is not None and counter is not None:
            counters[f"monster.{combat_id}"] = int(counter)
    return counters


def check(lines: Iterable[str]) -> tuple[int, int]:
    states = 0
    intervals = 0
    pending: dict[str, list[int]] = defaultdict(list)
    previous: dict[str, int] | None = None

    def finish_interval(next_counters: dict[str, int]) -> None:
        nonlocal intervals, pending, previous
        if previous is None:
            pending.clear()
            previous = next_counters
            return
        intervals += 1
        streams = set(previous) | set(next_counters) | set(pending)
        errors: list[str] = []
        for stream in sorted(streams):
            # "other" has no state counter by design and cannot be checked.
            if stream == "other":
                continue
            expected = next_counters.get(stream, 0) - previous.get(stream, 0)
            baseline = previous.get(stream)
            actual = 0
            for counter in pending.get(stream, []):
                if baseline is None:
                    # An unreported stream is not represented in this state.
                    baseline = counter
                    continue
                actual += counter - baseline
                baseline = counter
            if actual != expected:
                errors.append(f"{stream}: expected {expected}, ledger {actual}")
        if errors:
            raise ValueError("state interval %d: %s" % (intervals, "; ".join(errors)))
        pending.clear()
        previous = next_counters

    for line_number, raw in enumerate(lines, 1):
        if not raw.strip():
            continue
        try:
            row = json.loads(raw)
        except json.JSONDecodeError as exc:
            raise ValueError(f"line {line_number}: invalid JSON: {exc}") from exc
        event = row.get("event")
        if event == "begin":
            # A new begin after an end starts a fresh counter domain.  The
            # oracle emits begin before any rows for that run.
            pending.clear()
            previous = None
        elif event == "rng":
            stream = row.get("stream")
            counter = row.get("counter")
            if not isinstance(stream, str) or not isinstance(counter, int):
                raise ValueError(f"line {line_number}: malformed rng row")
            pending[stream].append(counter)
        elif event == "state":
            state = row.get("state")
            if not isinstance(state, dict):
                raise ValueError(f"line {line_number}: malformed state row")
            finish_interval(state_counters(state))
            states += 1
        elif event in {"action", "enter", "end"}:
            continue
    return states, intervals


def main(stream: TextIO) -> int:
    try:
        states, intervals = check(stream)
    except ValueError as exc:
        print(f"ledger check failed: {exc}", file=sys.stderr)
        return 1
    print(f"ledger check passed: {states} states, {intervals} intervals")
    return 0


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("path", nargs="?", default="-", help="oracle JSONL path, or - for stdin")
args = parser.parse_args()
if args.path == "-":
    raise SystemExit(main(sys.stdin))
with open(args.path, encoding="utf-8") as input_file:
    raise SystemExit(main(input_file))
