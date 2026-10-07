"""Small dependency-free Python binding for the M3 Env C ABI.

The C ABI is intentional: pybind11 is optional in this repository, while
ctypes is part of CPython and keeps the environment usable in a clean build.
"""
from __future__ import annotations

import ctypes
import json
import os
from dataclasses import dataclass
from pathlib import Path


_CHARACTERS = {
    "DEFECT": 1,
    "DEPRECATED_CHARACTER": 2,
    "DEPRIVED": 3,
    "IRONCLAD": 4,
    "NECROBINDER": 5,
    "RANDOM_CHARACTER": 6,
    "REGENT": 7,
    "SILENT": 8,
}


class _CAction(ctypes.Structure):
    _fields_ = [("kind", ctypes.c_int32), ("index", ctypes.c_int32),
                ("target", ctypes.c_int32), ("col", ctypes.c_int32),
                ("row", ctypes.c_int32)]


@dataclass(frozen=True)
class Action:
    kind: int
    index: int = -1
    target: int = -1
    col: int = 0
    row: int = 0

    def _c(self) -> _CAction:
        return _CAction(self.kind, self.index, self.target, self.col, self.row)

    @classmethod
    def _from_c(cls, value: _CAction) -> "Action":
        return cls(value.kind, value.index, value.target, value.col, value.row)


class UnportedError(RuntimeError):
    """The requested simulation path reached content not yet ported."""


class TraceLimitExceeded(RuntimeError):
    """Preview exceeded its requested causal trace capacity."""


class UnsupportedOperation(RuntimeError):
    """The requested operation or stop condition is outside the contract."""


class OperationCapacityExceeded(RuntimeError):
    """Native resource or original type range exceeded; no successor exists."""


class InvalidOperationRequest(RuntimeError):
    """A malformed, stale or currently illegal request was rejected."""


def _library() -> ctypes.CDLL:
    configured = os.environ.get("STS2SIM_ENV_LIB")
    candidates = [Path(configured)] if configured else []
    candidates += [Path(__file__).resolve().parents[1] / "build" / "libsts2env_c.so"]
    for path in candidates:
        if path and path.exists():
            return ctypes.CDLL(str(path))
    raise FileNotFoundError("libsts2env_c.so; set STS2SIM_ENV_LIB")


_LIB = _library()
_LIB.sts2_env_abi_version.restype = ctypes.c_uint32
_LIB.sts2_env_api_descriptor.restype = ctypes.c_char_p
API_DESCRIPTOR = json.loads(_LIB.sts2_env_api_descriptor().decode())
if _LIB.sts2_env_abi_version() != 1 or API_DESCRIPTOR.get("protocol_version") != 2:
    raise RuntimeError("Incompatible native Env ABI or descriptor protocol")
_LIB.sts2_env_create.restype = ctypes.c_void_p
_LIB.sts2_env_clone.argtypes = [ctypes.c_void_p]
_LIB.sts2_env_clone.restype = ctypes.c_void_p
_LIB.sts2_env_destroy.argtypes = [ctypes.c_void_p]
_LIB.sts2_env_reset.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int32, ctypes.c_int32]
_LIB.sts2_env_reset_named.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int32]
_LIB.sts2_env_reset_combat.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_int32, ctypes.c_int32,
                                       ctypes.c_int32, ctypes.c_int32]
_LIB.sts2_env_inspect_combat_named.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_char_p,
                                               ctypes.c_int32, ctypes.c_char_p, ctypes.c_int32]
