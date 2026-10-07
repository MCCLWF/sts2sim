#!/usr/bin/env python3
"""Live capture differential: the CombatSolver mod's capture of a real game
combat versus the simulator's own state at the same decision.

`tools/CombatSolverModChecks combat` is `Sts2Oracle combat` (real game logic,
seeded random policy) with `EnvLiveCapture` run at every decision point. This
script replays the same actions in `sts2sim_cli combat --snapshots 1` and
compares, per decision:

  1. the ordinary L3 state line (the replay must be the same game), and
  2. the mod's captured Env snapshot against the simulator's snapshot, after
     replacing process-local pool indices with stable identities.

A capture refusal (the bridge's `live_*` errors) is counted per message. A
field difference means the bridge wrote a value the simulator does not hold at
that point, i.e. the mapping of a live field onto simulator state is wrong.

Usage:
  tests/live/live_capture_diff.py --seeds S1,S2 [--characters C1,C2] [--encounter E]
      [--relics R1,R2] [--deck CARD,...] [--ascension N] [--policy-seeds 1,2]
  tests/live/live_capture_diff.py --matrix encounters|relics|cards ...   (one job per entry)
"""
import argparse
import collections
import concurrent.futures
import json
import os
import re
import subprocess
import sys
import tempfile
import threading

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DOTNET = os.environ.get("DOTNET", os.path.expanduser("~/.dotnet/dotnet"))
CHECKS = os.path.join(ROOT, "tools", "CombatSolverModChecks", "bin", "Release", "net9.0", "CombatSolverModChecks.dll")
sys.path.insert(0, os.path.join(ROOT, "tools"))
sys.path.insert(0, os.path.join(ROOT, "tests", "l3"))
from differential.common import first_diff  # noqa: E402
import l3_combat  # noqa: E402
sys.path.insert(0, os.path.join(ROOT, "tests", "l4"))
import l4_run  # noqa: E402

PILES = ("hand", "draw", "discard", "exhaust", "play")
CHARACTERS = ("IRONCLAD", "SILENT", "DEFECT", "REGENT", "NECROBINDER")
# Relic extra slots that hold a pool reference instead of a scalar
# (game/Models/Relics/<Relic>.h). Everything else in `extra` is a plain value.
RELIC_REFERENCE_SLOTS = {
    "PEN_NIB": {1: "card"}, "MUSIC_BOX": {1: "card"}, "PAELS_LEGION": {1: "card"},
    "UNSETTLING_LAMP": {0: "card", 1: "power"}, "VAMBRACE": {0: "card"},
}


# Power extra slots holding a card reference (env/snapshot_validation.cpp).
POWER_REFERENCE_SLOTS = {"NIGHTMARE_POWER": {0: "card"}, "CURL_UP_POWER": {0: "card"}, "SWIPE_POWER": {0: "card"},
                         "GIGANTIFICATION_POWER": {1: "card"}}
# Monster extra slots holding a creature reference (game/Models/Monsters).
MONSTER_REFERENCE_SLOTS = {"QUEEN": {1: "creature"}}

# Differences that are not mapping errors, with the reason each is expected.
EXPECTED = (
    (r"\.relics\[ARCHAIC_TOOTH\]\.extra\[[01]\]$",
     "Orobas' option-display card snapshots; ArchaicTooth never reads them once owned (not transported)"),
    (r"\.relics\[PAELS_TOOTH\]\.paels_tooth_cards",
     "read only by AfterCombatEnd, after the outcome boundary where every search from a live root stops"),
    (r"^\$\.run\.events\[1\]",
     "the simulator's scratch instance for queries on canonical events (EventModel.IsAllowed); no live object"),
    (r"^\$\.run\.events\[0\]\.(description|custom_done_page|extra|vars|options|slippery_skipped_removals|"
     r"crystal_sphere_cells|crystal_sphere_revealed_order|healed_amount|rng)",
     "private state of the event instance EventSynchronizer retains: the capture transports its identity and "
     "lifecycle only. A finished event is replaced by BeginEvent before anything reads it; the parent of a running "
     "combat is consulted after the combat ends, beyond a combat root."),
    (r"^\$\.run\.(rest_site|treasure|merchant|rewards)\.",
     "record of a room the run has left (reset when the next room of that kind begins); not transported"),
    (r"\[WATERFALL_GIANT\]\.monster\.extra\[2\]$",
     "WaterfallGiant.IncrementBuildUpAnimationTrack only counts when TestMode is off; the oracle runs the game's "
     "TestMode (docs/audits/testmode.md) while the port follows the shipping branch. Read by ShouldFadeAfterDeath (visual)."),
    (r"\.relics\[UNSETTLING_LAMP\]\.extra\[1\]$",
     "UnsettlingLamp's doubled power is only read while its triggering card is being played. Afterwards the entry "
     "may name a power that has left combat; the port keeps that pool slot, the capture writes no reference."),
    (r"\[(PARAFRIGHT|EYE_WITH_TEETH|FOGMOG)\]\.monster\.machine\.follow_up\[\d+\]$",
     "IllusionPower builds a fresh REVIVE_MOVE state per death. The port's static state keeps the previous revive's "
     "follow-up between deaths and rewrites it in AfterDeath before it is read; the game has no object to hold it."),
)


