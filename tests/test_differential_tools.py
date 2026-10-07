#!/usr/bin/env python3
"""Regression tests for strict L3/L4 trace and process failure handling.

These fixtures exercise the public parse/replay/compare APIs only. They use
saved-in-memory protocol events and tiny local fake CLIs, so no game install or
native simulator build is needed.
"""
import json
import io
from contextlib import redirect_stderr
import os
from pathlib import Path
import signal
import sys
import tempfile
from types import SimpleNamespace
from unittest.mock import patch
import unittest


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests" / "l3"))
sys.path.insert(0, str(ROOT / "tests" / "l4"))
sys.path.insert(0, str(ROOT / "tests"))
sys.path.insert(0, str(ROOT / "tools"))

import l3_combat  # noqa: E402
import l4_run  # noqa: E402
from differential.common import native_events, result_base  # noqa: E402


def jsonl(*events):
    return [json.dumps(event) for event in events]


def l3_begin(**extra):
    return {"event": "begin", "seed": "TESTSEED", "character": "IRONCLAD",
            "policy_seed": 1, **extra}


def l3_enter():
    return {"event": "enter", "coord": [0, 0], "encounter": "CULTISTS_NORMAL"}


def l3_state(turn=1, hand=None):
    state = {"turn": turn}
    if hand is not None:
        state["player"] = {"hand": hand}
    return {"event": "state", "state": state}


def l4_begin(**extra):
    return {"event": "begin", "seed": "TESTSEED", "character": "IRONCLAD",
            "policy_seed": 1, **extra}


def l4_decision(kind="map", index=0):
    return {"event": "decision", "kind": kind, "options": {"index": index},
            "run": {"act": 1, "total_floor": 0, "room": "map"}}


def l4_combat_decision():
    # Legacy L4 oracle captures put combat state directly under `combat`.
    # Unlike map/reward decisions, this event has no `options` or `run` key.
    return {"event": "decision", "kind": "combat",
            "combat": {"round": 1, "turn": 1}}


def l4_action(kind="map", index=0):
    if kind == "map":
        return {"event": "action", "kind": kind, "coord": [index, 0]}
    return {"event": "action", "kind": kind, "index": index}


def l4_end(reason="death"):
    return {"event": "end", "reason": reason, "run": {"act": 1, "total_floor": 1}}