_LIB.sts2_env_inspect_current.argtypes = [ctypes.c_void_p]
_LIB.sts2_env_export_oracle_combat_state.argtypes = [ctypes.c_void_p]
_LIB.sts2_env_export_oracle_run_state.argtypes = [ctypes.c_void_p]
_LIB.sts2_env_export_oracle_rewards.argtypes = [ctypes.c_void_p]
_LIB.sts2_env_event_rng_counter.argtypes = [ctypes.c_void_p]
_LIB.sts2_env_event_rng_counter.restype = ctypes.c_int32
_LIB.sts2_env_export_snapshot.argtypes = [ctypes.c_void_p]
_LIB.sts2_env_export_portable_snapshot.argtypes = [ctypes.c_void_p]
_LIB.sts2_env_import_snapshot.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_void_p)]
_LIB.sts2_env_validate_combat_root.argtypes = [ctypes.c_void_p]
_LIB.sts2_env_try_transition.argtypes = [ctypes.c_void_p, _CAction, ctypes.POINTER(ctypes.c_void_p)]
_LIB.sts2_env_preview.argtypes = [ctypes.c_void_p, _CAction, ctypes.c_int32, ctypes.POINTER(ctypes.c_void_p)]
_LIB.sts2_env_preview.restype = ctypes.c_int32
_LIB.sts2_env_preview_with_results.argtypes = [ctypes.c_void_p, _CAction, ctypes.c_int32,
                                              ctypes.c_int32, ctypes.POINTER(ctypes.c_void_p)]
_LIB.sts2_env_preview_with_results.restype = ctypes.c_int32
_LIB.sts2_env_predict_operation.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_size_t,
                                           ctypes.c_int32, ctypes.c_int32, ctypes.POINTER(ctypes.c_void_p)]
_LIB.sts2_env_predict_operation.restype = ctypes.c_int32
_LIB.sts2_env_export_run_queries.argtypes = [ctypes.c_void_p, ctypes.c_int32]
_LIB.sts2_env_export_run_queries.restype = ctypes.c_int32
_LIB.sts2_env_export_combat_queries.argtypes = [ctypes.c_void_p]
_LIB.sts2_env_export_combat_queries.restype = ctypes.c_int32
_LIB.sts2_env_export_calculated_vars.argtypes = [ctypes.c_void_p]
_LIB.sts2_env_export_calculated_vars.restype = ctypes.c_int32
_LIB.sts2_env_export_orbs.argtypes = [ctypes.c_void_p]
_LIB.sts2_env_export_orbs.restype = ctypes.c_int32
_LIB.sts2_env_export_powers.argtypes = [ctypes.c_void_p]
_LIB.sts2_env_export_powers.restype = ctypes.c_int32
_LIB.sts2_env_last_result.argtypes = [ctypes.c_void_p]
_LIB.sts2_env_last_result.restype = ctypes.c_char_p
_LIB.sts2_env_decision.argtypes = [ctypes.c_void_p]
_LIB.sts2_env_decision.restype = ctypes.c_int32
_LIB.sts2_env_hash.argtypes = [ctypes.c_void_p]
_LIB.sts2_env_hash.restype = ctypes.c_uint64
_LIB.sts2_env_determinize.argtypes = [ctypes.c_void_p, ctypes.c_uint64]
_LIB.sts2_env_step.argtypes = [ctypes.c_void_p, _CAction]
_LIB.sts2_env_legal_actions.argtypes = [ctypes.c_void_p, ctypes.POINTER(_CAction), ctypes.c_size_t]
_LIB.sts2_env_legal_actions.restype = ctypes.c_int32
_LIB.sts2_env_action_mask.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t]
_LIB.sts2_env_action_mask.restype = ctypes.c_int32
_LIB.sts2_env_observe.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_int32), ctypes.c_size_t]
_LIB.sts2_env_observe.restype = ctypes.c_int32
_LIB.sts2_env_last_error.argtypes = [ctypes.c_void_p]
_LIB.sts2_env_last_error.restype = ctypes.c_char_p