def enum_names(enum):
    ids = open(os.path.join(ROOT, "content", "generated", "ids.h")).read()
    body = re.search(r"enum class " + enum + r"[^\{]*\{(.*?)\};", ids, re.S).group(1)
    return [name for name in re.findall(r"^\s*(\w+)\s*(?:=\s*\d+)?\s*,?\s*$", body, re.M) if name != "Count"]


NAMES = {kind: enum_names(enum) for kind, enum in (
    ("card", "CardId"), ("relic", "RelicId"), ("power", "PowerId"), ("monster", "MonsterId"),
    ("potion", "PotionId"), ("orb", "OrbId"))}


def name(kind, index):
    table = NAMES[kind]
    return table[index] if 0 <= index < len(table) else "%s#%d" % (kind, index)


def project(snapshot):
    """The reachable logical state with pool references replaced by identities."""
    state = snapshot["state"]
    pools = {key: {slot["index"]: slot["value"] for slot in state[key]["slots"]}
             for key in ("cards", "powers", "creatures", "relics", "potions", "orbs", "rewards", "afflictions")}
    run, combat, player_combat = state["run"], state["combat"], state["player_combat"]
    player = run["player"]

    def creature_id(ref):
        return None if ref == 0 else "creature:%d" % pools["creatures"][ref]["combat_id"]

    location = {}
    for index, ref in enumerate(player["deck"]):
        location.setdefault(ref, "deck:%d" % index)
    for pile, refs in zip(PILES, player_combat["piles"]):
        for index, ref in enumerate(refs):
            location[ref] = "%s:%d" % (pile, index)

    def card_id(ref):
        if ref == 0:
            return None
        if ref in location:
            return location[ref]
        card = pools["cards"][ref]
        return "unlisted:%s+%d" % (name("card", card["id"]), card["upgrade"])

    power_location = {}
    for creature in pools["creatures"].values():
        for index, ref in enumerate(creature["powers"]):
            power_location[ref] = "creature:%d/power:%d" % (creature["combat_id"], index)

    def relic_position(ref):
        return None if ref == 0 else "relic:%d" % player["relics"].index(ref)

    def card(ref):
        value = dict(pools["cards"][ref])
        value["_name"] = name("card", value["id"])
        # CardModel.BaseStarCost initializes this flag on first read. The game
        # reads it from UI and playability checks the replay does not perform;
        # base_star_cost itself is compared.
        value.pop("star_cost_set")
        for key in ("clone_of", "deck_version"):
            value[key] = card_id(value[key])
        value["current_target"] = creature_id(value["current_target"])
        value["bing_bong_skip_relic"] = relic_position(value["bing_bong_skip_relic"])
        instance = value["affliction_instance"]
        # uid is a creation counter used for identity only; the card link is
        # the back-reference to this very card.
        value["affliction_instance"] = None if instance == 0 else {
            key: item for key, item in pools["afflictions"][instance].items() if key not in ("uid", "card")}
        return value

    def power(ref):
        value = dict(pools["powers"][ref])
        value["_name"] = name("power", value["id"])
        for key in ("owner", "applier", "target"):
            value[key] = creature_id(value[key])
        extra = list(value["extra"])
        for slot in POWER_REFERENCE_SLOTS.get(value["_name"], {}):
            if extra[slot]:
                extra[slot] = card_id(extra[slot])
        value["extra"] = extra
        value["dampen_casters"] = [creature_id(item) for item in value["dampen_casters"]]
        value["dampen_downgraded_cards"] = [dict(item, card=card_id(item["card"]))
                                            for item in value["dampen_downgraded_cards"]]
        return value

    def creature(ref):
        value = dict(pools["creatures"][ref])
        value["powers"] = [power(item) for item in value["powers"]]
        value["powered_damage_this_turn"] = [dict(item, dealer=creature_id(item["dealer"]))
                                             for item in value["powered_damage_this_turn"]]
        monster = dict(value["monster"])
        value["_name"] = name("monster", monster["id"]) if monster["id"] else "PLAYER"
        # Registry indices are assigned per native process; the bridge binds
        # the machine by comparing every state against the same definition.
        machine = dict(monster["machine"])
        machine.pop("definition")
        monster["machine"] = machine
        next_move = dict(monster["next_move"])
        next_move.pop("stun_move")
        monster["next_move"] = next_move
        if not machine["exists"]:
            # MonsterModel.ResetStateMachine: the game drops the machine and
            # its history; the simulator's slot keeps them unread.
            monster["machine"] = {"exists": False}
        extra = list(monster["extra"])
        for slot in MONSTER_REFERENCE_SLOTS.get(value["_name"], {}):
            if extra[slot]:
                extra[slot] = creature_id(extra[slot])
        monster["extra"] = extra
        value["monster"] = monster
        return value

    def relic(ref):
        value = dict(pools["relics"][ref])
        value["_name"] = name("relic", value["id"])
        value["paels_tooth_cards"] = [card(item) for item in value["paels_tooth_cards"]]
        extra = list(value["extra"])
        for slot, kind in RELIC_REFERENCE_SLOTS.get(value["_name"], {}).items():
            if extra[slot]:
                extra[slot] = card_id(extra[slot]) if kind == "card" else power_location.get(extra[slot], "power:?")
        value["extra"] = extra
        return value

    def potion(ref):
        if ref == 0:
            return None
        value = dict(pools["potions"][ref])
        value["_name"] = name("potion", value["id"])
        return value

    def orb(ref):
        value = dict(pools["orbs"][ref])
        value["_name"] = name("orb", value["id"])
        return value

    player_view = dict(player)
    player_view["creature"] = creature_id(player["creature"])
    player_view["deck"] = [card(ref) for ref in player["deck"]]
    player_view["relics"] = [relic(ref) for ref in player["relics"]]
    player_view["potions"] = [potion(ref) for ref in player["potions"]]
    run_view = {key: value for key, value in run.items() if key != "player"}
    run_view["map"] = project_map(run["map"])
    run_view["combat_extra_rewards"] = [pools["rewards"][ref] for ref in run["combat_extra_rewards"]]
    rewards = dict(run["rewards"])
    rewards["rewards"] = [pools["rewards"][ref] for ref in rewards["rewards"]]
    run_view["rewards"] = rewards
    combat_view = dict(combat)
    for key in ("allies", "enemies", "escaped"):
        combat_view[key] = [creature(ref) for ref in combat[key]]
    for key in ("player_last_nondupe_attack_card", "player_previous_nondupe_attack_card"):
        combat_view[key] = card_id(combat[key])
    combat_view["active_card_play_power_captures"] = [
        dict(item, power=power_location.get(item["power"], "power:?"), card=card_id(item["card"]))
        for item in combat["active_card_play_power_captures"]]
    player_combat_view = dict(player_combat)
    player_combat_view["piles"] = {pile: [card(ref) for ref in refs] for pile, refs in zip(PILES, player_combat["piles"])}
    player_combat_view["pets"] = [creature_id(ref) for ref in player_combat["pets"]]
    player_combat_view["orbs"] = [orb(ref) for ref in player_combat["orbs"]]
    return {"decision": snapshot["decision"], "run": run_view, "player": player_view, "combat": combat_view,
            "player_combat": player_combat_view, "card_selector_mode": state["card_selector_mode"]}


