"""Bind C++ observations to unchanged shipped-DLL signature metadata."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
data = json.loads((ROOT / "data/execution_signatures.json").read_text())
if (data["format"], data["version"], data["game_version"], data["game_dll_sha256"]) != (
    "sts2oracle.execution_signatures", 1, "v0.111.0",
    "2b40d2df538db1ceb5fa48d958c80ab730ada1e07db88a870aff01a661768b9f",
):
    raise ValueError("Execution signatures are not qualified for the pinned game DLL")


def resolve(owner, name, argument_names):
    candidates = [m for m in data["methods"] if m["owner"] == owner and m["name"] in (name, "get_" + name)
                  and len(m["parameters"]) == len(argument_names)]
    if len(candidates) != 1:
        raise ValueError(f"Cannot uniquely bind actual method signature: {owner}.{name}")
    method = candidates[0]
    return {**method, "parameters": [{**p, "cpp_name": arg} for p, arg in zip(method["parameters"], argument_names)]}


def capture_arguments(binding, after=False):
    values = []
    for p in binding["parameters"]:
        flags = (1 if p["is_out"] else 0) | (2 if p["is_by_ref"] else 0) | (4 if p["is_in"] else 0)
        if p["is_out"] and not after:
            value = "UndefinedExecutionValue()"
        else:
            value = f'FreezeExecutionValue({p["cpp_name"]})'
        values.append(f'ExecutionArgument{{"{p["name"]}", "{p["type"]}", {flags}, {value}}}')
    return "{" + ", ".join(values) + "}"
