"""Run a nosl module's command line without making every sampler worker import PyTorch.

  .venv/bin/python -m nosl.run strategy_q collect --out DIR ...

The sampler's worker processes are spawned, and a spawned process imports the
program's main module again. When that module is nosl.strategy_q or
nosl.combat_ppo, each worker loads PyTorch only to step a native batch, about
300 MiB per worker for nothing. This module imports nothing heavy at import
time, so a worker started through it holds numpy and the native library only.
"""
import importlib
import sys

if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise SystemExit("usage: python -m nosl.run MODULE [arguments of that module]")
    importlib.import_module("nosl." + sys.argv[1]).main(sys.argv[2:])
