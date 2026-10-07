#!/usr/bin/env python3
"""Batch manifest, evidence, and resume contract tests with a fake CLI."""
from contextlib import redirect_stderr, redirect_stdout
from argparse import Namespace
import copy
import io
import json
import os
from pathlib import Path
import sys
import signal
import subprocess
import tempfile
import time
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tests" / "l3"))
sys.path.insert(0, str(ROOT / "tests"))

import differential.batch as batch  # noqa: E402


BEGIN = {"event": "begin", "seed": "BATCHSEED01", "character": "IRONCLAD",
         "ascension": 0, "policy_seed": 1}
DECISION = {"event": "decision", "kind": "map", "options": {"coords": [[0, 0]]},
            "run": {"act": 1, "total_floor": 0, "room": "map"}}
ACTION = {"event": "action", "kind": "map", "coord": [0, 0]}
END = {"event": "end", "reason": "death", "run": {"act": 1, "total_floor": 1}}


class DifferentialBatchContractTests(unittest.TestCase):
    def test_unsupported_level_options_reject_before_outputs_or_processes(self):
        cases = {
            "l3": (("--hp", "10000"), ("--hp", "0"), ("--no-neow",),
                   ("--stop", "victory"), ("--max-decisions", "10"),
                   ("--policy-version", "legacy-v1"), ("--policy-version", "coverage-v2"),
                   ("--policy-version", "coverage-v3"),
                   ("--no-verify-snapshots",)),
            "l4": (("--encounter", "CULTISTS_NORMAL"), ("--starting-energy", "0"),
                   ("--starting-stars", "99"), ("--probe-cards", "BASH"),
                   ("--probe-helper", ""), ("--probe-after-card-turn",), ("--max-actions", "10")),
        }
        with tempfile.TemporaryDirectory() as temp:
            for level, options in cases.items():
                for index, option in enumerate(options):
                    with self.subTest(level=level, option=option):
                        out = Path(temp) / (level + str(index))
                        with patch.object(batch, "provenance") as provenance, redirect_stderr(io.StringIO()) as stderr:
                            with self.assertRaises(SystemExit) as caught:
                                batch.main(["run", "--level", level, "--out", str(out), *option])
                        self.assertEqual(2, caught.exception.code)
                        self.assertIn(level.upper() + " does not support", stderr.getvalue())
                        provenance.assert_not_called()
                        self.assertFalse(out.exists())

    def test_ignored_replay_capture_options_reject_before_outputs_or_processes(self):
        cases = {
            "l3": (("--max-actions", "10"), ("--encounter", "CULTISTS_NORMAL"),
                   ("--probe-cards", "BASH"), ("--probe-helper", "BASH"), ("--probe-after-card-turn",)),
            "l4": (("--hp", "10000"), ("--no-neow",), ("--stop", "victory"),
                   ("--max-decisions", "10"), ("--deck", "BASH"), ("--relics", "ANCHOR")),
        }
        shared = (("--seeds", "A"), ("--seeds-file", "missing.json"), ("--count", "1"),
                  ("--characters", "IRONCLAD"), ("--ascensions", "10"), ("--policy-seeds", "2"))
        with tempfile.TemporaryDirectory() as temp:
            for level, options in cases.items():
                for index, option in enumerate(options + shared):
                    with self.subTest(level=level, option=option):
                        out = Path(temp) / (level + str(index))
                        with patch.object(batch, "provenance") as provenance, redirect_stderr(io.StringIO()) as stderr:
                            with self.assertRaises(SystemExit) as caught:
                                batch.main(["replay", "--level", level, "--out", str(out),
                                            "--traces", "missing.oracle.jsonl", *option])
                        self.assertEqual(2, caught.exception.code)
                        self.assertIn("replay uses frozen trace inputs", stderr.getvalue())
                        provenance.assert_not_called()
                        self.assertFalse(out.exists())

    def test_l3_probe_configuration_requires_probes(self):
        with tempfile.TemporaryDirectory() as temp:
            for option in (("--probe-helper", "BASH"), ("--probe-after-card-turn",)):
                with self.subTest(option=option), redirect_stderr(io.StringIO()) as stderr:
                    with self.assertRaises(SystemExit) as caught:
                        batch.main(["run", "--level", "l3", "--out", str(Path(temp) / "unused"), *option])
                    self.assertEqual(2, caught.exception.code)
                    self.assertIn("require --probe-cards", stderr.getvalue())

    def test_exact_case_file_cannot_silently_ignore_expansion_requests(self):
        with tempfile.TemporaryDirectory() as temp:
            for option in (("--count", "1"), ("--characters", "SILENT"),
                           ("--ascensions", "10"), ("--policy-seeds", "2")):
                with self.subTest(option=option), redirect_stderr(io.StringIO()) as stderr:
                    with self.assertRaises(SystemExit) as caught:
                        batch.main(["run", "--out", str(Path(temp) / "unused"),
                                    "--cases-file", "missing.json", *option])
                    self.assertEqual(2, caught.exception.code)
                    self.assertIn("--cases-file fixes identities", stderr.getvalue())

    def test_l3_manifest_and_summary_do_not_claim_snapshot_verification(self):
        with tempfile.TemporaryDirectory() as temp:
            out = Path(temp) / "batch"
            cli = Path(temp) / "fake_l3_cli.py"
            state = {"event": "state", "state": {"player": {"hp": 80}}}
            end = {"event": "end", "player_hp": 80, "run": {"act": 0}}
            trace = [json.dumps(event) for event in
                     (BEGIN, {"event": "enter", "coord": [0, 0]}, state,
                      {"event": "action", "type": "end_turn"}, end)]
            cli.write_text("#!/usr/bin/env python3\nimport json\n" +
                           "\n".join("print(json.dumps(" + repr(event) + "))" for event in (state, end)) + "\n")
            cli.chmod(0o755)
            adapter = batch.module("l3")
            with patch.object(batch, "provenance", side_effect=self._stable_provenance), \
                 patch.object(batch, "module", return_value=adapter), \
                 patch.object(adapter, "run_oracle", return_value=trace), redirect_stdout(io.StringIO()):
                code = batch.main(["run", "--level", "l3", "--out", str(out), "--seeds", BEGIN["seed"],
                                   "--characters", "IRONCLAD", "--ascensions", "0", "--cli", str(cli)])
            self.assertEqual(0, code)
            manifest, job = self._manifest_job(out)
            stats = json.loads((out / "summary.json").read_text())
            self.assertFalse(manifest["config"]["verify_snapshots"])
            self.assertFalse(stats["verify_snapshots"])
            self.assertNotIn("snapshot successor", stats["comparison"])
            self.assertEqual(300, manifest["config"]["max_actions"])
            self.assertIsNone(manifest["config"]["max_decisions"])
            self.assertIsNone(manifest["config"]["stop"])
            self.assertIsNone(manifest["config"]["policy_version"])
            self.assertNotIn("policy_version", job)
            command = json.loads((out / "jobs" / job["id"] / "replay-00000.process.json").read_text())["command"]
            self.assertNotIn("--verify-snapshots", command)

    def test_l4_summary_snapshot_claim_follows_actual_replay_option(self):
        with tempfile.TemporaryDirectory() as temp:
            trace = self._write_trace(temp)
            cli = self._write_fake_cli(temp)
            for enabled in (False, True):
                with self.subTest(enabled=enabled):
                    out = Path(temp) / str(enabled)
                    extra = () if enabled else ("--no-verify-snapshots",)
                    self.assertEqual(0, self._run(out, trace, cli, extra=extra))
                    manifest, job = self._manifest_job(out)
                    stats = json.loads((out / "summary.json").read_text())
                    command = json.loads((out / "jobs" / job["id"] / "replay-00000.process.json").read_text())["command"]
                    self.assertEqual(enabled, manifest["config"]["verify_snapshots"])
                    self.assertEqual(enabled, stats["verify_snapshots"])
                    self.assertEqual(enabled, "snapshot successor" in stats["comparison"])
                    self.assertEqual(enabled, "--verify-snapshots" in command)
                    self.assertIsNone(manifest["config"]["policy_version"])

    def test_replay_policy_version_rejects_before_outputs_or_processes(self):
        with tempfile.TemporaryDirectory() as temp:
            for level in ("l3", "l4"):
                for version in ("legacy-v1", "coverage-v2", "coverage-v3"):
                    with self.subTest(level=level, version=version):
                        out = Path(temp) / (level + version)
                        with patch.object(batch, "provenance") as provenance, redirect_stderr(io.StringIO()) as stderr:
                            with self.assertRaises(SystemExit) as caught:
                                batch.main(["replay", "--level", level, "--out", str(out),
                                            "--traces", "missing.oracle.jsonl", "--policy-version", version])
                        self.assertEqual(2, caught.exception.code)
                        self.assertIn("--policy-version", stderr.getvalue())
                        provenance.assert_not_called()
                        self.assertFalse(out.exists())

    def test_identity_preserves_all_native_probe_inputs(self):
        from differential.common import result_base
        begin = dict(BEGIN, probe_card="MAD_SCIENCE", probe_type="Attack", probe_rider="Sapping",
                     probe_after_card_turn=True, probe_auto_death=False, neow=False)
        row = result_base("L3", {"begin": begin}, 1)
        self.assertEqual({k: v for k, v in begin.items() if k != "event"}, row["identity"])
        self.assertEqual("injected", row["scope"])
        changed = result_base("L3", {"begin": dict(begin, probe_rider="Violence")}, 1)
        self.assertNotEqual(row["identity"], changed["identity"])

    def test_included_model_definition_files_are_fingerprinted(self):
        with tempfile.TemporaryDirectory() as temp:
            cli = self._write_fake_cli(temp)
            sources = batch.provenance(cli, Path(temp) / "unused.dll", False)["source_files"]
            for path in ("game/Models/CardModel.def", "game/Models/PowerModel.def"):
                self.assertEqual(batch.digest(ROOT / path), sources.get(path))

    def test_exact_cases_are_frozen_without_cartesian_expansion(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "cases.json"
            cases = [dict(seed="A", character="SILENT", ascension=0, policy_seed=1),
                     dict(seed="B", character="DEFECT", ascension=10, policy_seed=2)]
            path.write_text(json.dumps(cases))
            args = Namespace(mode="run", cases_file=str(path))
            jobs = batch.make_jobs(args)
            self.assertEqual(cases, [{k: j[k] for k in cases[0]} for j in jobs])
            self.assertEqual(["coverage-v3", "coverage-v3"], [j["policy_version"] for j in jobs])
            args.policy_version = "legacy-v1"
            self.assertNotEqual([j["id"] for j in jobs], [j["id"] for j in batch.make_jobs(args)])
            path.write_text(json.dumps(cases + cases[:1]))
            with self.assertRaisesRegex(ValueError, "duplicate job"):
                batch.make_jobs(args)
            for bad in ({}, dict(cases[0], character="UNKNOWN"), dict(cases[0], ascension=True)):
                path.write_text(json.dumps([bad]))
                with self.assertRaisesRegex(ValueError, "cases-file"):
                    batch.make_jobs(args)

    def _write_trace(self, directory):
        path = Path(directory) / "minimal.oracle.jsonl"
        path.write_text("".join(json.dumps(event) + "\n" for event in (BEGIN, DECISION, ACTION, END)),
                        encoding="utf-8")
        return path

    def _write_fake_cli(self, directory):
        path = Path(directory) / "fake_sts2sim_cli.py"
        script = "\n".join((
            "#!/usr/bin/env python3",
            "import json, os",
            "mode = os.environ.get('DIFF_BATCH_FAKE_MODE', 'identical')",
            "if mode == 'unported':",
            "    print(json.dumps({'event': 'error', 'unported': True, 'message': 'synthetic Unported'}))",
            "else:",
            "    print(json.dumps(" + repr(DECISION) + "))",
            "    print(json.dumps(" + repr(END) + "))",
            "",
        ))
        path.write_text(script, encoding="utf-8")
        path.chmod(0o755)
        return path

    def _stable_provenance(self, cli, oracle, capture, dotnet=None):
        # Freeze the external identity in these contract tests. The suite is
        # run in a shared worktree where other assigned edits may be landing.
        return {"git_commit": "fixture-commit", "source_sha256": "fixture-source-v1",
                "source_files": {}, "dirty": [],
                "runtime": {"cli": {"path": str(cli), "sha256": "fixture-cli-v1"}},
                "system": {"platform": "test", "machine": "test"},
                "oracle_qualification": "test fixture"}

    def _args(self, out, trace, cli, extra=()):
        return ["replay", "--out", str(out), "--level", "l4", "--traces", str(trace),
                "--jobs", "1", "--timeout", "2", "--cli", str(cli),
                "--oracle", str(Path(out).parent / "unused-oracle.dll"), *extra]

    def _run(self, out, trace, cli, *, mode="identical", extra=(), provenance=None):
        value = provenance
        side_effect = value if callable(value) else None
        return_value = None if callable(value) else (copy.deepcopy(value) if value is not None else None)
        with patch.object(batch, "provenance", side_effect=side_effect,
                          return_value=return_value) if value is not None else patch.object(
                              batch, "provenance", side_effect=self._stable_provenance):
            with patch.dict(os.environ, {"DIFF_BATCH_FAKE_MODE": mode}):
                with redirect_stdout(io.StringIO()):
                    return batch.main(self._args(out, trace, cli, extra))

    def _manifest_job(self, out):
        manifest = json.loads((Path(out) / "manifest.json").read_text(encoding="utf-8"))
        self.assertEqual(1, len(manifest["jobs"]))
        return manifest, manifest["jobs"][0]

    def test_manifest_precedes_work_raw_evidence_is_complete_and_resume_does_not_duplicate(self):
        with tempfile.TemporaryDirectory() as temp:
            trace = self._write_trace(temp)
            cli = self._write_fake_cli(temp)
            out = Path(temp) / "batch"
            observed = []
            real_execute = batch.execute_job

            def observe_manifest_then_execute(job, config, level, cli_path, oracle, dotnet, directory):
                manifest_path = Path(directory) / "manifest.json"
                self.assertTrue(manifest_path.is_file(), "manifest must be durable before work starts")
                manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
                self.assertIn(job["id"], {item["id"] for item in manifest["jobs"]})
                observed.append(job["id"])
                return real_execute(job, config, level, cli_path, oracle, dotnet, directory)

            with patch.object(batch, "provenance", side_effect=self._stable_provenance), \
                 patch.object(batch, "execute_job", side_effect=observe_manifest_then_execute), \
                 patch.dict(os.environ, {"DIFF_BATCH_FAKE_MODE": "identical"}), \
                 redirect_stdout(io.StringIO()):
                first_exit = batch.main(self._args(out, trace, cli))

            self.assertEqual(0, first_exit)
            manifest, job = self._manifest_job(out)
            self.assertEqual([job["id"]], observed)
            job_dir = out / "jobs" / job["id"]
            for name in ("oracle.stdout.jsonl", "replay-00000.stdin.txt",
                         "replay-00000.stdout.jsonl", "replay-00000.stderr.txt",
                         "replay-00000.process.json", "result.json"):
                with self.subTest(artifact=name):
                    self.assertTrue((job_dir / name).is_file(), name)
            self.assertTrue((out / "summary.json").is_file())
            self.assertTrue((out / "triage.json").is_file())
            self.assertEqual(1, len((out / "results.jsonl").read_text().splitlines()))
            row = json.loads((out / "results.jsonl").read_text().splitlines()[0])
            self.assertEqual("identical", row["status"])
            self.assertIn("oracle.stdout.jsonl", row["artifacts"])

            with patch.object(batch, "provenance", side_effect=self._stable_provenance), \
                 patch.object(batch, "execute_job", side_effect=observe_manifest_then_execute), \
                 patch.dict(os.environ, {"DIFF_BATCH_FAKE_MODE": "identical"}), \
                 redirect_stdout(io.StringIO()):
                resumed_exit = batch.main(self._args(out, trace, cli, ("--resume",)))

            self.assertEqual(0, resumed_exit)
            self.assertEqual([job["id"]], observed, "completed jobs must be skipped on resume")
            self.assertEqual(1, len((out / "results.jsonl").read_text().splitlines()))
            self.assertEqual(manifest["jobs"][0]["id"], job["id"])

    def test_changed_input_trace_rejects_resume(self):
        with tempfile.TemporaryDirectory() as temp:
            trace = self._write_trace(temp)
            cli = self._write_fake_cli(temp)
            out = Path(temp) / "batch"
            self.assertEqual(0, self._run(out, trace, cli))
            trace.write_text(trace.read_text(encoding="utf-8").replace('"total_floor": 1', '"total_floor": 2'),
                             encoding="utf-8")

            with self.assertRaisesRegex(ValueError, "resume rejected: jobs changed"):
                self._run(out, trace, cli, extra=("--resume",))

    def test_changed_policy_version_rejects_resume(self):
        with tempfile.TemporaryDirectory() as temp:
            cli = self._write_fake_cli(temp)
            out = Path(temp) / "batch"
            adapter = batch.module("l4")
            begin = dict(BEGIN, policy_version="coverage-v3")
            trace = [json.dumps(event) for event in (begin, DECISION, ACTION, END)]
            args = ["run", "--out", str(out), "--level", "l4", "--seeds", BEGIN["seed"],
                    "--characters", "IRONCLAD", "--ascensions", "0", "--cli", str(cli)]
            with patch.object(batch, "provenance", side_effect=self._stable_provenance), \
                 patch.object(batch, "module", return_value=adapter), \
                 patch.object(adapter, "run_oracle", return_value=trace) as capture, redirect_stdout(io.StringIO()):
                self.assertEqual(0, batch.main(args))
                manifest, job = self._manifest_job(out)
                self.assertEqual("coverage-v3", manifest["config"]["policy_version"])
                self.assertEqual("coverage-v3", job["policy_version"])
                self.assertEqual("coverage-v3", json.loads((out / "results.jsonl").read_text())["identity"]["policy_version"])
                self.assertEqual(0, batch.main(args + ["--resume"]))
                with self.assertRaisesRegex(ValueError, "resume rejected: config changed"):
                    batch.main(args + ["--resume", "--policy-version", "legacy-v1"])
                self.assertEqual(1, capture.call_count)

    def test_capture_policy_version_is_required_and_preserves_raw_identity(self):
        with tempfile.TemporaryDirectory() as temp:
            cli = self._write_fake_cli(temp)
            adapter = batch.module("l4")
            for captured, requested, status in (
                (None, None, "invalid_trace"), ("legacy-v1", None, "invalid_trace"),
                ("coverage-v2", None, "invalid_trace"), ("coverage-v3", None, "identical"),
                ("coverage-v2", "coverage-v2", "identical"), ("legacy-v1", "legacy-v1", "identical"),
            ):
                with self.subTest(captured=captured, requested=requested):
                    out = Path(temp) / (str(captured) + str(requested))
                    begin = dict(BEGIN)
                    if captured is not None:
                        begin["policy_version"] = captured
                    trace = [json.dumps(event) for event in (begin, DECISION, ACTION, END)]
                    extra = [] if requested is None else ["--policy-version", requested]
                    with patch.object(batch, "provenance", side_effect=self._stable_provenance), \
                         patch.object(batch, "module", return_value=adapter), \
                         patch.object(adapter, "run_oracle", return_value=trace), redirect_stdout(io.StringIO()):
                        code = batch.main(["run", "--out", str(out), "--seeds", BEGIN["seed"],
                                           "--characters", "IRONCLAD", "--ascensions", "0", "--cli", str(cli), *extra])
                    row = json.loads((out / "results.jsonl").read_text())
                    self.assertEqual(status, row["status"])
                    self.assertEqual(0 if status == "identical" else 1, code)
                    if status == "identical":
                        self.assertEqual(captured, row["identity"]["policy_version"])
                    else:
                        self.assertIn("policy_version", row["first"]["message"])

    def test_resume_uses_frozen_input_after_external_trace_is_removed(self):
        with tempfile.TemporaryDirectory() as temp:
            trace = self._write_trace(temp)
            cli = self._write_fake_cli(temp)
            out = Path(temp) / "batch"
            self.assertEqual(0, self._run(out, trace, cli))
            trace.unlink()
            args = self._args(out, trace, cli, ("--resume",))
            del args[args.index("--traces"):args.index("--traces") + 2]
            with patch.object(batch, "provenance", side_effect=self._stable_provenance), redirect_stdout(io.StringIO()):
                self.assertEqual(0, batch.main(args))
            self.assertEqual(1, len((out / "results.jsonl").read_text().splitlines()))

    def test_pending_interrupted_job_replays_its_frozen_copy_without_external_input(self):
        with tempfile.TemporaryDirectory() as temp:
            trace = self._write_trace(temp)
            cli = self._write_fake_cli(temp)
            out = Path(temp) / "batch"
            self.assertEqual(0, self._run(out, trace, cli))
            # Simulate interruption after result.json but before its append.
            (out / "results.jsonl").write_text("")
            trace.unlink()
            args = self._args(out, trace, cli, ("--resume",))
            del args[args.index("--traces"):args.index("--traces") + 2]
            with patch.object(batch, "provenance", side_effect=self._stable_provenance), redirect_stdout(io.StringIO()):
                self.assertEqual(0, batch.main(args))
            self.assertEqual(1, len(list((out / "interrupted").iterdir())))
            row = json.loads((out / "results.jsonl").read_text().splitlines()[0])
            self.assertEqual("identical", row["status"])
            self.assertEqual(1, row["matched"])

    @unittest.skipUnless(os.name == "posix", "POSIX process groups required")
    def test_sigterm_stops_workers_and_descendants_and_preserves_raw_output(self):
        with tempfile.TemporaryDirectory() as temp:
            trace = self._write_trace(temp)
            marker = Path(temp) / "pids.json"
            cli = Path(temp) / "sleeping_cli.py"
            cli.write_text("#!/usr/bin/env python3\nimport os,json,subprocess,sys,time\n"
                           "child=subprocess.Popen([sys.executable,'-c','import time;time.sleep(30)'])\n"
                           f"open({str(marker)!r},'w').write(json.dumps([os.getpid(),child.pid]))\n"
                           "print('started',flush=True)\ntime.sleep(30)\n")
            cli.chmod(0o755)
            out = Path(temp) / "batch"
            args = self._args(out, trace, cli)
            args[args.index("--timeout") + 1] = "30"
            proc = subprocess.Popen([sys.executable, str(ROOT / "tools/differential/batch.py"), *args],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            pids = []
            try:
                # The worker creates the marker before its write is flushed:
                # wait for the content, not for the file.
                deadline = time.monotonic() + 5
                text = ""
                while not text and proc.poll() is None and time.monotonic() < deadline:
                    text = marker.read_text() if marker.exists() else ""
                    if not text:
                        time.sleep(0.01)
                self.assertTrue(text, "worker never started")
                pids = json.loads(text)
                # Allow the already-started worker's first output write to finish.
                time.sleep(0.05)
                proc.send_signal(signal.SIGTERM)
                proc.communicate(timeout=3)
                self.assertEqual(128 + signal.SIGTERM, proc.returncode)
                for pid in pids:
                    stat = Path(f"/proc/{pid}/stat")
                    self.assertTrue(not stat.exists() or stat.read_text().split()[2] == "Z",
                                    "interrupted batch left an executing descendant")
                outputs = list((out / "jobs").rglob("replay-*.stdout.jsonl"))
                self.assertEqual(1, len(outputs))
                self.assertIn(b"started", outputs[0].read_bytes())
            finally:
                if proc.poll() is None:
                    proc.kill()
                    proc.communicate()
                for pid in pids:
                    try:
                        os.kill(pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass

    @unittest.skipUnless(os.name == "posix", "POSIX process groups required")
    def test_timeout_of_l4_supervisor_cleans_its_isolated_native_process(self):
        from differential.common import ProcessFailure, run_process
        with tempfile.TemporaryDirectory() as temp:
            trace = self._write_trace(temp)
            marker = Path(temp) / "pid.txt"
            cli = Path(temp) / "sleeping_cli.py"
            cli.write_text("#!/usr/bin/env python3\nimport os,time\n"
                           f"open({str(marker)!r},'w').write(str(os.getpid()))\n"
                           "print('nested-started',flush=True)\ntime.sleep(30)\n")
            cli.chmod(0o755)
            inner = Path(temp) / "inner"
            command = [sys.executable, str(ROOT / "tests/l4/l4_run.py"), "--trace", str(trace),
                       "--cli", str(cli), "--timeout", "30", "--artifacts-dir", str(inner)]
            with self.assertRaises(ProcessFailure) as caught:
                run_process(command, timeout=0.5, artifacts=Path(temp) / "outer", allowed=(0,))
            self.assertEqual("timeout", caught.exception.status)
            self.assertTrue(marker.exists())
            pid = int(marker.read_text())
            self.assertFalse(Path(f"/proc/{pid}").exists(), "isolated native process survived its supervisor")
            outputs = list(inner.rglob("replay-*.stdout.jsonl"))
            self.assertEqual(1, len(outputs))
            self.assertIn(b"nested-started", outputs[0].read_bytes())

    def test_cached_artifact_or_result_json_pollution_rejects_resume(self):
        cases = (
            ("raw-artifact", "recorded artifacts changed", "artifact"),
            ("result-json", "result.json differs", "result"),
        )
        for name, message, target in cases:
            with self.subTest(name=name), tempfile.TemporaryDirectory() as temp:
                trace = self._write_trace(temp)
                cli = self._write_fake_cli(temp)
                out = Path(temp) / "batch"
                self.assertEqual(0, self._run(out, trace, cli))
                _, job = self._manifest_job(out)
                job_dir = out / "jobs" / job["id"]
                if target == "artifact":
                    artifact = job_dir / "replay-00000.stdout.jsonl"
                    artifact.write_text(artifact.read_text(encoding="utf-8") + "polluted\n", encoding="utf-8")
                else:
                    result = job_dir / "result.json"
                    value = json.loads(result.read_text(encoding="utf-8"))
                    value["status"] = "diverged"
                    result.write_text(json.dumps(value) + "\n", encoding="utf-8")

                with self.assertRaisesRegex(ValueError, message):
                    self._run(out, trace, cli, extra=("--resume",))

    def test_incomplete_results_jsonl_rejects_resume(self):
        with tempfile.TemporaryDirectory() as temp:
            trace = self._write_trace(temp)
            cli = self._write_fake_cli(temp)
            out = Path(temp) / "batch"
            self.assertEqual(0, self._run(out, trace, cli))
            results = out / "results.jsonl"
            with results.open("a", encoding="utf-8") as stream:
                stream.write("{\n")

            with self.assertRaisesRegex(ValueError, "results.jsonl line 2 is incomplete/corrupt"):
                self._run(out, trace, cli, extra=("--resume",))

    def test_git_source_or_runtime_identity_change_rejects_resume(self):
        changed_fields = ("git_commit", "source_sha256", "runtime", "system")
        for field in changed_fields:
            with self.subTest(field=field), tempfile.TemporaryDirectory() as temp:
                trace = self._write_trace(temp)
                cli = self._write_fake_cli(temp)
                out = Path(temp) / "batch"
                base = self._stable_provenance(cli, "unused", False)
                self.assertEqual(0, self._run(out, trace, cli, provenance=base))
                changed = copy.deepcopy(base)
                if field == "runtime":
                    changed["runtime"]["cli"]["sha256"] = "changed-runtime"
                else:
                    changed[field] = "changed-identity"

                with self.assertRaisesRegex(ValueError, f"resume rejected: {field} changed"):
                    self._run(out, trace, cli, extra=("--resume",), provenance=changed)

    def _runtime_fixture(self, directory):
        root = Path(directory)
        game_data = root / "game-data"
        localization = root / "src" / "localization" / "eng"
        game_data.mkdir(parents=True)
        localization.mkdir(parents=True)
        for name, data in (("sts2.dll", b"game-v1"), ("GodotSharp.dll", b"godot-v1"),
                           ("0Harmony.dll", b"harmony-v1"), ("catalog.json", b"{}"),
                           ("libgodot.so", b"native-v1"), ("sts2.xml", b"symbols-v1")):
            (game_data / name).write_bytes(data)
        (localization / "CORE.json").write_bytes(b"{\"key\":\"value\"}")
        for name in ("cards", "powers", "relics", "potions", "events", "enchantments",
                     "characters", "monsters"):
            (localization / (name + ".json")).write_bytes(b"{}")
        oracle_dir = root / "oracle"
        oracle_dir.mkdir()
        oracle = oracle_dir / "Sts2Oracle.dll"
        oracle.write_bytes(b"oracle-v1")
        (oracle_dir / "Sts2Oracle.runtimeconfig.json").write_text("{}\n", encoding="utf-8")
        cli = root / "fake-cli"
        cli.write_bytes(b"fake-cli-v1")
        dotnet = root / "fake-dotnet"
        runtime_paths = json.dumps({"schema_version": 1, "game_data": str(game_data),
                                    "localization": str(localization)})
        dotnet.write_text(
            "#!/usr/bin/env python3\nimport sys\n"
            "if sys.argv[-1:] == ['runtime-provenance']:\n"
            "    print(" + repr(runtime_paths) + ")\n"
            "else:\n    print('Microsoft.NETCore.App 9.0.0 [/fixture]')\n",
            encoding="utf-8")
        dotnet.chmod(0o755)
        return {"game_data": game_data, "localization": localization,
                "oracle": oracle, "cli": cli, "dotnet": dotnet}

    def _runtime_batch_args(self, out, fixture):
        return ["run", "--out", str(out), "--seeds", "RUNTIMEPROVENANCE01",
                "--characters", "IRONCLAD", "--ascensions", "0", "--policy-seeds", "1",
                "--jobs", "1", "--cli", str(fixture["cli"]), "--oracle", str(fixture["oracle"]),
                "--dotnet", str(fixture["dotnet"])]

    def _fake_successful_job(self, job, config, level, cli, oracle, dotnet, out):
        directory = Path(out) / "jobs" / job["id"]
        directory.mkdir(parents=True, exist_ok=True)
        row = {"id": job["id"], "index": job["index"], "job": job,
               "level": level.upper(), "status": "identical", "matched": 1, "total": 1,
               "first": None, "elapsed_seconds": 0.0}
        row["artifacts"] = batch.artifacts_hashes(directory)
        batch.write_json(directory / "result.json", row)
        return row

    def test_external_oracle_runtime_changes_and_inventory_reject_resume(self):
        with tempfile.TemporaryDirectory() as temp:
            fixture = self._runtime_fixture(temp)
            out = Path(temp) / "batch"
            args = self._runtime_batch_args(out, fixture)
            paths = {key: fixture[key] for key in ("game_data", "localization")}
            with patch.object(batch, "oracle_runtime_paths", return_value=paths), \
                 patch.object(batch, "execute_job", side_effect=self._fake_successful_job), \
                 redirect_stdout(io.StringIO()):
                self.assertEqual(0, batch.main(args))

            manifest = json.loads((out / "manifest.json").read_text(encoding="utf-8"))
            external = manifest["provenance"]["runtime"]["oracle_external"]
            self.assertEqual({"sts2.dll", "GodotSharp.dll", "0Harmony.dll", "catalog.json",
                              "libgodot.so", "sts2.xml"},
                             set(external["game_data_files"]))
            self.assertEqual({"CORE.json", "cards.json", "powers.json", "relics.json", "potions.json",
                              "events.json", "enchantments.json", "characters.json", "monsters.json"},
                             set(external["localization_files"]))

            mutations = (
                (fixture["game_data"] / "sts2.dll", b"game-v2"),
                (fixture["game_data"] / "0Harmony.dll", b"harmony-v2"),
                (fixture["game_data"] / "libgodot.so", b"native-v2"),
                (fixture["localization"] / "CORE.json", b"{\"key\":\"changed\"}"),
            )
            for path, changed in mutations:
                with self.subTest(path=path.name):
                    original = path.read_bytes()
                    path.write_bytes(changed)
                    try:
                        with patch.object(batch, "oracle_runtime_paths", return_value=paths), \
                             patch.object(batch, "execute_job", side_effect=AssertionError("resume launched a job")), \
                             redirect_stdout(io.StringIO()), \
                             self.assertRaisesRegex(ValueError, "resume rejected: runtime changed"):
                            batch.main(args + ["--resume"])
                    finally:
                        path.write_bytes(original)

            additions = (
                fixture["game_data"] / "late-dependency.dll",
                fixture["localization"] / "NEW_TABLE.json",
            )
            for path in additions:
                with self.subTest(added=path.name):
                    path.write_bytes(b"new-runtime-input")
                    try:
                        with patch.object(batch, "oracle_runtime_paths", return_value=paths), \
                             patch.object(batch, "execute_job", side_effect=AssertionError("resume launched a job")), \
                             redirect_stdout(io.StringIO()), \
                             self.assertRaisesRegex(ValueError, "resume rejected: runtime changed"):
                            batch.main(args + ["--resume"])
                    finally:
                        path.unlink()

            removed_table = fixture["localization"] / "CORE.json"
            original_table = removed_table.read_bytes()
            removed_table.unlink()
            try:
                with patch.object(batch, "oracle_runtime_paths", return_value=paths), \
                     patch.object(batch, "execute_job", side_effect=AssertionError("resume launched a job")), \
                     redirect_stdout(io.StringIO()), \
                     self.assertRaisesRegex(ValueError, "resume rejected: runtime changed"):
                    batch.main(args + ["--resume"])
            finally:
                removed_table.write_bytes(original_table)

    def test_capture_requires_external_runtime_files_but_offline_replay_does_not_query_oracle(self):
        with tempfile.TemporaryDirectory() as temp:
            fixture = self._runtime_fixture(temp)
            paths = {key: fixture[key] for key in ("game_data", "localization")}
            missing = fixture["game_data"] / "0Harmony.dll"
            missing.unlink()
            with patch.object(batch, "oracle_runtime_paths", return_value=paths):
                with self.assertRaisesRegex(ValueError, "missing required file:.*0Harmony.dll"):
                    batch.provenance(fixture["cli"], fixture["oracle"], True, str(fixture["dotnet"]))

            with patch.object(batch, "oracle_runtime_paths", side_effect=AssertionError("offline replay queried Oracle")):
                value = batch.provenance(fixture["cli"], fixture["oracle"], False, "unused-dotnet")
            self.assertNotIn("oracle_external", value["runtime"])
            self.assertNotIn("oracle", value["runtime"])

    def test_capture_requires_common_gameplay_localization_tables(self):
        with tempfile.TemporaryDirectory() as temp:
            fixture = self._runtime_fixture(temp)
            paths = {key: fixture[key] for key in ("game_data", "localization")}
            (fixture["localization"] / "cards.json").unlink()
            with patch.object(batch, "oracle_runtime_paths", return_value=paths):
                with self.assertRaisesRegex(ValueError, "missing required English tables: cards.json"):
                    batch.provenance(fixture["cli"], fixture["oracle"], True, str(fixture["dotnet"]))

    def test_runtime_change_during_execution_preserves_results_without_qualification(self):
        with tempfile.TemporaryDirectory() as temp:
            fixture = self._runtime_fixture(temp)
            out = Path(temp) / "batch"

            def save_result_then_mutate_runtime(job, config, level, cli, oracle, dotnet, directory):
                row = self._fake_successful_job(job, config, level, cli, oracle, dotnet, directory)
                (fixture["game_data"] / "sts2.dll").write_bytes(b"game-v2-during-batch")
                return row

            with patch.object(batch, "execute_job", side_effect=save_result_then_mutate_runtime), \
                 redirect_stdout(io.StringIO()), \
                 self.assertRaisesRegex(ValueError, "batch source/runtime changed during execution"):
                batch.main(self._runtime_batch_args(out, fixture))

            rows = (out / "results.jsonl").read_text(encoding="utf-8").splitlines()
            self.assertEqual(1, len(rows), "the completed raw result must remain recorded")
            self.assertEqual("identical", json.loads(rows[0])["status"])
            self.assertTrue((out / "jobs" / json.loads(rows[0])["id"] / "result.json").is_file())
            self.assertTrue((out / "manifest.json").is_file())
            self.assertFalse((out / "summary.json").exists(), "changed runtime cannot qualify the batch")

    def test_runtime_provenance_rejects_invalid_json_schema_and_missing_paths(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            game_data = root / "game-data"
            localization = root / "localization"
            game_data.mkdir()
            localization.mkdir()
            valid = {"schema_version": 1, "game_data": str(game_data), "localization": str(localization)}
            responses = (
                ("not-json", "did not return one JSON object"),
                (json.dumps({"schema_version": 1, "game_data": str(game_data)}), "missing or unknown fields"),
                (json.dumps(dict(valid, schema_version=2)), "unsupported schema_version"),
            )
            for stdout, message in responses:
                with self.subTest(stdout=stdout):
                    completed = subprocess.CompletedProcess(["dotnet"], 0, stdout, "")
                    with patch.object(batch.subprocess, "run", return_value=completed), \
                         self.assertRaisesRegex(ValueError, message):
                        batch.oracle_runtime_paths("dotnet", "oracle.dll")

            missing = dict(valid, game_data=str(root / "absent-game-data"))
            completed = subprocess.CompletedProcess(["dotnet"], 0, json.dumps(missing), "")
            with patch.object(batch.subprocess, "run", return_value=completed), \
                 self.assertRaisesRegex(ValueError, "game_data path is unavailable"):
                batch.oracle_runtime_paths("dotnet", "oracle.dll")

    def test_unported_remains_unported_in_strict_and_explore_summaries(self):
        with tempfile.TemporaryDirectory() as temp:
            trace = self._write_trace(temp)
            cli = self._write_fake_cli(temp)
            cases = (("strict", (), 1, True), ("explore", ("--explore",), 0, False))
            for name, flags, expected_exit, strict in cases:
                with self.subTest(name=name):
                    out = Path(temp) / name
                    exit_code = self._run(out, trace, cli, mode="unported", extra=flags)
                    self.assertEqual(expected_exit, exit_code)
                    stats = json.loads((out / "summary.json").read_text(encoding="utf-8"))
                    self.assertEqual(strict, stats["strict"])
                    self.assertEqual({"unported": 1}, stats["statuses"])
                    self.assertNotIn("identical", stats["statuses"])

    def test_worker_exception_is_recorded_instead_of_losing_job(self):
        with tempfile.TemporaryDirectory() as temp:
            trace = self._write_trace(temp)
            cli = self._write_fake_cli(temp)
            out = Path(temp) / "batch"

            def unexpected_worker_failure(*args, **kwargs):
                raise RuntimeError("injected worker failure outside job adapter")

            try:
                with patch.object(batch, "provenance", side_effect=self._stable_provenance), \
                     patch.object(batch, "execute_job", side_effect=unexpected_worker_failure), \
                     patch.dict(os.environ, {"DIFF_BATCH_FAKE_MODE": "identical"}), \
                     redirect_stdout(io.StringIO()):
                    batch.main(self._args(out, trace, cli))
            except RuntimeError as exc:
                self.fail(f"worker exception escaped the batch and lost its result row: {exc}")

            lines = (out / "results.jsonl").read_text(encoding="utf-8").splitlines()
            self.assertEqual(1, len(lines))
            row = json.loads(lines[0])
            self.assertEqual("tool_error", row["status"])
            manifest, job = self._manifest_job(out)
            self.assertEqual(job["id"], row["id"])
            stats = json.loads((out / "summary.json").read_text(encoding="utf-8"))
            self.assertEqual(1, stats["finished_jobs"])
            self.assertEqual(1, stats["planned_jobs"])
            self.assertEqual(1, stats["statuses"]["tool_error"])


if __name__ == "__main__":
    unittest.main()