def project_map(source):
    """Map points by coordinate. Point indices follow each side's own
    construction order, and a set's slot layout only fixes the enumeration
    order of its members, which is what is compared here."""
    points = source["points"]

    def coord(index):
        return None if index < 0 else "%d,%d" % (points[index]["coord"]["col"], points[index]["coord"]["row"])

    def members(collection):
        return [coord(slot["value"]) for slot in collection["slots"] if slot.get("used", True)]

    # Generation leaves detached points in the simulator's array; only points
    # reachable from the grid, the fixed points and their edges exist in a map.
    reachable, pending = set(), [index for column in source["grid"] for index in column]
    pending += [source[key] for key in ("start", "boss", "second_boss")]
    pending += [slot["value"] for slot in source["start_points"]["slots"] if slot.get("used", True)]
    while pending:
        index = pending.pop()
        if index < 0 or index in reachable:
            continue
        reachable.add(index)
        for key in ("children", "parents"):
            pending += [slot["value"] for slot in points[index][key]["slots"] if slot.get("used", True)]
    view = {key: value for key, value in source.items() if key not in ("points", "grid", "start", "boss", "second_boss", "start_points")}
    view["points"] = {coord(index): dict({key: value for key, value in points[index].items() if key not in ("coord", "children", "parents")},
                                         children=members(points[index]["children"]), parents=members(points[index]["parents"]))
                      for index in sorted(reachable)}
    view["grid"] = [[coord(index) for index in column] for column in source["grid"]]
    for key in ("start", "boss", "second_boss"):
        view[key] = coord(source[key])
    view["start_points"] = members(source["start_points"])
    return view