class TraceProtocolTests(unittest.TestCase):
    def test_l4_trace_policy_option_rejects_before_capture_or_replay(self):
        for version in ("legacy-v1", "coverage-v2", "coverage-v3"):
            with self.subTest(version=version), \
                 patch.object(sys, "argv", ["l4_run.py", "--trace", "missing.oracle.jsonl", "--policy-version", version]), \
                 patch.object(l4_run, "run_oracle") as capture, \
                 patch.object(l4_run, "evaluate_runs") as evaluate, redirect_stderr(io.StringIO()) as stderr:
                with self.assertRaises(SystemExit) as caught:
                    l4_run.main()
                self.assertEqual(2, caught.exception.code)
                self.assertIn("not supported with frozen --trace", stderr.getvalue())
                capture.assert_not_called()
                evaluate.assert_not_called()

    def test_l4_potion_actions_and_capture_policy_identity(self):
        for event, command in (
            ({"type": "use_potion", "slot": 2, "target": None}, "potion 2 -1"),
            ({"type": "use_potion", "slot": 0, "target": 1}, "potion 0 1"),
            ({"type": "discard_potion", "slot": 1}, "discard 1"),
        ):
            action = dict(event, event="action", kind="combat")
            run, = l4_run.parse_runs(jsonl(l4_begin(policy_version="coverage-v2"),
                                             l4_combat_decision(), action, l4_end()))
            self.assertEqual([command], run["actions"])
            l4_run.require_policy_version([run], "coverage-v2")
            with self.assertRaisesRegex(ValueError, "policy_version"):
                l4_run.require_policy_version([run], "legacy-v1")
        legacy = {"begin": l4_begin()}
        l4_run.require_policy_version([legacy], "legacy-v1")
        with self.assertRaisesRegex(ValueError, "policy_version"):
            l4_run.require_policy_version([legacy], "coverage-v2")
        with self.assertRaisesRegex(ValueError, "unsupported policy_version"):
            l4_run.parse_runs(jsonl(l4_begin(policy_version="unknown")))

    def test_v3_reward_rejects_disabled_reroll_and_missing_native_evidence(self):
        # This cached option list is the native DAA098F90159CD9 failure at
        # decision 331. Native CardReward.Reroll sets CanReroll=false.
        decision = l4_decision("card_select")
        decision["options"] = {"cards": ["BOOT_SEQUENCE", "COLD_SNAP+1", "REFRACT"],
                               "min": 0, "max": 1, "can_skip": True,
                               "alternatives": ["Skip", "REROLL"], "can_reroll": True}
        valid = {"event": "action", "kind": "card_select", "indices": [3],
                 "legal_reward_indices": [0, 1, 2, 3], "native_reward_can_reroll": False}
        run, = l4_run.parse_runs(jsonl(l4_begin(policy_version="coverage-v3"), decision, valid, l4_end()))
        self.assertEqual(["select 1 3"], run["actions"])
        for changes in ({"indices": [4]}, {"legal_reward_indices": [0, 1, 2, 3, 4]},
                        {"native_reward_can_reroll": None}, {"legal_reward_indices": None},
                        {"indices": [0, 0]}, {"indices": [False]}):
            with self.subTest(changes=changes), self.assertRaisesRegex(ValueError, "coverage-v3 reward"):
                l4_run.parse_runs(jsonl(l4_begin(policy_version="coverage-v3"), decision,
                                       dict(valid, **changes), l4_end()))
        # Old captures remain parseable as raw historical evidence; their
        # unavailable action must still fail actual Env execution.
        legacy = {"event": "action", "kind": "card_select", "indices": [4]}
        run, = l4_run.parse_runs(jsonl(l4_begin(policy_version="coverage-v2"), decision, legacy, l4_end()))
        self.assertEqual(["select 1 4"], run["actions"])

    def test_native_malformed_payload_preserves_valid_prefix(self):
        for bad in ({"event": "state"}, {"event": "decision", "kind": "card_select"},
                    {"event": "error"}):
            with self.subTest(event=bad):
                prefix, error = native_events("\n".join(jsonl(l3_state(), bad)),
                                              {"state", "decision", "error"})
                self.assertEqual([l3_state()], prefix)
                self.assertEqual("native_error", error["status"])
        for kind in ("map", "rewards", "shop"):
            actual = {"event": "decision", "kind": kind, "options": []}
            prefix, error = native_events(json.dumps(actual), {"decision"})
            self.assertEqual([actual], prefix)
            self.assertIsNone(error)
        for override in ({"starting_energy": 0}, {"starting_stars": 0}, {"potion": "BLOCK_POTION"}):
            row = result_base("L3", {"begin": l3_begin(**override)}, 1)
            self.assertEqual("injected", row["scope"])

    def test_l3_rejects_empty_and_incomplete_combat_traces(self):
        cases = [
            ("empty", [], "trace contains no combats"),
            ("missing-end", jsonl(l3_begin(), l3_enter(), l3_state()), "missing its end"),
        ]
        for name, lines, message in cases:
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, message):
                l3_combat.parse_runs(lines)

    def test_l3_rejects_duplicate_begin_bad_counts_and_unknown_action_kinds(self):
        cases = [
            ("duplicate-begin", jsonl(l3_begin(), l3_enter(), l3_state(), l3_begin()),
             "before ending the previous one"),
            ("state-action-count", jsonl(l3_begin(), l3_enter(), l3_state(),
                                          {"event": "action", "type": "end_turn"},
                                          l3_state(2), {"event": "end"}),
             "state/action counts differ"),
            ("unknown-action-kind", jsonl(l3_begin(), l3_enter(), l3_state(),
                                           {"event": "action", "type": "block"},
                                           {"event": "end"}),
             "unknown combat action"),
            ("selection-action-count", jsonl(l3_begin(), l3_enter(), l3_state(),
                                             {"event": "action", "kind": "card_select", "indices": []},
                                             {"event": "end"}),
             "selection action has no preceding decision"),
        ]
        for name, lines, message in cases:
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, message):
                l3_combat.parse_runs(lines)

    def test_l4_rejects_empty_incomplete_duplicate_and_mismatched_traces(self):
        complete_prefix = (l4_begin(), l4_decision(), l4_action())
        cases = [
            ("empty", [], "trace contains no runs"),
            ("missing-end", jsonl(*complete_prefix), "empty or missing its end"),
            ("duplicate-begin", jsonl(*complete_prefix, l4_begin()),
             "another run before ending the previous one"),
            ("unanswered-decision", jsonl(l4_begin(), l4_decision(), l4_end()),
             "unanswered decision"),
            ("action-kind-mismatch", jsonl(l4_begin(), l4_decision(),
                                             l4_action("combat"), l4_end()),
             "no matching decision"),
        ]
        for name, lines, message in cases:
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, message):
                l4_run.parse_runs(lines)

    def test_l3_probe_is_reported_as_bounded_not_a_complete_combat(self):
        lines = jsonl(
            l3_begin(probe_card="BASH+1"), l3_enter(), l3_state(hand=["BASH+1"]),
            {"event": "action", "type": "play", "hand_index": 0,
             "target": None, "card": "BASH"},
            l3_state(2),
        )
        run, = l3_combat.parse_runs(lines)
        row = l3_combat.compare_run(run, run["observations"], None, None)

        self.assertEqual("probe_identical", row["status"])
        self.assertNotEqual("identical", row["status"])
        self.assertEqual(len(run["states"]), row["matched_states"])
        self.assertEqual(len(run["states"]), row["total_states"])
        self.assertEqual(len(run["observations"]), row["matched"])
        self.assertEqual({"probe_identical": 1}, l3_combat.summary([row])["statuses"])
        self.assertEqual(0, l3_combat.summary([row])["statuses"].get("identical", 0))

    def test_l3_required_probe_play_checks_completed_probe_and_its_upgrade(self):
        played_lines = jsonl(
            l3_begin(probe_card="BASH+1"), l3_enter(), l3_state(hand=["BASH+1"]),
            {"event": "action", "type": "play", "hand_index": 0,
             "target": None, "card": "BASH"},
            {"event": "end", "reason": "victory"},
        )
        played_run, = l3_combat.parse_runs(played_lines)
        played = l3_combat.compare_run(
            played_run, played_run["observations"], played_run["end"], None,
            required_probe_play="BASH")
        self.assertEqual("identical", played["status"])

        unplayed_lines = jsonl(
            l3_begin(probe_card="BASH+1"), l3_enter(), l3_state(hand=["STRIKE_IRONCLAD"]),
            {"event": "action", "type": "play", "hand_index": 0,
             "target": None, "card": "STRIKE_IRONCLAD"},
            {"event": "end", "reason": "victory"},
        )
        unplayed_run, = l3_combat.parse_runs(unplayed_lines)
        args = SimpleNamespace(cli="unused", ascension=0, deck=None, relics=None, potion=None,
                               starting_energy=None, starting_stars=None, take_special_rewards=False,
                               potion_target=None, require_probe_play="BASH")

        def fake_replay(*_positional, diagnostics=None, **_kwargs):
            diagnostics.update(observations=unplayed_run["observations"], process={}, error=None)
            return unplayed_run["states"], [], unplayed_run["end"], None

        with patch.object(l3_combat, "replay", side_effect=fake_replay):
            unplayed, = l3_combat.evaluate_runs(args, [unplayed_run])
        self.assertEqual("diverged", unplayed["status"])
        self.assertEqual("probe card was not played", unplayed["first"]["message"])

    def test_l3_unended_probe_requires_the_source_specified_upgrade(self):
        lines = jsonl(
            l3_begin(probe_card="BASH+1"), l3_enter(), l3_state(hand=["BASH"]),
            {"event": "action", "type": "play", "hand_index": 0,
             "target": None, "card": "BASH"},
            l3_state(2),
        )
        with self.assertRaisesRegex(ValueError, "probe card was not played"):
            l3_combat.parse_runs(lines)

    def test_l4_accepts_legacy_combat_decision_without_run_or_options(self):
        combat = l4_combat_decision()
        lines = jsonl(
            l4_begin(), combat,
            {"event": "action", "kind": "combat", "type": "end_turn"},
            {"event": "end", "reason": "death", "run": {"act": 1, "total_floor": 1}},
        )

        run, = l4_run.parse_runs(lines)
        self.assertEqual(combat, run["decisions"][0])
        self.assertNotIn("run", run["decisions"][0])
        self.assertNotIn("options", run["decisions"][0])
        row = l4_run.compare_run(run, run["decisions"], run["end"], None)

        self.assertEqual("identical", row["status"])
        self.assertEqual(1, row["matched"])
        self.assertEqual({"identical": 1}, l4_run.summary([row])["statuses"])

    def test_l4_max_decisions_prefix_is_never_identical(self):
        lines = jsonl(l4_begin(), l4_decision(), l4_action(), l4_end("max_decisions"))
        run, = l4_run.parse_runs(lines)
        row = l4_run.compare_run(run, run["decisions"], run["end"], None)

        self.assertEqual("truncated", row["status"])
        self.assertEqual(1, row["matched"])
        self.assertEqual(1, l4_run.summary([row])["statuses"]["truncated"])
        self.assertEqual(1, l4_run.exit_code([row], strict=False))

    def test_l4_preserves_first_state_divergence_before_later_unported_error(self):
        lines = jsonl(l4_begin(), l4_decision(index=0), l4_action(index=0),
                      l4_decision(index=1), l4_action(index=1), l4_end())
        run, = l4_run.parse_runs(lines)
        first_actual = dict(run["decisions"][0])
        first_actual["options"] = {"index": 999}
        error = {"status": "unported", "unported": True, "message": "later content is unported"}

        row = l4_run.compare_run(run, [first_actual], None, error)

        self.assertEqual("diverged", row["status"])
        self.assertEqual(0, row["first"]["index"])
        self.assertEqual("$.options.index", row["first"]["path"])

        # The original masking bug was in the separate L3 classification path.
        combat, = l3_combat.parse_runs(jsonl(l3_begin(), l3_enter(), l3_state(),
            {"event": "action", "type": "end_turn"}, {"event": "end"}))
        row = l3_combat.compare_run(combat, [l3_state(999)], None, error)
        self.assertEqual("diverged", row["status"])
        self.assertEqual("$.state.turn", row["first"]["path"])


