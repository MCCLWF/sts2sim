#!/usr/bin/env python3
"""L2 check: new-run setup and act maps, simulator vs. the real game.

Runs `Sts2Oracle newrun` (the shipped game logic, headless) and
`sts2sim_cli newrun` on the same seeds/characters/ascensions and diffs the
JSON field by field. Floats are compared as float32.

Usage:
  tests/l2/compare_newrun.py [--seeds N] [--ascensions 0,5,10] [--verbose]
"""
import argparse
import json
import os
import random
import struct
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DOTNET = os.path.expanduser(os.environ.get("DOTNET", "~/.dotnet/dotnet"))
ORACLE = os.path.join(ROOT, "tools", "Sts2Oracle", "bin", "Release", "net9.0", "Sts2Oracle.dll")
CLI = os.path.join(ROOT, "build", "sts2sim_cli")
CHARACTERS = ["IRONCLAD", "SILENT", "REGENT", "NECROBINDER", "DEFECT"]
SEED_ALPHABET = "0123456789ABCDEFGHJKLMNPQRSTUVWXYZ"

# Save fields the simulator does not model (wall clock, platform, profile data).
IGNORED = {
    "/save/dailyTime", "/save/map_drawings", "/save/num_reloads", "/save/platform_type",
    "/save/pre_finished_room", "/save/run_time", "/save/save_time", "/save/schema_version",
    "/save/start_time", "/save/win_time",
}
IGNORED_PLAYER = {"discovered_cards", "discovered_enemies", "discovered_epochs", "discovered_potions",
                  "discovered_relics", "unlock_state"}


def f32(x):
    return struct.unpack("f", struct.pack("f", x))[0]


def ignored(path):
    if path in IGNORED:
        return True
    parts = path.split("/")
    # The oracle never enters the map, so the current act carries an empty
    # placeholder map; maps are compared separately under /maps.
    if len(parts) == 5 and parts[2] == "acts" and parts[4] == "saved_map":
        return True
    return len(parts) >= 5 and parts[2] == "players" and parts[4] in IGNORED_PLAYER


def diff(a, b, path, out):
    if ignored(path):
        return
    if isinstance(a, dict) and isinstance(b, dict):
        for k in sorted(set(a) | set(b)):
            p = path + "/" + k
            if ignored(p):
                continue
            if k not in b:
                out.append("%s: missing in sim (game: %s)" % (p, json.dumps(a[k])[:120]))
            elif k not in a:
                out.append("%s: extra in sim (%s)" % (p, json.dumps(b[k])[:120]))
            else:
                diff(a[k], b[k], p, out)
    elif isinstance(a, list) and isinstance(b, list):
        if len(a) != len(b):
            out.append("%s: length game=%d sim=%d" % (path, len(a), len(b)))
        for i, (x, y) in enumerate(zip(a, b)):
            diff(x, y, "%s/%d" % (path, i), out)
    elif isinstance(a, float) or isinstance(b, float):
        if not (isinstance(a, (int, float)) and isinstance(b, (int, float)) and f32(a) == f32(b)):
            out.append("%s: game=%r sim=%r" % (path, a, b))
    elif a != b:
        out.append("%s: game=%s sim=%s" % (path, json.dumps(a)[:120], json.dumps(b)[:120]))


def run(cmd):
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res.returncode != 0:
        sys.exit("command failed: %s\n%s" % (" ".join(cmd), res.stderr[-2000:]))
    return [json.loads(line) for line in res.stdout.splitlines() if line.strip()]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seeds", type=int, default=20)
    ap.add_argument("--ascensions", default="0,10")
    ap.add_argument("--characters", default=",".join(CHARACTERS))
    ap.add_argument("--rng-seed", type=int, default=1)
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    rnd = random.Random(args.rng_seed)
    seeds = ["".join(rnd.choice(SEED_ALPHABET) for _ in range(12)) for _ in range(args.seeds)]
    common = ["newrun", "--seeds", ",".join(seeds), "--characters", args.characters, "--ascensions", args.ascensions]
    game = run([DOTNET, ORACLE] + common)
    sim = run([CLI] + common)
    if len(game) != len(sim):
        sys.exit("run count mismatch: game %d sim %d" % (len(game), len(sim)))

    failed = 0
    for g, s in zip(game, sim):
        out = []
        diff(g, s, "", out)
        if out:
            failed += 1
            if failed <= 5 or args.verbose:
                print("MISMATCH seed=%s character=%s ascension=%s (%d diffs)" % (g["seed"], g["character"], g["ascension"], len(out)))
                for line in out[:12]:
                    print("   " + line)
    print("L2 newrun: %d runs, %d mismatched" % (len(game), failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
