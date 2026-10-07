#!/usr/bin/env python3
"""Contract tests for supervised, isolated L4 relic-matrix workers."""
from argparse import Namespace
from contextlib import redirect_stdout
import io
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests" / "l4"))
sys.path.insert(0, str(ROOT / "tests"))

import l4_run  # noqa: E402
from differential.common import ProcessFailure  # noqa: E402


class L4NonheroRelicMatrixTests(unittest.TestCase):
    def _args(self, artifacts_dir):
        return Namespace(
            relics=None,
            relic_matrix=None,
            seeds="SEED123",
            characters="IRONCLAD",
            policy_seeds="2",
            max_decisions=777,
            stop="after-act1",
            cli="/fixture/sts2sim_cli",
            ascension=10,
            no_neow=True,
            hp=250,
            deck="BASH,STRIKE_IRONCLAD",
            force_sacrifice=1,
            verify_snapshots=True,
            verbose=True,
            timeout=0.25,
            artifacts_dir=str(artifacts_dir) if artifacts_dir is not None else None,
            matrix_workers=2,
        )

    @staticmethod
    def _option(command, name):
        return command[command.index(name) + 1]

    def test_workers_receive_timeout_and_unique_supervised_artifacts(self):
        with tempfile.TemporaryDirectory() as temp:
            artifact_parent = Path(temp) / "matrix-artifacts"
            calls = []

            def fake_run_process(command, **kwargs):
                calls.append((command, kwargs))
                return "worker output\n", "", {"returncode": 0}

            with patch.object(l4_run, "nonhero_relic_inventory",
                              return_value={"in_scope": ["RELIC_ALPHA", "RELIC_BETA", "MASSIVE_SCROLL"]}), \
                 patch.object(l4_run, "run_process", side_effect=fake_run_process), \
                 redirect_stdout(io.StringIO()) as output:
                status = l4_run.run_nonhero_relic_matrix(self._args(artifact_parent))

            self.assertEqual(0, status)
            self.assertEqual(2, len(calls), "multiplayer-only relic must remain skipped")
            self.assertIn("MATRIX SKIP MASSIVE_SCROLL", output.getvalue())
            artifact_root = Path(output.getvalue().split("MATRIX ARTIFACTS ", 1)[1].splitlines()[0])
            self.assertTrue(artifact_root.is_dir())
            self.assertEqual(artifact_parent.resolve(), artifact_root.parent.resolve())

            by_relic = {}
            for command, kwargs in calls:
                relic = self._option(command, "--relics")
                by_relic[relic] = (command, kwargs)
                self.assertEqual([sys.executable, l4_run.__file__], command[:2])
                self.assertEqual("0.25", self._option(command, "--timeout"))
                self.assertEqual("SEED123", self._option(command, "--seeds"))
                self.assertEqual("IRONCLAD", self._option(command, "--characters"))
                self.assertEqual("2", self._option(command, "--policy-seeds"))
                self.assertEqual("777", self._option(command, "--max-decisions"))
                self.assertEqual("after-act1", self._option(command, "--stop"))
                self.assertEqual("/fixture/sts2sim_cli", self._option(command, "--cli"))
                self.assertIn("--require-complete", command)
                self.assertIn("--no-neow", command)
                self.assertIn("--verify-snapshots", command)
                self.assertIn("--verbose", command)
                self.assertEqual("10", self._option(command, "--ascension"))
                self.assertEqual("250", self._option(command, "--hp"))
                self.assertEqual("BASH,STRIKE_IRONCLAD", self._option(command, "--deck"))
                self.assertEqual("1", self._option(command, "--force-sacrifice"))

                child_artifacts = Path(self._option(command, "--artifacts-dir"))
                result_json = Path(self._option(command, "--result-json"))
                self.assertEqual(artifact_root / "workers" / relic, child_artifacts)
                self.assertEqual(child_artifacts / "result.json", result_json)
                self.assertEqual(0.25, kwargs["timeout"])
                self.assertEqual(str(artifact_root / "processes" / relic), kwargs["artifacts"])
                self.assertEqual("matrix worker " + relic, kwargs["role"])
                self.assertEqual((0, 1), kwargs["allowed"])

            self.assertEqual({"RELIC_ALPHA", "RELIC_BETA"}, set(by_relic))
            child_dirs = {self._option(command, "--artifacts-dir") for command, _ in calls}
            process_prefixes = {kwargs["artifacts"] for _, kwargs in calls}
            result_paths = {self._option(command, "--result-json") for command, _ in calls}
            self.assertEqual(2, len(child_dirs))
            self.assertEqual(2, len(process_prefixes))
            self.assertEqual(2, len(result_paths))

    def test_supervisor_timeout_failure_counts_as_matrix_failure(self):
        with tempfile.TemporaryDirectory() as temp:
            def fake_run_process(command, **kwargs):
                relic = self._option(command, "--relics")
                if relic == "RELIC_ALPHA":
                    raise ProcessFailure("timeout", {
                        "returncode": None,
                        "message": "matrix worker exceeded 0.25s",
                    }, "partial stdout", "timed out")
                return "ok\n", "", {"returncode": 0}

            with patch.object(l4_run, "nonhero_relic_inventory",
                              return_value={"in_scope": ["RELIC_ALPHA", "RELIC_BETA"]}), \
                 patch.object(l4_run, "run_process", side_effect=fake_run_process), \
                 redirect_stdout(io.StringIO()) as output:
                status = l4_run.run_nonhero_relic_matrix(self._args(Path(temp) / "artifacts"))

            self.assertEqual(1, status)
            self.assertIn("MATRIX DIFF/ERROR RELIC_ALPHA", output.getvalue())
            self.assertIn("partial stdout", output.getvalue())
            self.assertIn("timed out", output.getvalue())
            self.assertIn("1 failed", output.getvalue())


if __name__ == "__main__":
    unittest.main()
