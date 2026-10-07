"""Run the derived Mod in an isolated game snapshot, preserving its diagnostic log."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import threading


def verify_output(directory, mode, verify_branches=False, expect_rejection=False, expect_monster_rejection=False, expect_event_rejection=False):
    log = (directory / "game.log").read_text()
    launcher = (directory / "launcher.txt").read_text()
    if '"status": "Passed"' not in launcher or "UNATTENDED_INSTANCE_REMOVED" not in launcher:
        raise SystemExit("missing successful scenario or instance cleanup evidence")
    if "[Sts2EnvCompare] READY" not in log:
        raise SystemExit("missing Env initializer evidence")
    if "Calling initializer method of type CombatSolver.Entry for CombatSolverEnv" not in log:
        raise SystemExit("native initializer did not run from derived assembly")
    if "[Sts2EnvCompare] FAILED" in log:
        raise SystemExit("Env comparison reported failure")
    if mode == "both" and not any((verify_branches, expect_rejection, expect_monster_rejection, expect_event_rejection)) and "native_plan_env_check=True" not in log:
        raise SystemExit("missing successful native-plan Env replay evidence")
    if verify_branches:
        if "LIVE_CARD_COMPONENT" not in log or "source=live immutable=true full_root=false" not in log:
            raise SystemExit("missing immutable live card component capture evidence")
        if "LIVE_MONSTER_COMPONENT" not in log or "source=live scope=monster_rng_and_fsm immutable=true full_root=false" not in log:
            raise SystemExit("missing immutable live monster RNG/FSM capture evidence")
        if "BRANCH_ENV_ROOT_MONSTERS_MATCH scope=monster_rng_and_fsm source=live_vs_seed_snapshot_import full_root=false" not in log:
            raise SystemExit("missing root monster RNG/FSM agreement evidence")
        if "BRANCH_ENV_CHECK" not in log or "transitions=1 " not in log:
            raise SystemExit("missing a native search branch verified through Env")
        if "scope=structural_and_combat_rng" not in log:
            raise SystemExit("missing complete combat RNG verification evidence")
        if "root_source=seed_snapshot_import" not in log:
            raise SystemExit("missing complete snapshot import boundary evidence")
        if "BRANCH_ENV_DISABLED" in log:
            raise SystemExit("branch verifier disabled Env; inspect the first divergence")
        if "BRANCH_ENV_SUMMARY" not in log or "env_disabled=False" not in log:
            raise SystemExit("missing completed search branch verification summary")
    if expect_rejection:
        if "root_mismatch:hand,hand_states" not in log or "env_disabled=True" not in log:
            raise SystemExit("missing explicit rejection of the injected native root")
        if "ENV_BRANCH_WARNING shown=true" not in log:
            raise SystemExit("missing game UI warning for the rejected Env branch")
    if expect_monster_rejection:
        if "LIVE_MONSTER_COMPONENT" not in log or "root_monster_runtime_mismatch:1:monster.rng." not in log:
            raise SystemExit("missing frozen monster component and exact RNG root mismatch")
        if "LIVE_RUN_COMPONENT run_streams=12 player_streams=3" not in log or "source=live immutable=true full_root=false" not in log:
            raise SystemExit("missing immutable live run context and all RNG streams")
        if "env_disabled=True" not in log or "env_checked_transitions_root_cumulative=0" not in log:
            raise SystemExit("monster mismatch was not rejected before search verification")
        if "ENV_BRANCH_WARNING shown=true" not in log or "native_expanded=100" not in log:
            raise SystemExit("missing UI warning or continued Native search")
    if expect_event_rejection:
        for component in ("LIVE_CARD_COMPONENT", "LIVE_MONSTER_COMPONENT", "LIVE_RUN_COMPONENT run_streams=12 player_streams=3",
                          "LIVE_CREATURE_COMPONENT", "LIVE_INVENTORY_COMPONENT", "LIVE_COMBAT_COMPONENT",
                          "LIVE_PROFILE_COMPONENT source=run", "LIVE_PROFILE_COMPONENT source=player"):
            if component not in log:
                raise SystemExit(f"missing owned actual component: {component}")
        if "full_veteran_compatible=False" not in log or not (
            "live_event_pending:1" in log or "live_event_bookkeeping_pending:1" in log):
            raise SystemExit("missing captured partial profile or unfinished event refusal")
        if "env_disabled=True" not in log or "env_checked_transitions_root_cumulative=0" not in log:
            raise SystemExit("stale event was not rejected before search verification")
        if ("ENV_UNAVAILABLE_WARNING shown=true" not in log
                or "ENV_MODE_UNAVAILABLE" not in log or "root_source=live_capture env_imported=false" not in log
                or "native_expanded=100" not in log):
            raise SystemExit("missing stale-event UI warning or continued Native search")
    print(f"derived smoke passed; evidence: {directory}")


def main():
    parser = argparse.ArgumentParser()
    steam = Path.home() / "snap/steam/common/.local/share/Steam"
    parser.add_argument("--game-root", type=Path, default=steam / "steamapps/common/Slay the Spire 2")
    parser.add_argument("--ritsu-root", type=Path, default=steam / "steamapps/workshop/content/2868840/3747602295")
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--mode", choices=("native", "env", "both"), default="both")
    parser.add_argument("--verify-only", action="store_true", help="Check retained output without starting another game")
    parser.add_argument("--verify-branches", action="store_true",
                        help="Keep natural enemy HP, stop after first search and require branch agreement")
    parser.add_argument("--expect-branch-rejection", action="store_true",
                        help="Keep default injected Strike and assert root rejection and UI warning")
    parser.add_argument("--expect-monster-root-rejection", action="store_true",
                        help="Natural root: assert the live monster RNG differs from the seed template and is refused")
    parser.add_argument("--expect-event-bookkeeping-rejection", action="store_true",
                        help="Natural isolated root: capture partial profile, then assert an unfinished event is refused with UI warning")
    parser.add_argument("--memory-reservation-mib", type=int, default=4096)
    args = parser.parse_args()
    if any((args.verify_branches, args.expect_branch_rejection, args.expect_monster_root_rejection, args.expect_event_bookkeeping_rejection)) and args.mode != "both":
        parser.error("branch verification requires --mode both")
    if sum((args.verify_branches, args.expect_branch_rejection, args.expect_monster_root_rejection, args.expect_event_bookkeeping_rejection)) > 1:
        parser.error("branch acceptance and rejection are separate fixtures")
    if args.memory_reservation_mib <= 0:
        parser.error("--memory-reservation-mib must be positive")
    if args.verify_only:
        verify_output(args.out, args.mode, args.verify_branches, args.expect_branch_rejection, args.expect_monster_root_rejection, args.expect_event_bookkeeping_rejection)
        return
    project = Path(__file__).resolve().parent
    repo = project.parent.parent
    upstream = repo / "external/CombatSolver"
    args.out.mkdir(parents=True, exist_ok=True)
    local = repo / ".local"
    local.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="derived-smoke-", dir=local) as directory:
        fixture = Path(directory)
        game = fixture / "source"
        game.mkdir()
        for source in args.game_root.iterdir():
            if source.name != "mods":
                (game / source.name).symlink_to(source, target_is_directory=source.is_dir())
        mod = game / "mods/CombatSolverEnv"
        mod.mkdir(parents=True)
        dll = project / ".godot/mono/temp/bin/Release/CombatSolverEnv.dll"
        for source in (dll, project / "CombatSolverEnv.json", repo / "build/libsts2env_c.so",
                       upstream / "LICENSE", upstream / "THIRD_PARTY_NOTICES.md"):
            shutil.copy2(source, mod / source.name)
        # The upstream launcher requires these paths. Its placeholder manifest has no
        # executable content: the only loaded solver is the derived assembly above.
        harness = fixture / "harness"
        harness.mkdir()
        shutil.copy2(dll, harness / "CombatSolver.dll")
        (harness / "CombatSolver.json").write_text(json.dumps({
            "id": "SolverFixturePlaceholder", "name": "Isolated launcher placeholder",
            "version": "1.0.0", "has_dll": False, "has_pck": False,
            "affects_gameplay": False,
        }))
        instance = fixture.name
        runtime_log = upstream / ".local/headless-instances" / instance / "godot-headless.log"
        runtime_root = runtime_log.parent.resolve()
        host_root = Path(os.environ.get("COMBATSOLVER_HEADLESS_HOST_ROOT",
            str(Path(os.environ.get("XDG_STATE_HOME", str(Path.home() / ".local/state")))
                / "CombatSolver/headless-host-v1")))
        lease_path = host_root / "leases" / (hashlib.sha256(str(runtime_root).encode()).hexdigest() + ".json")
        peak_rss_kib = 0
        stop = threading.Event()

        def retain_log():
            nonlocal peak_rss_kib
            while not stop.wait(0.1):
                try:
                    shutil.copy2(runtime_log, args.out / "game.log")
                except FileNotFoundError:
                    pass  # Log is created on startup and removed by launcher cleanup.
                try:
                    lease = json.loads(lease_path.read_text())
                    if lease.get("instanceRoot") != str(runtime_root) or "gamePid" not in lease:
                        continue
                    proc = Path("/proc") / str(lease["gamePid"])
                    if (proc / "exe").resolve() != Path(lease["executable"]):
                        continue
                    for line in (proc / "status").read_text().splitlines():
                        if line.startswith("VmHWM:"):
                            peak_rss_kib = max(peak_rss_kib, int(line.split()[1]))
                except FileNotFoundError:
                    pass  # The exact owned process/lease may exit between reads.

        watcher = threading.Thread(target=retain_log)
        watcher.start()
        try:
            env = dict(os.environ, STS2ENV_COMPARE_MODE=args.mode,
                       STS2ENV_COMPARE_ITERATIONS="16", STS2ENV_COMPARE_BEAM_WIDTH="4",
                       STS2ENV_COMPARE_DEPTH="3")
            with (args.out / "launcher.txt").open("w") as output:
                command = [
                    str(upstream / "tools/run-unattended-test.sh"),
                    "--scenario-id", "MCC-DERIVED-SMOKE", "--seed", "DUAL001",
                    "--character-id", "IRONCLAD", "--encounter-id", "CULTISTS_NORMAL",
                    "--force-short-search-only", "--short-search-budget-override-milliseconds", "1500",
                    "--cleanup-instance-on-exit", "--headless-instance", instance,
                    "--sts2-game-root", str(game), "--ritsu-workshop-root", str(args.ritsu_root),
                    "--combat-solver-build-dir", str(harness),
                    "--headless-memory-reservation-mib", str(args.memory_reservation_mib),
                ]
                if any((args.verify_branches, args.expect_branch_rejection, args.expect_monster_root_rejection, args.expect_event_bookkeeping_rejection)):
                    # The upstream defaults inject one Strike after TurnStarted.
                    # An explicit empty list preserves the natural root. Cap this
                    # fixture at the branch boundary; terminal adapters are separate.
                    command += ["--enemy-current-hp", "999999999",
                                "--search-max-expanded-nodes-for-test", "100",
                                "--search-max-degree-of-parallelism-for-test", "8",
                                "--stop-after-initial-solver-result-assertion"]
                    if args.verify_branches or args.expect_monster_root_rejection or args.expect_event_bookkeeping_rejection:
                        command += ["--cards-json", "[]"]
                result = subprocess.run(command, env=env, stdout=output, stderr=subprocess.STDOUT)
            if result.returncode:
                raise SystemExit(f"derived smoke failed ({result.returncode}); see {args.out / 'launcher.txt'}")
        finally:
            stop.set()
            watcher.join()
            (args.out / "memory.json").write_text(json.dumps({
                "reservation_mib": args.memory_reservation_mib,
                "sampled_peak_rss_kib": peak_rss_kib,
            }, indent=2) + "\n")
        if peak_rss_kib > args.memory_reservation_mib * 1024:
            raise SystemExit("fixture exceeded its declared RSS reservation; increase it before retrying")
    verify_output(args.out, args.mode, args.verify_branches, args.expect_branch_rejection, args.expect_monster_root_rejection, args.expect_event_bookkeeping_rejection)


if __name__ == "__main__":
    main()
