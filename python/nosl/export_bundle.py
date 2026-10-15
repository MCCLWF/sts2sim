"""Write a policy bundle for the native actor (nosl/actor.h).

  .venv/bin/python -m nosl.export_bundle --combat C.pt [--base B.pt] [--strategy Q.pt --margin 0.02]
                                         [--table TABLE.json --table-delta 0.01]
                                         [--readiness MODEL.json --readiness-lambda 0.15 --readiness-scope cards,upgrade
                                          --readiness-acts 0,1 | --readiness-chain] --out DIR

DIR gets one exported network per role (nosl.export) and `policy.bundle`, the
text file that names them. The actor then decides inside combat with the
combat network and outside it by the strategic rule of nosl.strategy_q: the
base policy's most probable action (the combat network's when no base is
given), replaced by the value network's best action when that is more than
the margin above it. All of them read the same public observation tables.

--table adds the value table of nosl.strategy_q's table rule (tools/nosl/card_table.py output) as `table.tsv`,
one context, label and value per line. The native actor spells the contexts of nested choices and of rest sites;
a table that holds any other kind is refused, because the actor would silently not apply it.

--readiness adds the readiness rule of nosl.readiness (tools/nosl/boss_readiness.py output) as `readiness.tsv`:
the boss doors of --readiness-acts, or with --readiness-chain the whole chain of doors with its weights. Rows are
written in the model file's own order with every number in full, so that the actor sums what Python sums.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path

from .export import export
from .model import load


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--combat", required=True)
    p.add_argument("--base", default="")
    p.add_argument("--strategy", default="")
    p.add_argument("--margin", type=float, default=0.0)
    p.add_argument("--table", default="")
    p.add_argument("--table-delta", type=float, default=0.0)
    p.add_argument("--readiness", default="")
    p.add_argument("--readiness-lambda", type=float, default=0.07)
    p.add_argument("--readiness-scope", default="cards,upgrade")
    p.add_argument("--readiness-acts", default="0")
    p.add_argument("--readiness-no-table", action="store_true")
    p.add_argument("--readiness-chain", action="store_true")
    p.add_argument("--combat-value", default="", help="a combat advantage checkpoint (nosl.rollout_distill --target advantage)")
    p.add_argument("--combat-margin", type=float, default=0.05)
    p.add_argument("--guards", default="", help="nosl.guards, e.g. nodiscard,nobuyfull,takerewards")
    p.add_argument("--guards-low", default="", help="LEVEL:GUARDS, the set used instead at ascension LEVEL or below")
    p.add_argument("--out", required=True)
    args = p.parse_args(argv)
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    lines, record = ["nosl-policy-bundle 1"], {"margin": args.margin}
    for role, checkpoint in (("combat", args.combat), ("base", args.base), ("strategy", args.strategy)):
        if not checkpoint:
            continue
        blob = load(checkpoint, "cpu")[1]
        if role == "strategy" and blob.get("kind") != "strategy_q":
            raise SystemExit("--strategy is not a strategy_q checkpoint")
        if role != "strategy" and blob.get("actor_filter") != 1:
            raise SystemExit(f"--{role} must be a policy trained behind actor filter 1")
        export(checkpoint, str(out / f"{role}.bin"))
        lines.append(f"{role} {role}.bin")
        record[role] = {"checkpoint": checkpoint, "checkpoint_sha256": hashlib.sha256(Path(checkpoint).read_bytes()).hexdigest(),
                        "exported_sha256": hashlib.sha256((out / f"{role}.bin").read_bytes()).hexdigest()}
    if args.strategy:
        lines.append(f"margin {args.margin!r}")
    if args.table:
        if not args.strategy:
            raise SystemExit("--table needs --strategy: the table rule works over the strategic rule")
        contexts = json.loads(Path(args.table).read_text())["contexts"]
        other = sorted({context.split()[0] for context in contexts} - {"choice", "rest"})
        if other:
            raise SystemExit(f"the native actor does not know table contexts of kind {other}")
        rows = [f"{context}\t{label}\t{value!r}" for context, labels in sorted(contexts.items()) for label, value in sorted(labels.items())]
        if any("\t" in context or "\t" in label for context, labels in contexts.items() for label in labels):
            raise SystemExit("a table context or label holds a tab")
        (out / "table.tsv").write_text("\n".join(rows) + "\n")
        lines += ["table table.tsv", f"table_delta {args.table_delta!r}"]
        record["table"] = {"source": args.table, "delta": args.table_delta, "rows": len(rows),
                           "source_sha256": hashlib.sha256(Path(args.table).read_bytes()).hexdigest(),
                           "exported_sha256": hashlib.sha256((out / "table.tsv").read_bytes()).hexdigest()}
    if args.readiness:
        if not args.strategy:
            raise SystemExit("--readiness needs --strategy: the readiness rule works over the strategic rule")
        data = json.loads(Path(args.readiness).read_text())
        if args.readiness_chain:
            doors = [(name, data["segments"][name]) for name in data["order"]]
        else:
            doors = [(f"act{act}", data["acts"][act]) for act in args.readiness_acts.split(",")]
        rows = []
        for name, t in doors:
            act = int(t["act"]) if "act" in t else int(name[3:])
            which, floor, passes = t.get("door") or ["last", {0: 16, 1: 32}[act], None]
            rows.append(f"door\t{name}\t{act}\t{which}\t{floor}\t{passes or 0}\t{float(t['hp10'])!r}")
            rows += [f"boss\t{name}\t{int(boss)}" for boss in t["boss"]]
            for card, v in t["card"].items():
                if len(v["n"]) != 5 or len(v["u"]) != 3:
                    raise SystemExit("the native actor reads 5 copy levels and 3 upgrade levels per card")
                rows.append(f"card\t{name}\t{int(card)}\t" + "\t".join(repr(float(x)) for x in v["n"] + v["u"]))
            rows += [f"card_boss\t{name}\t{int(boss)}\t{int(card)}\t{float(value)!r}"
                     for boss, items in t["card_boss"].items() for card, value in items.items()]
        if args.readiness_chain:
            rows += [f"weight\t{int(level)}\t{name}\t{float(w['p'])!r}\t{float(w['gain'])!r}"
                     for level, items in data["weights"].items() for name, w in items.items()]
        (out / "readiness.tsv").write_text("\n".join(rows) + "\n")
        lines += ["readiness readiness.tsv", f"readiness_lambda {args.readiness_lambda!r}", f"readiness_scope {args.readiness_scope}",
                  f"readiness_chain {int(args.readiness_chain)}", f"readiness_table {int(not args.readiness_no_table)}"]
        record["readiness"] = {"source": args.readiness, "lambda": args.readiness_lambda, "scope": args.readiness_scope,
                               "acts": None if args.readiness_chain else args.readiness_acts, "chain": args.readiness_chain,
                               "table": not args.readiness_no_table, "rows": len(rows),
                               "source_sha256": hashlib.sha256(Path(args.readiness).read_bytes()).hexdigest(),
                               "exported_sha256": hashlib.sha256((out / "readiness.tsv").read_bytes()).hexdigest()}
    if args.combat_value:
        if load(args.combat_value, "cpu")[1].get("kind") != "combat_advantage":
            raise SystemExit("--combat-value is not a combat advantage checkpoint")
        export(args.combat_value, str(out / "combat_value.bin"))
        lines += ["combat_value combat_value.bin", f"combat_margin {args.combat_margin!r}"]
        record["combat_value"] = {"checkpoint": args.combat_value, "margin": args.combat_margin,
                                  "checkpoint_sha256": hashlib.sha256(Path(args.combat_value).read_bytes()).hexdigest(),
                                  "exported_sha256": hashlib.sha256((out / "combat_value.bin").read_bytes()).hexdigest()}
    if args.guards:
        from .guards import parse
        parse(args.guards)
        lines.append(f"guards {args.guards}")
        record["guards"] = args.guards
    if args.guards_low:
        from .guards import parse_low
        level, low = parse_low(args.guards_low)
        lines.append(f"guards_low {level} {args.guards_low.partition(':')[2]}")
        record["guards_low"] = args.guards_low
    (out / "policy.bundle").write_text("\n".join(lines) + "\n")
    (out / "bundle.json").write_text(json.dumps(record, indent=1) + "\n")
    print(json.dumps(record))


if __name__ == "__main__":
    main()