def all_diffs(live, sim, path, out, limit=40):
    """Every differing leaf, with model names in the path so reports aggregate."""
    if len(out) >= limit:
        return
    if type(live) is not type(sim):
        out.append((path, live, sim))
    elif isinstance(live, dict):
        for key in list(live) + [key for key in sim if key not in live]:
            if key == "_name":
                continue
            if key not in live or key not in sim:
                out.append((path + "." + key, live.get(key, "<missing>"), sim.get(key, "<missing>")))
            else:
                all_diffs(live[key], sim[key], path + "." + key, out, limit)
    elif isinstance(live, list):
        if len(live) != len(sim):
            out.append((path + ".length", len(live), len(sim)))
        for index, (left, right) in enumerate(zip(live, sim)):
            label = left.get("_name") if isinstance(left, dict) and "_name" in left else None
            all_diffs(left, right, "%s[%s]" % (path, label if label else index), out, limit)
    elif live != sim:
        out.append((path, live, sim))


def run_checked(cmd, timeout):
    proc = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    return proc.returncode, proc.stdout, proc.stderr


def oracle_command(job, args):
    cmd = [DOTNET, CHECKS, "combat", "--seeds", args.seeds, "--characters", job.get("characters", args.characters),
           "--policy-seeds", args.policy_seeds, "--max-actions", str(args.max_actions)]
    if args.ascension:
        cmd += ["--ascension", str(args.ascension)]
    for key in ("encounter", "deck", "relics", "potion"):
        value = job.get(key) or getattr(args, key)
        if value:
            cmd += ["--" + key, value]
    if job.get("potion") or args.potion:
        cmd += ["--potion-target", "auto"]
    for key in ("starting_energy", "starting_stars"):
        value = job.get(key, getattr(args, key))
        if value is not None:
            cmd += ["--" + key.replace("_", "-"), str(value)]
    return cmd


def replay_command(cli, run):
    begin = run["begin"]
    cmd = [cli, "combat", "--seed", begin["seed"], "--character", begin["character"],
           "--ascension", str(begin.get("ascension", 0)), "--coord", "%d,%d" % tuple(run["coord"]),
           "--actions", ",".join(run["actions"]) or "-", "--snapshots", "1"]
    if run.get("encounter"):
        cmd += ["--encounter", run["encounter"]]
    for key in ("deck", "relics", "potion"):
        if begin.get(key):
            cmd += ["--" + key, begin[key]]
    if begin.get("potion"):
        cmd += ["--potion-target", begin.get("potion_target", "auto")]
    for key in ("starting_energy", "starting_stars"):
        if begin.get(key) is not None:
            cmd += ["--" + key.replace("_", "-"), str(begin[key])]
    return cmd