class DifferentialProcessDiagnosticsTests(unittest.TestCase):
    def test_invalid_utf8_is_rejected_without_changing_raw_evidence(self):
        from differential.common import ProcessFailure, run_process
        with tempfile.TemporaryDirectory() as temp:
            prefix = str(Path(temp) / "encoding")
            command = [sys.executable, "-c", "import os; os.write(1, b'\\xff'); os.write(2, b'\\xfe')"]
            with self.assertRaises(ProcessFailure) as caught:
                run_process(command, artifacts=prefix, allowed=(0,))
            self.assertEqual("native_error", caught.exception.status)
            self.assertIn("UTF-8", caught.exception.detail["message"])
            self.assertEqual(b"\xff", Path(prefix + ".stdout.jsonl").read_bytes())
            self.assertEqual(b"\xfe", Path(prefix + ".stderr.txt").read_bytes())

    def test_real_cli_emits_current_decision_before_bad_successor_action(self):
        cli = ROOT / "build" / "sts2sim_cli"
        if not cli.is_file():
            self.skipTest("C++ CLI has not been built")
        from differential.common import run_process
        out, _, _ = run_process([str(cli), "replay", "--seed", "PREFIXDIAGNOSTIC",
                                  "--verify-snapshots", "1"], stdin="map 0 0\n")
        evs = [json.loads(line) for line in out.splitlines()]
        self.assertEqual("decision", evs[0]["event"])
        self.assertEqual("event", evs[0]["kind"])
        self.assertEqual("error", evs[-1]["event"])

    def _fake_cli(self, directory, source):
        path = Path(directory) / "fake_cli.py"
        path.write_text("#!/usr/bin/env python3\n" + source, encoding="utf-8")
        path.chmod(0o755)
        return str(path)

    def test_l3_bounded_probe_rejects_native_end_after_matching_states(self):
        with tempfile.TemporaryDirectory() as temp:
            oracle_lines = jsonl(
                l3_begin(probe_card="BASH+1"), l3_enter(), l3_state(hand=["BASH+1"]),
                {"event": "action", "type": "play", "hand_index": 0,
                 "target": None, "card": "BASH"},
                l3_state(2),
            )
            run, = l3_combat.parse_runs(oracle_lines)
            native_lines = jsonl(l3_state(hand=["BASH+1"]), l3_state(2), {"event": "end"})
            cli = self._fake_cli(temp, "print(" + repr("\n".join(native_lines)) + ")\n")
            diagnostics = {}

            _, _, end, error = l3_combat.replay(
                cli, run, ascension=0, diagnostics=diagnostics)
            row = l3_combat.compare_run(run, diagnostics["observations"], end, error)
            empty_end = l3_combat.compare_run(run, run["observations"], {}, None)

            self.assertEqual("diverged", row["status"])
            self.assertEqual("bounded probe unexpectedly ended combat", row["first"]["message"])
            self.assertEqual("diverged", empty_end["status"])

    def test_l3_reports_unexpected_native_decision_at_its_position(self):
        with tempfile.TemporaryDirectory() as temp:
            oracle_lines = jsonl(
                l3_begin(), l3_enter(), l3_state(),
                {"event": "action", "type": "end_turn"}, {"event": "end"})
            run, = l3_combat.parse_runs(oracle_lines)
            native_lines = jsonl(l3_state(),
                {"event": "decision", "kind": "map", "options": {}},
                {"event": "end"})
            cli = self._fake_cli(temp, "print(" + repr("\n".join(native_lines)) + ")\n")
            diagnostics = {}

            _, _, end, error = l3_combat.replay(
                cli, run, ascension=0, diagnostics=diagnostics)
            row = l3_combat.compare_run(run, diagnostics["observations"], end, error)

            self.assertEqual("diverged", row["status"])
            self.assertEqual(1, row["first"]["index"])
            self.assertEqual("decision", row["first"]["kind"])

    def test_native_sigsegv_with_partial_json_is_native_error_and_keeps_raw_streams(self):
        if not hasattr(signal, "SIGSEGV"):
            self.skipTest("SIGSEGV is unavailable on this platform")
        with tempfile.TemporaryDirectory() as temp:
            cli = self._fake_cli(temp, (
                "import os, signal, sys\n"
                "sys.stdout.write('{\\\"event\\\":\\\"decision\\\"')\n"
                "sys.stdout.flush()\n"
                "sys.stderr.write('native crash marker\\n')\n"
                "sys.stderr.flush()\n"
                "os.kill(os.getpid(), signal.SIGSEGV)\n"
            ))
            prefix = str(Path(temp) / "crash")
            run = {"begin": {"seed": "S", "character": "IRONCLAD"}, "actions": []}

            decisions, end, error = l4_run.replay(cli, run, artifacts=prefix)

            self.assertEqual([], decisions)
            self.assertIsNone(end)
            self.assertEqual("native_error", error["status"])
            self.assertFalse(error["unported"])
            self.assertLess(error["returncode"], 0)
            self.assertEqual('{"event":"decision"', Path(prefix + ".stdout.jsonl").read_text())
            self.assertEqual("native crash marker\n", Path(prefix + ".stderr.txt").read_text())
            process = json.loads(Path(prefix + ".process.json").read_text())
            self.assertEqual("native", process["role"])
            self.assertFalse(process["timeout"])

    def test_l3_timeout_is_not_unported_and_process_artifacts_survive(self):
        with tempfile.TemporaryDirectory() as temp:
            cli = self._fake_cli(temp, (
                "import sys, time\n"
                "print('started', flush=True)\n"
                "print('waiting', file=sys.stderr, flush=True)\n"
                "time.sleep(30)\n"
            ))
            prefix = str(Path(temp) / "timeout")
            run = {"begin": {"seed": "S", "character": "IRONCLAD"},
                   "coord": [0, 0], "actions": []}
            diagnostics = {}

            states, decisions, end, error = l3_combat.replay(
                cli, run, ascension=0, timeout=0.2, artifacts=prefix, diagnostics=diagnostics)

            self.assertEqual([], states)
            self.assertEqual([], decisions)
            self.assertIsNone(end)
            self.assertEqual("timeout", error["status"])
            self.assertFalse(error["unported"])
            self.assertTrue(diagnostics["error"]["timeout"])
            self.assertTrue(json.loads(Path(prefix + ".process.json").read_text())["timeout"])
            self.assertIn("started", Path(prefix + ".stdout.jsonl").read_text())
            self.assertIn("waiting", Path(prefix + ".stderr.txt").read_text())


if __name__ == "__main__":
    unittest.main()
