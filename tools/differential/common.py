"""Shared, dependency-free differential protocol and process diagnostics."""
import copy
from contextlib import contextmanager
import json
import os
from pathlib import Path
import signal
import subprocess
import threading
import time

_process_lock = threading.Lock()
_processes = set()
_cancelled = threading.Event()


def cancel_processes():
    """Stop current workers and prevent queued workers from launching children."""
    with _process_lock:
        _cancelled.set()
        pending = tuple(_processes)
        for proc in pending:
            try:
                # Python supervisors can now clean up their own isolated
                # children before their enclosing process is forced down.
                os.killpg(proc.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
    def force_remaining():
        with _process_lock:
            for proc in pending:
                if proc not in _processes:
                    continue
                try:
                    os.killpg(proc.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
    timer = threading.Timer(1.0, force_remaining)
    timer.daemon = True
    timer.start()


@contextmanager
def process_supervision(on_cancel=None):
    with _process_lock:
        if _processes:
            raise RuntimeError("previous differential processes are still running")
        _cancelled.clear()
    previous = {}
    def stop():
        cancel_processes()
        if on_cancel is not None:
            on_cancel()
    def interrupt(signum, frame):
        stop()
        raise SystemExit(128 + signum)
    if threading.current_thread() is threading.main_thread():
        for signum in (signal.SIGINT, signal.SIGTERM):
            previous[signum] = signal.signal(signum, interrupt)
    try:
        yield
    except BaseException:
        stop()
        raise
    finally:
        for signum, handler in previous.items():
            signal.signal(signum, handler)


class ProcessFailure(RuntimeError):
    def __init__(self, status, detail, stdout="", stderr=""):
        self.status = status
        self.detail = detail
        self.stdout = stdout
        self.stderr = stderr
        super().__init__(detail["message"])


def write_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)


def run_process(command, *, stdin=None, timeout=180, artifacts=None, role="native", allowed=(0, 1)):
    """Preserve raw evidence even on a signal, malformed output, or timeout.

    A session makes timeout/interrupt cleanup include any children; the engine
    and oracle themselves are each isolated in one process per job.
    """
    start = time.monotonic()
    detail = {"command": [str(x) for x in command], "role": role}
    raw_stdout = raw_stderr = b""
    timed_out = False
    interrupted = None
    proc = None
    try:
        with _process_lock:
            if _cancelled.is_set():
                raise OSError("differential batch interrupted before process launch")
            proc = subprocess.Popen(command, stdin=subprocess.PIPE if stdin is not None else subprocess.DEVNULL,
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE, start_new_session=True)
            _processes.add(proc)
        try:
            raw_stdout, raw_stderr = proc.communicate(stdin.encode("utf-8") if stdin is not None else None,
                                                      timeout=timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            try:
                os.killpg(proc.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
            try:
                raw_stdout, raw_stderr = proc.communicate(timeout=2)
            except subprocess.TimeoutExpired:
                try:
                    os.killpg(proc.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                raw_stdout, raw_stderr = proc.communicate()
        except BaseException as exc:
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            raw_stdout, raw_stderr = proc.communicate()
            interrupted = exc
        detail["returncode"] = proc.returncode
    except OSError as exc:
        detail["returncode"] = None
        raw_stderr = str(exc).encode("utf-8")
    finally:
        with _process_lock:
            _processes.discard(proc)
    stderr = raw_stderr.decode("utf-8", errors="replace")
    invalid_encoding = False
    try:
        stdout = raw_stdout.decode("utf-8")
    except UnicodeDecodeError:
        invalid_encoding = True
        # Keep readable valid-prefix diagnostics, while rejecting the stream.
        stdout = raw_stdout.decode("utf-8", errors="replace")
    detail["elapsed_seconds"] = time.monotonic() - start
    detail["timeout"] = timed_out
    code = detail["returncode"]
    if timed_out:
        detail["message"] = f"{role} exceeded {timeout:g}s"
    elif code is not None and code < 0:
        detail["signal"] = signal.Signals(-code).name if -code in signal.Signals._value2member_map_ else str(-code)
        detail["message"] = f"{role} terminated by {detail['signal']} (exit {code})"
    elif invalid_encoding:
        detail["message"] = f"{role} stdout is not valid UTF-8"
    else:
        detail["message"] = f"{role} exit {code}: {stderr.strip()[-2000:]}"
    if artifacts:
        prefix = Path(artifacts)
        prefix.parent.mkdir(parents=True, exist_ok=True)
        Path(str(prefix) + ".stdout.jsonl").write_bytes(raw_stdout)
        Path(str(prefix) + ".stderr.txt").write_bytes(raw_stderr)
        if stdin is not None:
            Path(str(prefix) + ".stdin.txt").write_text(stdin, encoding="utf-8")
        write_json(str(prefix) + ".process.json", detail)
    if interrupted is not None:
        raise interrupted
    if timed_out or invalid_encoding or code not in allowed or (role == "oracle" and "Exception:" in stderr):
        raise ProcessFailure("timeout" if timed_out else role + "_error", detail, stdout, stderr)
    return stdout, stderr, detail


def events(lines, allowed):
    for number, line in enumerate(lines, 1):
        line = line.strip().lstrip("\ufeff")
        if not line:
            continue
        try:
            ev = json.loads(line)
        except (TypeError, ValueError) as exc:
            raise ValueError(f"trace line {number}: invalid JSON: {str(exc)[:200]}") from exc
        if not isinstance(ev, dict) or ev.get("event") not in allowed:
            raise ValueError(f"trace line {number}: unknown or missing event")
        yield ev


def native_events(stdout, allowed):
    """Malformed native JSON is a native protocol failure, never an oracle error."""
    parsed = []
    terminal = False
    try:
        for ev in events(stdout.splitlines(), allowed):
            if terminal:
                raise ValueError("native event after terminal end/error")
            kind = ev["event"]
            if kind == "state" and not isinstance(ev.get("state"), dict):
                raise ValueError("native state payload is missing or malformed")
            if kind == "decision":
                if not isinstance(ev.get("kind"), str):
                    raise ValueError("native decision kind is missing or malformed")
                payload = "combat" if ev["kind"] == "combat" else "options"
                payload_types = dict if payload == "combat" or ev["kind"] == "card_select" else (dict, list)
                if not isinstance(ev.get(payload), payload_types):
                    raise ValueError(f"native decision {payload} payload is missing or malformed")
            if kind == "error" and not isinstance(ev.get("message"), str):
                raise ValueError("native error message is missing or malformed")
            parsed.append(ev)
            terminal = ev["event"] in ("end", "error")
        return parsed, None
    except ValueError as exc:
        return parsed, {"status": "native_error", "message": str(exc), "unported": False}


def first_diff(a, b, path="$"):
    if type(a) is not type(b):
        return path, a, b
    if isinstance(a, dict):
        for key in list(a) + [key for key in b if key not in a]:
            if key not in a or key not in b:
                return f"{path}.{key}", a.get(key, "<missing>"), b.get(key, "<missing>")
            diff = first_diff(a[key], b[key], f"{path}.{key}")
            if diff:
                return diff
        return None
    if isinstance(a, list):
        for index, (left, right) in enumerate(zip(a, b)):
            diff = first_diff(left, right, f"{path}[{index}]")
            if diff:
                return diff
        if len(a) != len(b):
            return path + ".length", len(a), len(b)
        return None
    return None if a == b else (path, a, b)


def comparable(ev):
    value = copy.deepcopy(ev)
    if value.get("kind") == "card_select":
        # Async stack walking is diagnostic metadata, not a card selection rule.
        value.get("options", {}).pop("source", None)
    return value


def result_base(level, run, total):
    begin = run["begin"]
    end = run.get("end") or {}
    state = end.get("run", {})
    injected = (begin.get("neow") is False or
                any(begin.get(k) for k in ("hp", "deck", "relics", "probe_card", "probe_potion", "potion"))
                or any(begin.get(k) is not None for k in ("starting_energy", "starting_stars"))
                or bool(run.get("encounter")))
    return {"schema_version": 1, "level": level,
            "identity": {**{key: begin.get(key) for key in ("seed", "character", "ascension", "policy_seed")},
                         **{key: value for key, value in begin.items() if key not in ("event", "schema_version")}},
            "status": "identical", "matched": 0, "total": total, "first": None,
            "endpoint": {"reason": end.get("reason", "combat_end" if end else "probe"),
                         "act": state.get("act"), "floor": state.get("total_floor")},
            "scope": "injected" if injected else "natural"}


def fail(result, status, message, *, index=None, kind=None, diff=None, context=None):
    result["status"] = status
    result["first"] = {"message": message, "index": index, "kind": kind}
    if diff:
        result["first"].update(path=diff[0], expected=diff[1], actual=diff[2])
    if context:
        result["first"]["context"] = context
    return result


def error_status(error):
    return error.get("status", "unported" if error.get("unported") else "native_error")


def summary(results):
    counts = {}
    endpoints = {}
    for row in results:
        counts[row["status"]] = counts.get(row["status"], 0) + 1
        ep = row.get("endpoint", {})
        key = f"{ep.get('reason')}@act{ep.get('act')}"
        endpoints[key] = endpoints.get(key, 0) + 1
    return {"jobs": len(results), "statuses": counts,
            "matched": sum(row.get("matched", 0) for row in results),
            "total": sum(row.get("total", 0) for row in results), "endpoints": endpoints}


def exit_code(results, strict):
    allowed = {"identical", "probe_identical"}
    if not strict:
        allowed.add("unported")
    return 0 if results and all(row["status"] in allowed for row in results) else 1