def evaluate(job, args):
    """One oracle process (all seeds x characters x policies of a job)."""
    result = {"job": job, "runs": 0, "states": 0, "captured": 0, "identical": 0, "expected": 0, "problems": []}
    code, out, err = run_checked(oracle_command(job, args), args.timeout)
    lines = out.splitlines()
    try:
        runs = l3_combat.parse_runs(lines)
    except ValueError as exc:
        # The oracle could not reach a first decision with this setup.
        result["problems"].append(("setup", "oracle: %s (exit %d)" % (exc, code)))
        return result
    if code != 0:
        result["problems"].append(("oracle", "exit %d: %s" % (code, err.strip().splitlines()[-1:])))
    for run in runs:
        result["runs"] += 1
        label = "%s/%s/p%s" % (run["begin"]["seed"], run["begin"]["character"], run["begin"]["policy_seed"])
        code, out, _ = run_checked(replay_command(args.cli, run), args.timeout)
        sim_states = []
        for line in out.splitlines():
            event = json.loads(line)
            if event["event"] == "state":
                sim_states.append(event)
            elif event["event"] == "error" and sim_states:
                result["problems"].append(("replay", "%s: %s" % (label, event["message"])))
        oracle_states = [event for event in run["observations"] if event["event"] == "state"]
        for index, oracle_state in enumerate(oracle_states):
            result["states"] += 1
            live = oracle_state.get("live") or {}
            if index >= len(sim_states):
                result["problems"].append(("setup" if index == 0 else "replay",
                                           "%s: simulator state %d missing" % (label, index)))
                break
            diff = first_diff(oracle_state["state"], sim_states[index]["state"])
            if diff:
                result["problems"].append(("l3", "%s state %d: %s" % (label, index, diff[0])))
                break
            if "error" in live:
                result["problems"].append(("capture", live["error"]))
                continue
            result["captured"] += 1
            diffs = []
            all_diffs(project(live["snapshot"]), project(sim_states[index]["snapshot"]), "$", diffs)
            unexpected = [item for item in diffs if not any(re.search(pattern, item[0]) for pattern, _ in EXPECTED)]
            result["expected"] += len(diffs) - len(unexpected)
            if not unexpected:
                result["identical"] += 1
            for path, left, right in unexpected:
                result["problems"].append(("field", path, json.dumps(left)[:160], json.dumps(right)[:160],
                                           "%s state %d" % (label, index)))
    return result