class Env:
    observation_size = 2048

    def __init__(self, seed: str | None = None, character: str = "IRONCLAD", ascension: int = 0):
        self._handle = _LIB.sts2_env_create()
        if not self._handle:
            raise RuntimeError("sts2_env_create failed")
        if seed is not None:
            self.reset(seed, character, ascension)

    def __del__(self):
        handle = getattr(self, "_handle", None)
        if handle:
            _LIB.sts2_env_destroy(handle)
            self._handle = None

    def _check(self, result: int) -> None:
        if result != 0:
            message = _LIB.sts2_env_last_error(self._handle)
            detail = (message or b"environment error").decode()
            if result == 1:
                raise UnportedError(detail)
            if result == 2:
                raise TraceLimitExceeded(detail)
            if result == 3:
                raise UnsupportedOperation(detail)
            if result == 4:
                raise OperationCapacityExceeded(detail)
            if result == 5:
                raise InvalidOperationRequest(detail)
            raise RuntimeError(detail)

    def _check_count(self, count: int) -> None:
        if count < 0:
            message = _LIB.sts2_env_last_error(self._handle)
            raise RuntimeError((message or b"environment error").decode())

    def _result_dict(self) -> dict:
        result = _LIB.sts2_env_last_result(self._handle)
        return json.loads((result or b"{}").decode())

    def inspect_current(self) -> dict:
        """Return a read-only snapshot of this Env's current decision."""
        self._check(_LIB.sts2_env_inspect_current(self._handle))
        return self._result_dict()

    def reset_named(self, seed: str, character: str = "IRONCLAD", ascension: int = 0) -> None:
        """Reset using the canonical character name from the native descriptor."""
        if not isinstance(character, str):
            raise TypeError("character must be a string")
        self._check(_LIB.sts2_env_reset_named(
            self._handle, seed.encode(), character.upper().encode(), ascension))

    def export_oracle_combat_state(self) -> dict:
        """Return the diagnostic OracleCombat.Dump payload for a combat decision."""
        self._check(_LIB.sts2_env_export_oracle_combat_state(self._handle))
        return self._result_dict()

    def export_oracle_run_state(self) -> dict:
        """Return the diagnostic OracleRun.DumpRun payload for the current run."""
        self._check(_LIB.sts2_env_export_oracle_run_state(self._handle))
        return self._result_dict()

    def export_oracle_rewards(self) -> list[dict]:
        """Return active reward entries with their selected status."""
        self._check(_LIB.sts2_env_export_oracle_rewards(self._handle))
        return self._result_dict()

    @property
    def event_rng_counter(self) -> int:
        """Return the active event RNG counter, or -1 outside an event."""
        return int(_LIB.sts2_env_event_rng_counter(self._handle))

    def export_snapshot(self) -> dict:
        """Complete privileged state, including hidden RNG and replay state."""
        self._check(_LIB.sts2_env_export_snapshot(self._handle))
        return self._result_dict()

    def export_portable_snapshot(self) -> dict:
        """Export full state for another process with the identical native build."""
        self._check(_LIB.sts2_env_export_portable_snapshot(self._handle))
        return self._result_dict()

    @classmethod
    def restore_snapshot(cls, snapshot: dict) -> "Env":
        """Restore local snapshots in this runtime, or portable snapshots of this build."""
        payload = json.dumps(snapshot, ensure_ascii=False, separators=(",", ":"), allow_nan=False).encode()
        diagnostic = Env()
        successor = ctypes.c_void_p()
        try:
            diagnostic._check(_LIB.sts2_env_import_snapshot(diagnostic._handle, payload, len(payload), ctypes.byref(successor)))
            if not successor.value:
                raise RuntimeError("Env import returned no successor")
            branch = cls.__new__(cls)
            branch._handle = successor.value
            successor.value = None
            return branch
        finally:
            if successor.value:
                _LIB.sts2_env_destroy(successor.value)
            diagnostic.__del__()

    def validate_combat_root(self) -> None:
        """Reject active combat models with unported C++ overrides."""
        self._check(_LIB.sts2_env_validate_combat_root(self._handle))

    def predict(self, action: Action) -> "Env":
        """Return an independent successor; leave this Env unchanged on failure."""
        successor = ctypes.c_void_p()
        self._check(_LIB.sts2_env_try_transition(self._handle, action._c(), ctypes.byref(successor)))
        branch = type(self).__new__(type(self))
        branch._handle = successor.value
        return branch

    def predict_operation(self, kind: str, params: dict, *, source_hash: int | None = None,
                          stop: str = "operation_done", trace_limit: int = 16384,
                          include_future: bool = False, include_rng: bool = False,
                          include_causal: bool = False) -> tuple["Env", dict]:
        """Predict a native command on an owned branch.

        Supply the hash from the facts query for every instance reference or
        shop index. A request must retain that hash across intervening actions;
        references must never be rebound to a changed state.
        """
        references = kind in ("transform_cards", "reroll_card_reward", "merchant_purchase") or (
            kind == "obtain_relic" and "relic_ref" in params)
        if references and source_hash is None:
            raise InvalidOperationRequest("State references require their facts-query source_hash")
        if not isinstance(trace_limit, int) or not 0 <= trace_limit <= 1_000_000:
            raise ValueError("trace_limit must be an integer between 0 and 1000000")
        if not all(isinstance(flag,bool) for flag in (include_future,include_rng,include_causal)):
            raise TypeError("include_future, include_rng and include_causal must be bools")
        if (include_rng or include_causal) and trace_limit==0:
            raise ValueError("Privileged diagnostics require an enabled trace")
        request = {"format": "sts2env.operation", "version": 1, "kind": kind,
                   "source_hash": self.state_hash if source_hash is None else source_hash,
                   "stop": stop, "params": params}
        payload = json.dumps(request, ensure_ascii=False, separators=(",", ":"), allow_nan=False).encode()
        successor = ctypes.c_void_p()
        status = _LIB.sts2_env_predict_operation(self._handle, payload, len(payload), trace_limit,
                                                int(include_future) | (2 if include_rng else 0) | (4 if include_causal else 0), ctypes.byref(successor))
        try:
            self._check(status)
            if not successor.value:
                raise RuntimeError("Native operation returned no successor")
            branch = type(self).__new__(type(self))
            branch._handle = successor.value
            successor.value = None
            try:
                return branch, branch._result_dict()
            except BaseException:
                branch.__del__()
                raise
        finally:
            if successor.value:
                _LIB.sts2_env_destroy(successor.value)

    def preview(self, action: Action, trace_limit: int = 16384) -> tuple["Env", dict]:
        """Return an owned successor and its result/causal trace without changing this Env."""
        if not isinstance(trace_limit, int):
            raise TypeError("trace_limit must be an integer")
        if trace_limit < 0 or trace_limit > 1_000_000:
            raise ValueError("trace_limit must be between 0 and 1000000")

        successor = ctypes.c_void_p()
        status = _LIB.sts2_env_preview(self._handle, action._c(), trace_limit, ctypes.byref(successor))
        try:
            self._check(status)
        except BaseException:
            if successor.value:
                _LIB.sts2_env_destroy(successor.value)
            raise
        if not successor.value:
            raise RuntimeError("Env preview returned no successor")

        branch = type(self).__new__(type(self))
        branch._handle = successor.value
        successor.value = None
        try:
            result = branch._result_dict()
        except BaseException:
            branch.__del__()
            raise
        return branch, result

    def preview_with_results(self, action: Action, trace_limit: int = 16384,
                             include_future: bool = False, include_rng: bool = False,
                             include_causal: bool = False) -> tuple["Env", dict]:
        """Preview one action and return ordinary run facts before and after it."""
        if not isinstance(trace_limit, int):
            raise TypeError("trace_limit must be an integer")
        if trace_limit < 0 or trace_limit > 1_000_000:
            raise ValueError("trace_limit must be between 0 and 1000000")
        if not all(isinstance(flag,bool) for flag in (include_future,include_rng,include_causal)):
            raise TypeError("include_future, include_rng and include_causal must be bools")
        if (include_rng or include_causal) and trace_limit==0:
            raise ValueError("Privileged diagnostics require an enabled trace")

        successor = ctypes.c_void_p()
        flags = int(include_future) | (2 if include_rng else 0) | (4 if include_causal else 0)
        status = _LIB.sts2_env_preview_with_results(
            self._handle, action._c(), trace_limit, flags, ctypes.byref(successor))
        try:
            self._check(status)
        except BaseException:
            if successor.value:
                _LIB.sts2_env_destroy(successor.value)
            raise
        if not successor.value:
            raise RuntimeError("Env preview with results returned no successor")

        branch = type(self).__new__(type(self))
        branch._handle = successor.value
        successor.value = None
        try:
            result = branch._result_dict()
        except BaseException:
            branch.__del__()
            raise
        return branch, result

    def reset(self, seed: str, character: str = "IRONCLAD", ascension: int = 0) -> None:
        key = character.upper()
        if key not in _CHARACTERS:
            raise ValueError(f"unknown character: {character}")
        self._check(_LIB.sts2_env_reset(self._handle, seed.encode(), _CHARACTERS[key], ascension))

    def reset_combat(self, seed: str, encounter: int, character: str = "IRONCLAD", ascension: int = 0,
                     act_index: int = 0) -> None:
        """Start at the first combat decision for a generated EncounterId ordinal."""
        key = character.upper()
        if key not in _CHARACTERS:
            raise ValueError(f"unknown character: {character}")
        self._check(_LIB.sts2_env_reset_combat(self._handle, seed.encode(), _CHARACTERS[key], ascension,
                                               encounter, act_index))

    def inspect_combat_named(self, seed: str, encounter: str, character: str = "IRONCLAD", ascension: int = 0,
                             act_index: int = 0) -> dict:
        """Reset to a named combat root and return its structural snapshot without searching."""
        key = character.upper()
        if key not in _CHARACTERS:
            raise ValueError(f"unknown character: {character}")
        self._check(_LIB.sts2_env_inspect_combat_named(
            self._handle, seed.encode(), key.encode(), ascension, encounter.upper().encode(), act_index))
        return self._result_dict()

    def export_combat_queries(self) -> dict:
        """Export read-only combat query values for this ordinary Env."""
        self._check(_LIB.sts2_env_export_combat_queries(self._handle))
        return self._result_dict()

    def export_run_queries(self, include_future: bool = False) -> dict:
        """Export ordinary run facts; future privileged facts are opt-in."""
        if not isinstance(include_future, bool):
            raise TypeError("include_future must be a bool")
        flags = 1 if include_future else 0
        self._check(_LIB.sts2_env_export_run_queries(self._handle, flags))
        return self._result_dict()

    def export_calculated_vars(self) -> dict:
        """Export calculated card and power variables for this ordinary Env."""
        self._check(_LIB.sts2_env_export_calculated_vars(self._handle))
        return self._result_dict()

    def export_orbs(self) -> dict:
        """Export the current combat orbs for this ordinary Env."""
        self._check(_LIB.sts2_env_export_orbs(self._handle))
        return self._result_dict()

    def export_powers(self) -> dict:
        """Export the current combat powers for this ordinary Env."""
        self._check(_LIB.sts2_env_export_powers(self._handle))
        return self._result_dict()

    @property
    def decision(self) -> int:
        return int(_LIB.sts2_env_decision(self._handle))

    @property
    def state_hash(self) -> int:
        return int(_LIB.sts2_env_hash(self._handle))

    def legal_actions(self) -> list[Action]:
        return [Action._from_c(value) for value in self._read_buffer(_LIB.sts2_env_legal_actions, _CAction)]

    def action_mask(self) -> list[int]:
        return list(self._read_buffer(_LIB.sts2_env_action_mask, ctypes.c_uint8))

    def observe(self) -> list[int]:
        return list(self._read_buffer(_LIB.sts2_env_observe, ctypes.c_int32))

    def _read_buffer(self, function, element_type):
        count = function(self._handle, None, 0)
        self._check_count(count)
        buffer = (element_type * count)()
        written = function(self._handle, buffer, count)
        self._check_count(written)
        return buffer[:written]

    def step(self, action: Action) -> None:
        self._check(_LIB.sts2_env_step(self._handle, action._c()))

    def determinize(self, seed: int) -> None:
        self._check(_LIB.sts2_env_determinize(self._handle, seed))

    def clone(self) -> "Env":
        copy = object.__new__(Env)
        copy._handle = _LIB.sts2_env_clone(self._handle)
        if not copy._handle:
            raise RuntimeError("sts2_env_clone failed")
        return copy


class VectorEnv:
    def __init__(self, count: int, character: str = "IRONCLAD", ascension: int = 0):
        self.envs = [Env() for _ in range(count)]
        self.character = character
        self.ascension = ascension

    def reset(self, seeds: list[str]) -> None:
        if len(seeds) != len(self.envs):
            raise ValueError("seed count does not match VectorEnv")
        for env, seed in zip(self.envs, seeds):
            env.reset(seed, self.character, self.ascension)

    def step(self, index: int, action: Action) -> None:
        self.envs[index].step(action)

    def observations(self) -> list[list[int]]:
        return [env.observe() for env in self.envs]

    def hashes(self) -> list[int]:
        return [env.state_hash for env in self.envs]
