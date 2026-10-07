"""ctypes binding of the NoSL sampler ABI (nosl/batch_api.h)."""
from __future__ import annotations

import ctypes
import os
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
SILENT = 8  # CharacterId::SILENT

RUNNING, VICTORY, DEATH, UNPORTED, ERROR, CAPACITY, STEP_LIMIT = range(7)
STATUS_NAMES = ("running", "victory", "death", "unported", "error", "capacity", "step_limit")


def _library() -> ctypes.CDLL:
    path = Path(os.environ.get("STS2SIM_NOSL_LIB", ROOT / "build" / "libsts2nosl_c.so"))
    if not path.exists():
        raise FileNotFoundError(f"{path}; build target sts2nosl_c or set STS2SIM_NOSL_LIB")
    return ctypes.CDLL(str(path))


_LIB = _library()
_I32P = ctypes.POINTER(ctypes.c_int32)
_LIB.nosl_obs_shape.argtypes = [_I32P]
_LIB.nosl_batch_create.argtypes = [ctypes.c_int32] * 4
_LIB.nosl_batch_create.restype = ctypes.c_void_p
_LIB.nosl_batch_destroy.argtypes = [ctypes.c_void_p]
_LIB.nosl_batch_reset.argtypes = [ctypes.c_void_p, ctypes.c_int32, ctypes.c_char_p, ctypes.c_int32, ctypes.c_int32]
_LIB.nosl_batch_step.argtypes = [ctypes.c_void_p, _I32P, ctypes.c_int32] + [_I32P] * 6
_LIB.nosl_batch_set_active.argtypes = [ctypes.c_void_p, ctypes.c_int32]
_LIB.nosl_batch_swap.argtypes = [ctypes.c_void_p, ctypes.c_int32, ctypes.c_int32]
_LIB.nosl_batch_fork.argtypes = [ctypes.c_void_p, ctypes.c_int32, ctypes.c_int32]
_LIB.nosl_batch_set_filter.argtypes = [ctypes.c_void_p, ctypes.c_int32]
_LIB.nosl_batch_run_info.argtypes = [ctypes.c_void_p, ctypes.c_int32, _I32P]
_LIB.nosl_batch_last_error.argtypes = [ctypes.c_void_p, ctypes.c_int32]
_LIB.nosl_batch_last_error.restype = ctypes.c_char_p

_shape = (ctypes.c_int32 * 4)()
_LIB.nosl_obs_shape(_shape)
SCHEMA_VERSION, GLOBAL_WIDTH, TOKEN_WIDTH, ACTION_WIDTH = (int(v) for v in _shape)

# Global columns (nosl/observation.h, GlobalColumn).
G_DECISION, G_ACT, G_ACT_FLOOR, G_TOTAL_FLOOR, G_HP, G_MAX_HP = 1, 4, 5, 6, 7, 8
RUN_INFO_FIELDS = ("act", "floor", "hp", "max_hp", "gold", "deck", "relics", "last_encounter")


def _ptr(array: np.ndarray):
    if array.dtype != np.int32 or not array.flags["C_CONTIGUOUS"]:
        raise ValueError("sampler buffers must be C-contiguous int32")
    return array.ctypes.data_as(_I32P)


class Batch:
    """`runs` independent runs. Not thread-safe; use one per process."""

    def __init__(self, runs: int, max_tokens: int, max_actions: int, max_steps: int):
        self.runs, self.max_tokens, self.max_actions = runs, max_tokens, max_actions
        self._handle = _LIB.nosl_batch_create(runs, max_tokens, max_actions, max_steps)
        if not self._handle:
            raise ValueError("nosl_batch_create rejected its arguments")

    def __del__(self):
        handle = getattr(self, "_handle", None)
        if handle:
            _LIB.nosl_batch_destroy(handle)
            self._handle = None

    def reset(self, index: int, seed: str, ascension: int, character: int = SILENT) -> int:
        return int(_LIB.nosl_batch_reset(self._handle, index, seed.encode(), character, ascension))

    def step(self, choice, global_, tokens, actions, counts, status, steps, auto_forced: bool = True) -> None:
        if _LIB.nosl_batch_step(self._handle, _ptr(choice), int(auto_forced), _ptr(global_), _ptr(tokens),
                                _ptr(actions), _ptr(counts), _ptr(status), _ptr(steps)) != 0:
            raise RuntimeError("nosl_batch_step rejected its arguments")

    def set_active(self, active: int) -> None:
        """Step and write only the first `active` slots; the rest are spares."""
        if _LIB.nosl_batch_set_active(self._handle, active) != 0:
            raise ValueError("bad active slot count")

    def set_filter(self, version: int) -> None:
        """The actor filter (nosl/actor_filter.h) of runs reset from now on."""
        if _LIB.nosl_batch_set_filter(self._handle, version) != 0:
            raise ValueError("unknown actor filter version")

    def swap(self, a: int, b: int) -> None:
        if _LIB.nosl_batch_swap(self._handle, a, b) != 0:
            raise ValueError("bad slot index")

    def fork(self, source: int, destination: int) -> int:
        """Offline training only: independent Env/filter/observation continuation."""
        status = int(_LIB.nosl_batch_fork(self._handle, source, destination))
        if status < 0:
            raise ValueError("fork requires distinct valid slots and a running source")
        return status

    def run_info(self, index: int) -> dict:
        out = (ctypes.c_int32 * 8)()
        _LIB.nosl_batch_run_info(self._handle, index, out)
        return dict(zip(RUN_INFO_FIELDS, (int(v) for v in out)))

    def last_error(self, index: int) -> str:
        return (_LIB.nosl_batch_last_error(self._handle, index) or b"").decode(errors="replace")