def evaluate_run_job(job, args):
    """Full runs (Sts2Oracle `run`): every combat decision of every room."""
    result = {"job": job, "runs": 0, "states": 0, "captured": 0, "identical": 0, "expected": 0, "problems": []}
    cmd = [DOTNET, CHECKS, "run", "--seeds", job["seeds"], "--characters", job["characters"],
           "--policy-seeds", args.policy_seeds, "--max-decisions", str(args.max_decisions),
           "--policy-version", "coverage-v3"]
    if args.ascension:
        cmd += ["--ascension", str(args.ascension)]
    if args.no_neow:
        cmd += ["--no-neow", "1"]
    if args.hp:
        cmd += ["--hp", str(args.hp)]
    for key in ("deck", "relics"):
        value = job.get(key) or getattr(args, key)
        if value:
            cmd += ["--" + key, value]
    if args.stop:
        cmd += ["--stop", args.stop]
    with tempfile.TemporaryFile("w+") as trace:
        oracle_process = subprocess.run(cmd, stdout=trace, stderr=subprocess.PIPE, text=True, timeout=args.timeout)
        code = oracle_process.returncode
        trace.seek(0)
        try:
            runs = l4_run.parse_runs(trace)
        except ValueError as exc:
            result["problems"].append(("setup", "oracle: %s (exit %d)" % (exc, code)))
            return result
    if code != 0:
        result["problems"].append(("oracle", "exit %d: %s" % (code, oracle_process.stderr.strip().splitlines()[-1:])))
    for run in runs:
        result["runs"] += 1
        begin = run["begin"]
        label = "%s/%s/p%s" % (begin["seed"], begin["character"], begin["policy_seed"])
        replay = [args.cli, "replay", "--seed", begin["seed"], "--character", begin["character"],
                  "--ascension", str(begin.get("ascension", 0)), "--snapshots", "1"]
        if not begin.get("neow", True):
            replay += ["--no-neow", "1"]
        for key in ("hp", "deck", "relics", "stop"):
            if begin.get(key):
                replay += ["--" + key, str(begin[key])]
        proc = subprocess.Popen(replay, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
        writer = threading.Thread(target=lambda: (proc.stdin.write("\n".join(run["actions"]) + "\n"), proc.stdin.close()))
        writer.start()
        index = 0
        for line in proc.stdout:
            event = json.loads(line)
            if event["event"] == "error":
                result["problems"].append(("replay", "%s: %s" % (label, event["message"])))
            if event["event"] != "decision":
                continue
            if index >= len(run["decisions"]):
                result["problems"].append(("replay", "%s: extra simulator decision %d" % (label, index)))
                break
            oracle = run["decisions"][index]
            index += 1
            if oracle.get("kind") != event.get("kind"):
                result["problems"].append(("l4", "%s decision %d: %s vs %s" % (label, index - 1, oracle.get("kind"), event.get("kind"))))
                break
            if oracle.get("kind") != "combat":
                continue
            result["states"] += 1
            if first_diff(oracle["combat"], event["combat"]):
                result["problems"].append(("l4", "%s decision %d: combat state differs" % (label, index - 1)))
                break
            live = oracle.get("live") or {}
            if "error" in live:
                result["problems"].append(("capture", live["error"]))
                continue
            result["captured"] += 1
            diffs = []
            all_diffs(project(live["snapshot"]), project(event["snapshot"]), "$", diffs)
            unexpected = [item for item in diffs if not any(re.search(pattern, item[0]) for pattern, _ in EXPECTED)]
            result["expected"] += len(diffs) - len(unexpected)
            if not unexpected:
                result["identical"] += 1
            for path, left, right in unexpected:
                result["problems"].append(("field", path, json.dumps(left)[:160], json.dumps(right)[:160],
                                           "%s decision %d" % (label, index - 1)))
        proc.stdout.close()
        writer.join()
        code = proc.wait()
        if code != 0:
            result["problems"].append(("replay", "%s: simulator exit %d" % (label, code)))
        if index != len(run["decisions"]):
            result["problems"].append(("replay", "%s: compared %d/%d decisions" % (label, index, len(run["decisions"]))))
    return result


def matrix_jobs(kind, args):
    content = json.load(open(os.path.join(ROOT, "data", "content.json")))
    models = [m for m in content["models"] if not m["props"].get("IsMock", False)]
    entry = lambda m: m["id"].split(".", 1)[1]
    if kind == "encounters":
        return [{"encounter": entry(m)} for m in models if m["kind"] == "EncounterModel"]
    if kind == "relics":
        return [{"relics": entry(m)} for m in models if m["kind"] == "RelicModel"]
    if kind == "potions":
        return [{"potion": entry(m)} for m in models if m["kind"] == "PotionModel"]
    if kind == "cards":
        # One small deck per card: the card, its upgrade, and the owning
        # character's basics, with enough energy to play it most turns.
        owners = {"CARD_POOL.%s_CARD_POOL" % name: name for name in CHARACTERS}
        jobs = []
        for index, model in enumerate(m for m in models if m["kind"] == "CardModel"):
            props = model["props"]
            # Mad Science only exists configured by Tinker Time (tests/l3 --probe-mad-science).
            if (props.get("MultiplayerConstraint") == "MultiplayerOnly" or props.get("Pool") == "CARD_POOL.DEPRECATED_CARD_POOL"
                    or entry(model) == "MAD_SCIENCE"):
                continue
            character = owners.get(props.get("Pool"), CHARACTERS[index % len(CHARACTERS)])
            card = entry(model)
            copies = [card, card + "+1"] if props.get("MaxUpgradeLevel", 0) > 0 else [card, card]
            basics = ["STRIKE_" + character] * 3 + ["DEFEND_" + character] * 2
            jobs.append({"deck": ",".join(copies + basics), "characters": character, "starting_energy": 20, "card": card})
        return jobs
    raise ValueError("unknown matrix " + kind)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seeds", required=True)
    ap.add_argument("--characters", default="IRONCLAD")
    ap.add_argument("--policy-seeds", default="1")
    ap.add_argument("--ascension", type=int, default=0)
    ap.add_argument("--encounter")
    ap.add_argument("--deck")
    ap.add_argument("--relics")
    ap.add_argument("--starting-energy", type=int)
    ap.add_argument("--starting-stars", type=int)
    ap.add_argument("--potion", help="procure and use this potion before the first decision")
    ap.add_argument("--matrix", choices=("encounters", "relics", "cards", "potions"), help="one job per vanilla entry of this kind")
    ap.add_argument("--allow-setup-failures", action="store_true",
                    help="list combats the oracle or the replay cannot set up (an obtain-time choice, a multi-step "
                         "reward) without failing; nothing is claimed about them")
    ap.add_argument("--only", help="comma-separated matrix entries to keep")
    ap.add_argument("--runs", action="store_true",
                    help="play whole runs (Sts2Oracle `run`, coverage-v3 policy) and compare every combat decision")
    ap.add_argument("--no-neow", action="store_true")
    ap.add_argument("--hp", type=int, default=0, help="with --runs: start with this max HP so runs reach later acts")
    ap.add_argument("--stop", choices=("before-act1-boss", "after-act1", "after-act2", "victory"))
    ap.add_argument("--max-decisions", type=int, default=3000)
    ap.add_argument("--max-actions", type=int, default=300)
    ap.add_argument("--jobs", type=int, default=max(1, (os.cpu_count() or 2) // 2))
    ap.add_argument("--timeout", type=float, default=300)
    ap.add_argument("--cli", default=os.path.join(ROOT, "build", "sts2sim_cli"))
    ap.add_argument("--examples", type=int, default=2, help="examples printed per distinct finding")
    ap.add_argument("--result-json")
    args = ap.parse_args()

    if args.runs:
        # One oracle process per (seed, character): a run is long and its trace is large.
        jobs = [{"seeds": seed, "characters": character} for seed in args.seeds.split(",")
                for character in args.characters.split(",")]
    else:
        jobs = matrix_jobs(args.matrix, args) if args.matrix else [{}]
    if args.only:
        keep = set(args.only.split(","))
        jobs = [job for job in jobs if set(job.values()) & keep]
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = list(pool.map(lambda job: guarded(job, args), jobs))

    totals = collections.Counter()
    refusals, fields, other = collections.Counter(), collections.defaultdict(list), collections.Counter()
    setup = collections.defaultdict(set)
    refusal_jobs = collections.defaultdict(set)
    for result in results:
        for key in ("runs", "states", "captured", "identical", "expected"):
            totals[key] += result[key]
        job = result["job"]
        tag = ",".join("%s=%s" % (key, job[key]) for key in ("card", "encounter", "relics", "potion", "seeds", "characters")
                       if key in job) or "-"
        for problem in result["problems"]:
            if problem[0] == "capture":
                refusals[problem[1]] += 1
                refusal_jobs[problem[1]].add(tag)
            elif problem[0] == "field":
                fields[problem[1]].append(problem[2:] + (tag,))
            elif problem[0] == "setup":
                setup[tag].add(problem[1])
            else:
                other["%s: %s [%s]" % (problem[0], problem[1], tag)] += 1
    print("runs %(runs)d  decisions %(states)d  captured %(captured)d  identical %(identical)d"
          "  (expected differences: %(expected)d)" % totals)
    if refusals:
        print("\ncapture refusals (%d decisions):" % sum(refusals.values()))
        for message, count in sorted(refusals.items(), key=lambda item: -item[1]):
            print("  %6d  %s   [%s]" % (count, message, "; ".join(sorted(refusal_jobs[message])[:3])))
    if fields:
        print("\nfield differences (live capture vs simulator):")
        for path, items in sorted(fields.items(), key=lambda item: -len(item[1])):
            print("  %6d  %s" % (len(items), path))
            for left, right, where, tag in items[:args.examples]:
                print("            live=%s sim=%s  (%s; %s)" % (left, right, where, tag))
    if setup:
        print("\nnot set up by the combat harness (%d jobs): %s" % (len(setup), " ".join(sorted(setup))))
    if other:
        print("\nother problems:")
        for message, count in sorted(other.items(), key=lambda item: -item[1]):
            print("  %6d  %s" % (count, message))
    if args.result_json:
        with open(args.result_json, "w") as handle:
            json.dump({"totals": totals, "refusals": refusals, "refusal_jobs": {k: sorted(v) for k, v in refusal_jobs.items()},
                       "fields": {k: v[:5] for k, v in fields.items()}, "field_counts": {k: len(v) for k, v in fields.items()},
                       "setup_failures": {k: sorted(v) for k, v in setup.items()},
                       "other": other}, handle, indent=1)
    clean = (not refusals and not fields and not other and totals["captured"] and totals["identical"] == totals["captured"]
             and (args.allow_setup_failures or not setup))
    print("\n" + (("all compared live captures match the simulator; %d setup jobs excluded" % len(setup)
                    if setup else "live capture matches the simulator at every decision") if clean else "live capture differential FAILED"))
    return 0 if clean else 1


def guarded(job, args):
    try:
        return evaluate_run_job(job, args) if args.runs else evaluate(job, args)
    except subprocess.TimeoutExpired:
        return {"job": job, "runs": 0, "states": 0, "captured": 0, "identical": 0, "expected": 0,
                "problems": [("oracle", "timeout")]}


if __name__ == "__main__":
    sys.exit(main())
