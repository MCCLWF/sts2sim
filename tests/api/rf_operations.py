#!/usr/bin/env python3
"""Strict shipped-DLL differential consumer of the typed public Env C ABI.

Run after building libsts2env_c.so and Sts2Oracle, for example:
  python3 tests/api/rf_operations.py --dotnet ~/.dotnet/dotnet

The oracle supplies every generated result and selection. This consumer only
translates those selections into public Answer actions and checks each prefix,
clone, v4 snapshot restore, source immutability and stale-request rejection.
Raw outputs and frozen runtime/source fingerprints stay under artifacts by default.
"""
from __future__ import annotations

import argparse
import ctypes
import importlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import traceback

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "differential"))
from batch import digest, provenance

STATUS = {0: "ok", 1: "unported", 2: "trace_limit", 3: "unsupported", 4: "capacity", 5: "invalid_request", -1: "error"}
ROOM_TYPES = ["Unassigned", "Monster", "Elite", "Boss", "Treasure", "Shop", "Event", "RestSite", "Map"]
RNG_TRACE = False  # Only enabled by the explicit diagnostic CLI option.
CAUSAL_TRACE = False  # Includes hidden model identities and listener order.


def equal(expected, actual, path="value"):
    """No scalar coercion: bool/int, enum and decimal bit differences fail."""
    if type(expected) is not type(actual):
        raise AssertionError(f"{path}: expected {type(expected).__name__} {expected!r}, got {type(actual).__name__} {actual!r}")
    if isinstance(expected, dict):
        if expected.keys() != actual.keys():
            raise AssertionError(f"{path}: keys differ: expected {list(expected)}, got {list(actual)}")
        for key in expected:
            equal(expected[key], actual[key], f"{path}.{key}")
    elif isinstance(expected, list):
        if len(expected) != len(actual):
            raise AssertionError(f"{path}: expected {len(expected)} entries, got {len(actual)}")
        for index, (a, b) in enumerate(zip(expected, actual)):
            equal(a, b, f"{path}[{index}]")
    elif expected != actual:
        raise AssertionError(f"{path}: expected {expected!r}, got {actual!r}")


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2, allow_nan=False) + "\n")


def validate_rng_trace(preview, enabled):
    equal(enabled, preview["rng_trace_enabled"], "rng.enabled")
    equal("privileged_diagnostic", preview["rng_trace_visibility"], "rng.visibility")
    equal(False, preview["trace_complete"], "rng.trace_complete")
    calls = preview["rng_calls"]
    if not enabled:
        equal([], calls, "rng.disabled_calls")
        return
    sequences = [e["sequence"] for family in ("events", "power_events") for e in preview[family]]
    children = {}
    for index, call in enumerate(calls):
        equal(index, call["call"], "rng.call")
        equal(True, call["finished"], "rng.finished")
        if not call["sequence"] < call["finished_sequence"]:
            raise AssertionError("RNG call finished before it began")
        sequences.extend((call["sequence"], call["finished_sequence"]))
        parent = call["parent_call"]
        if parent >= index or parent < -1:
            raise AssertionError("RNG parent does not precede child")
        if parent >= 0:
            owner = calls[parent]
            if not owner["sequence"] < call["sequence"] < call["finished_sequence"] < owner["finished_sequence"]:
                raise AssertionError("RNG child outside parent lifetime")
            equal(owner["frame"], preview["frames"][call["frame"]]["parent"], "rng.causal_parent")
            children.setdefault(parent, []).append(call)
    if len(set(sequences)) != len(sequences):
        raise AssertionError("RNG/event sequence reused")
    for parent, nested in children.items():
        equal(calls[parent]["before"], nested[0]["before"], "rng.first_child_state")
        equal(calls[parent]["after"], nested[-1]["after"], "rng.last_child_state")
        for first, second in zip(nested, nested[1:]):
            equal(first["after"], second["before"], "rng.sibling_state_chain")


def validate_causal_trace(preview, enabled):
    equal(enabled, preview["causal_trace_enabled"], "causal.enabled")
    equal("privileged_diagnostic", preview["causal_trace_visibility"], "causal.visibility")
    equal(False, preview["trace_complete"], "causal.trace_complete")
    frames = preview["frames"]
    sequences = [e["sequence"] for family in ("events", "power_events") for e in preview[family]]
    sequences += [s for c in preview["rng_calls"] for s in (c["sequence"], c["finished_sequence"])]
    for i, frame in enumerate(frames):
        equal(True, frame["finished"], "causal.finished")
        if not -1 <= frame["parent"] < i or not frame["sequence"] < frame["finished_sequence"]:
            raise AssertionError("Invalid frame parent/lifetime")
        if frame["parent"] >= 0:
            parent = frames[frame["parent"]]
            if not parent["sequence"] < frame["sequence"] < frame["finished_sequence"] < parent["finished_sequence"]:
                raise AssertionError("Causal frame outside parent lifetime")
        if frame["kind"] in ("hook", "model_method") and not enabled:
            raise AssertionError("Default trace leaked privileged listener order")
        if frame["kind"] == "model_method" and not all(frame[key] for key in ("source_model_id", "source_model_kind", "source_ref_kind")):
            raise AssertionError("Model frame did not freeze identity/reference kind")
        if frame["source_is_canonical"]:
            equal(True, frame["source_is_canonical_known"], "causal.canonical_fact_known")
            equal("model_method", frame["kind"], "causal.canonical_frame_kind")
            equal("canonical_definition", frame["source_ref_kind"], "causal.canonical_reference_kind")
            equal(0, frame["source_ref"], "causal.canonical_reference")
        elif frame["source_ref_kind"] == "canonical_definition":
            raise AssertionError("Mutable frame fabricated canonical definition identity")
        method = frame["kind"] in ("hook", "model_method")
        equal(method, frame["method_values_observed"], "causal.method_values_observed")
        if method:
            if not frame["declared_return_type"]:
                raise AssertionError("Method omitted shipped-DLL return declaration")
            equal(not frame["unwound"], frame["method_returned"], "causal.method_returned")
            if frame["unwound"]:
                equal([], frame["parameters_after"], "causal.unwound_after")
                equal(None, frame["return_value"], "causal.unwound_result")
            else:
                if frame["return_value"] is None:
                    raise AssertionError("Completed method omitted result")
                equal(len(frame["parameters_before"]), len(frame["parameters_after"]), "causal.parameter_count")
            for before in frame["parameters_before"]:
                if before["is_out"]:
                    equal({"kind": "undefined"}, before["value"], "causal.undefined_out")
        else:
            equal(False, frame["method_returned"], "causal.nonmethod_returned")
            equal([], frame["parameters_before"], "causal.nonmethod_parameters")
            equal(None, frame["return_value"], "causal.nonmethod_result")
        sequences.extend((frame["sequence"], frame["finished_sequence"]))
    if len(set(sequences)) != len(sequences):
        raise AssertionError("Causal/event/RNG sequence reused")


def item(value):
    """Compare the entire public instance payload, apart from runtime-local refs."""
    if value is None:
        return None
    result = {k: v for k, v in value.items() if k not in ("model_id", "static_usable", "card_ref", "relic_ref", "potion_ref")}
    result["id"] = value["model_id"]["entry"]
    return result


def state(env, queries=None):
    q = queries if queries is not None else env.export_run_queries()
    snapshot = env.export_snapshot()
    run = snapshot["state"]["run"]
    result = {
        "act_index": q["act_index"], "act_floor": q["act_floor"], "total_floor": q["total_floor"],
        "room_type": ROOM_TYPES[q["room_type"]], "event_id": q["event"]["entry"] if q["event"] else None,
        "hp": q["player"]["hp"], "max_hp": q["player"]["max_hp"], "gold": q["player"]["gold"],
        "deck": [item(c) for c in q["player"]["deck"]],
        "relics": [item(r) for r in q["player"]["relics"]],
        "potions": [item(p["item"]) for p in q["player"]["potions"]],
        "rng": dict(zip(snapshot["run_rng_streams"], run["rng"]["rngs"])),
        "player_rng": dict(zip(snapshot["player_rng_streams"], run["player"]["rng"]["rngs"])),
        "event_rng": run["events"][0]["rng"] if q["event"] else None,
    }
    if q["shop"] is not None:
        result["shop"] = {"card_removal_used": q["shop"]["card_removal_used"],
                          "entries": [dict(e, item=item(e["item"])) for e in q["shop"]["entries"]]}
    if q["event"] is not None and q["event"]["entry"] == "CRYSTAL_SPHERE":
        result["crystal_sphere"] = q["event"]["crystal_sphere"]
    return result



def reward(value):
    kind = value["type"]
    if kind == 4:
        result = {"type": "Relic", "relic": item(value["relic"])}
    elif kind == 3:
        result = {"type": "Potion", "potion": item(value["potion"])}
    elif kind == 1:
        result = {"type": "Card", "cards": [item(c) for c in value["cards"]]}
    elif kind == 2:
        result = {"type": "Gold", "amount": value["gold"]}
    else:
        raise AssertionError(f"Unsupported oracle comparison reward type {kind}")
    result["selected"] = value["selected"]
    return result


def request(env, oracle):
    params = oracle["params"]
    if oracle["kind"] == "transform_cards":
        deck = env.export_run_queries()["player"]["deck"]
        params = {"cards": [deck[i]["card_ref"] for i in params["deck_indices"]],
                  "rng_source": params["rng_source"], "mode": params["mode"]}
    return {"format": "sts2env.operation", "version": 1, "source_hash": env.state_hash,
            "kind": oracle["kind"], "params": params, "stop": "operation_done"}


def native_operation(env, operation, trace_limit=16384, expected_status=0):
    before = env.export_snapshot()
    payload = json.dumps(operation, separators=(",", ":"), allow_nan=False).encode()
    output = ctypes.c_void_p(1)  # Failures must explicitly clear this out-pointer.
    status = LIB.sts2_env_predict_operation(env._handle, payload, len(payload), trace_limit,
                                          ((2 if RNG_TRACE else 0) | (4 if CAUSAL_TRACE else 0)) if trace_limit else 0,
                                          ctypes.byref(output))
    detail = (LIB.sts2_env_last_error(env._handle) or b"").decode()
    if status != expected_status:
        if output.value and output.value != 1:
            LIB.sts2_env_destroy(output.value)
        raise AssertionError(f"operation returned {STATUS.get(status, status)}; expected {STATUS.get(expected_status, expected_status)}: {detail}")
    equal(before, env.export_snapshot(), "operation.source_snapshot")
    if status:
        equal(None, output.value, "failed_operation.successor")
        if not detail:
            raise AssertionError("Failed operation supplied no owned diagnostic")
        return None, {"status": status, "diagnostic": detail}
    if not output.value or output.value == env._handle:
        raise AssertionError("Successful operation did not return an independent owned Env")
    branch = Env.__new__(Env)
    branch._handle = output.value
    result = branch._result_dict()  # Copy before any read overwrites diagnostics.
    validate_rng_trace(result, RNG_TRACE and trace_limit != 0)
    validate_causal_trace(result, CAUSAL_TRACE and trace_limit != 0)
    equal(operation, result["operation"], "preview.operation")
    equal(env.state_hash, result["source_hash"], "preview.source_hash")
    equal(branch.state_hash, result["successor_hash"], "preview.successor_hash")
    equal("sts2env.preview", result["format"], "preview.format")
    equal(2, result["version"], "preview.version")
    equal("operation_prefix", result["trace_semantics"], "preview.trace_semantics")
    equal(env.state_hash, result["run_results"]["before"]["state_hash"], "preview.before_hash")
    equal(branch.state_hash, result["run_results"]["after"]["state_hash"], "preview.after_hash")
    return branch, result


def answer(env, index, operation):
    before = env.export_snapshot()
    action = Action(kind=12, index=index)
    if action not in env.legal_actions():
        raise AssertionError(f"Oracle Answer {index} is not legal: {env.legal_actions()}")
    successor, preview = env.preview_with_results(action, include_rng=RNG_TRACE, include_causal=CAUSAL_TRACE)
    validate_rng_trace(preview, RNG_TRACE)
    validate_causal_trace(preview, CAUSAL_TRACE)
    equal(before, env.export_snapshot(), "answer.source_snapshot")
    if successor._handle == env._handle:
        raise AssertionError("Answer preview aliased the source")
    predicted = env.predict(action)
    try:
        equal(successor.export_snapshot(), predicted.export_snapshot(), "preview_vs_predict")
    finally:
        predicted.__del__()
    equal(True, preview["choice_replayed"], "answer.choice_replayed")
    equal(operation, preview["continuation_of"], "answer.pending_operation")
    return successor, preview


def restored_prefix(env, operation, answers, directory, prefix):
    snapshot = env.export_snapshot()
    equal(4, snapshot["version"], "snapshot.version")
    equal(operation, snapshot["pending_operation"], "snapshot.pending_operation")
    equal(None, snapshot["pending"], "snapshot.pending_action")
    equal(answers, snapshot["answers"], "snapshot.answers")
    if snapshot["replay_base"] is None:
        raise AssertionError("Pending operation has no replay base")
    clone = env.clone()
    restored = Env.restore_snapshot(snapshot)
    portable = env.export_portable_snapshot()
    portable_restored = Env.restore_snapshot(portable)
    try:
        equal(snapshot, clone.export_snapshot(), "clone.snapshot")
        equal(snapshot, restored.export_snapshot(), "restore.snapshot")
        equal(env.export_run_queries(), restored.export_run_queries(), "restore.queries")
        equal(env.legal_actions(), restored.legal_actions(), "restore.legal_actions")
        equal(portable, portable_restored.export_portable_snapshot(), "portable_restore.canonical_snapshot")
        equal(env.export_run_queries(), portable_restored.export_run_queries(), "portable_restore.queries")
        equal(env.legal_actions(), portable_restored.legal_actions(), "portable_restore.legal_actions")
        # A source-bound request must become stale at every operation prefix.
        _, stale = native_operation(restored, operation, expected_status=5)
        if "stale source_hash" not in stale["diagnostic"]:
            raise AssertionError(f"Wrong stale-prefix diagnostic: {stale}")
        write_json(directory / f"prefix-{prefix:03d}.json", {"snapshot": snapshot, "portable": portable,
            "queries": env.export_run_queries(), "stale_request": stale})
    except BaseException:
        portable_restored.__del__()
        raise
    finally:
        clone.__del__()
        restored.__del__()
    return portable_restored


def save_operation_state(env, directory, name, operation=None):
    snapshot = env.export_snapshot()
    document = {"format": "sts2env.rf_operation_state", "version": 1,
                "snapshot": snapshot, "portable": env.export_portable_snapshot(),
                "queries": env.export_run_queries()}
    if operation is not None:
        document["operation"] = operation
    equal(snapshot, env.export_snapshot(), f"{name}.read_only")
    write_json(directory / f"{name}.json", document)


def execute_checked(env, operation, choices, directory, handles, selector_source="FromDeckForTransformation"):
    save_operation_state(env, directory, "initial", operation)
    answers, previews = [], []
    stale = dict(operation, source_hash=operation["source_hash"] ^ 1)
    _, stale_result = native_operation(env, stale, expected_status=5)
    if "stale source_hash" not in stale_result["diagnostic"]:
        raise AssertionError("Stale request rejected for an unrelated reason")
    _, unsupported = native_operation(env, dict(operation, kind="rf_unknown_operation"), expected_status=3)
    _, invalid = native_operation(env, dict(operation, version=999), expected_status=5)
    branch, first = native_operation(env, operation)
    equal("needs_choice" if choices else "complete", first["operation_status"], "first.operation_status")
    handles.append(branch)
    previews.append(first)
    plain, plain_result = native_operation(env, operation, trace_limit=0)
    try:
        equal(branch.export_snapshot(), plain.export_snapshot(), "trace_on_vs_off")
        equal(first["operation_result"], plain_result["operation_result"], "trace_on_vs_off.operation_result")
        equal(False, plain_result["trace_enabled"], "trace_disabled")
        equal([], plain_result["frames"], "trace_disabled.frames")
        equal([], plain_result["events"], "trace_disabled.events")
    finally:
        plain.__del__()
    env = branch
    prefix = 0

    def advance(index, expected_choice_state):
        nonlocal env, prefix
        equal(7, env.decision, "prefix.decision")
        equal(expected_choice_state, state(env), f"prefix[{prefix}].state")
        restored = restored_prefix(env, operation, answers, directory, prefix)
        handles.append(restored)
        successor, preview = answer(restored, index, operation)
        handles.append(successor)
        previews.append(preview)
        answers.append(index)
        env = successor
        prefix += 1

    for choice_index, original in enumerate(choices):
        q = env.export_run_queries()
        c = q["choice"]
        if c is None:
            raise AssertionError(f"Oracle choice {choice_index} has no native Choice")
        if original["kind"] == "card_select":
            options = [item(o["cards"][0]) for o in c["options"]]
            equal(original["options"], options, "card_select.options")
            equal("card_select", c["kind"], "card_select.kind")
            equal(True, c["subset"], "card_select.subset")
            if selector_source is not None:
                equal(selector_source, c["source"], "card_select.source")
            equal(original["min_select"], c["min_select"], "card_select.min")
            equal(original["max_select"], c["max_select"], "card_select.max")
            equal("count", c["prompt"], "card_select.prompt")
            equal([], c["selected"], "card_select.selected")
            equal(list(range(original["min_select"], min(original["max_select"], len(options)) + 1)), c["legal_answers"], "card_select.count_answers")
            refs = [o["cards"][0]["card_ref"] for o in c["options"]]
            if len(set(refs)) != len(refs):
                raise AssertionError("Card selector collapsed distinct instances")
            advance(len(original["selected_indices"]), original["state"])
            selected = []
            for index in original["selected_indices"]:
                c = env.export_run_queries()["choice"]
                equal("index", c["prompt"], "card_select.index_prompt")
                equal(selected, c["selected"], "card_select.prefix")
                equal(options, [item(o["cards"][0]) for o in c["options"]], "card_select.replayed_options")
                equal(refs, [o["cards"][0]["card_ref"] for o in c["options"]], "card_select.replayed_instance_refs")
                equal([i for i in range(len(options)) if i not in selected], c["legal_answers"], "card_select.index_answers")
                advance(index, original["state"])
                selected.append(index)
        elif original["kind"] == "crystal_sphere":
            equal("option", c["kind"], "crystal.kind")
            equal("crystal_sphere", c["source"], "crystal.source")
            keys = [f"CRYSTAL_SPHERE.cell.{x}.{y}" for y in range(11) for x in range(11)]
            keys += ["CRYSTAL_SPHERE.button.DIVINATION_LABEL_SMALL", "CRYSTAL_SPHERE.button.DIVINATION_LABEL_BIG"]
            equal(keys, [o["key"] for o in c["options"]], "crystal.options")
            equal(["alternative"] * 123, [o["kind"] for o in c["options"]], "crystal.option_kinds")
            equal(list(range(123)), c["legal_answers"], "crystal.legal_answers")
            advance(original["selected_index"], original["state"])
        elif original["kind"] == "rewards":
            equal("option", c["kind"], "rewards.kind")
            equal("rewards", c["source"], "rewards.source")
            expected_kinds = ["reward"] * len(original["options"]) + ["proceed"]
            equal(expected_kinds, [o["kind"] for o in c["options"]], "rewards.option_kinds")
            equal(original["options"], [reward(o["reward"]) for o in c["options"] if o["kind"] == "reward"], "rewards.options")
            equal([not r["selected"] for r in original["options"]] + [original["can_proceed"]],
                  [o["enabled"] for o in c["options"]], "rewards.enabled")
            legal = [i for i, r in enumerate(original["options"]) if not r["selected"]]
            if original["can_proceed"]:
                legal.append(len(original["options"]))
            equal(legal, c["legal_answers"], "rewards.legal_answers")
            advance(original["selected_index"], original["state"])
        elif original["kind"] == "card_reward":
            equal("CardSelectCmd", c["source"], "card_reward.source")
            equal(original["options"], [item(o["cards"][0]) for o in c["options"] if o["kind"] == "card"], "card_reward.cards")
            alternatives = [o["key"] for o in c["options"] if o["kind"] == "alternative"]
            equal(original["alternatives"], alternatives, "card_reward.alternatives")
            equal(original["enabled_alternatives"], [o["enabled"] for o in c["options"] if o["kind"] == "alternative"],
                  "card_reward.enabled_alternatives")
            legal = list(range(len(original["options"]))) + [len(original["options"]) + i
                for i, enabled in enumerate(original["enabled_alternatives"]) if enabled]
            equal(legal, c["legal_answers"], "card_reward.legal_answers")
            for i, enabled in enumerate(original["enabled_alternatives"]):
                if not enabled:
                    disabled = Action(kind=12, index=len(original["options"]) + i)
                    previous = env.export_snapshot()
                    try:
                        invalid_branch = env.predict(disabled)
                    except RuntimeError:
                        pass
                    else:
                        invalid_branch.__del__()
                        raise AssertionError("Removed UI alternative returned an executable branch")
                    equal(previous, env.export_snapshot(), "disabled_alternative.source")
            advance(original["selected_index"], original["state"])
        else:
            raise AssertionError(f"Unrecognized oracle Choice {original['kind']}")

    save_operation_state(env, directory, "completed")
    return env, previews, answers, prefix, {"stale": stale_result, "unsupported": unsupported, "invalid": invalid}


def check_case(oracle, directory):
    env = Env(oracle["seed"], oracle["character"], oracle["ascension"])
    handles = [env]
    answers = []
    previews = []
    try:
        equal("new_run_enter_act_0_unchosen_neow", oracle["fixture"], "oracle.fixture")
        equal("sts2oracle.env_operation", oracle["format"], "oracle.format")
        equal(2, oracle["version"], "oracle.version")
        equal(ORACLE_TEST_MODE, oracle["oracle_mode"]["test_mode"], "oracle.test_mode")
        equal("event", env.export_run_queries()["decision_name"], "fixture.decision")
        equal(oracle["initial"], state(env), "fixture.initial")
        for setup_index, setup in enumerate(oracle["setup"]):
            equal(setup["before"], state(env), f"setup[{setup_index}].before")
            setup_request = {"format": "sts2env.operation", "version": 1, "source_hash": env.state_hash,
                             "kind": "obtain_relic", "stop": "operation_done",
                             "params": {"relic_id": setup["relic_id"], "insert_index": -1}}
            env, setup_result = native_operation(env, setup_request)
            handles.append(env)
            equal("complete", setup_result["operation_status"], f"setup[{setup_index}].status")
            equal(setup["after"], state(env), f"setup[{setup_index}].after")
        equal(oracle["source"], state(env), "source")
        operation = request(env, oracle)
        env, previews, answers, prefix, checks = execute_checked(env, operation, oracle["choices"], directory, handles)

        # Strictly reject an extra Proceed after the oracle's final custom reward.
        equal(3, env.decision, "final.decision_at_unchosen_neow")
        equal("complete", previews[-1]["operation_status"], "final.operation_status")
        equal(None, env.export_run_queries()["choice"], "final.choice")
        equal(oracle["final"], state(env), "final")
        final_snapshot = env.export_snapshot()
        equal(None, final_snapshot["pending_operation"], "final.pending_operation")
        equal([], final_snapshot["answers"], "final.answers")
        restored_final = Env.restore_snapshot(final_snapshot)
        try:
            equal(final_snapshot, restored_final.export_snapshot(), "final.restored_snapshot")
        finally:
            restored_final.__del__()
        write_json(directory / "final.json", {"snapshot": final_snapshot, "state": state(env), "answers": answers})
        write_json(directory / "previews.json", previews)
        return {"seed": oracle["seed"], "character": oracle["character"], "ascension": oracle["ascension"],
                "case_id": oracle["case_id"], "kind": oracle["kind"],
                "relic_id": oracle["relic_id"], "passed": True, "oracle_choices": len(oracle["choices"]),
                "native_prefixes": prefix, "answers": answers, **checks,
                "trace_complete": previews[-1]["trace_complete"]}
    finally:
        for handle in reversed(handles):
            handle.__del__()


def freeze(args, library, host):
    result = provenance(ROOT / "build" / "sts2sim_cli", args.oracle, True, dotnet=str(host))
    if args.oracle_test_mode == "off":
        result["oracle_qualification"] = "TestMode=false shipped DLL with scoped presentation/input adapters; fixture scope, not live capture"
    result["runtime"]["env_library"] = {"path": str(library), "sha256": digest(library)}
    if args.operation == "merchant_purchase":
        result["runtime"]["shop_fixture_library"] = {"path": str(args.shop_fixture), "sha256": digest(args.shop_fixture)}
    return result


def check_merchant_case(oracle, directory, fixture):
    equal("sts2oracle.env_operation", oracle["format"], "oracle.format")
    equal(3, oracle["version"], "oracle.version")
    equal("new_run_map_start_shop_override", oracle["fixture"], "oracle.fixture")
    equal(ORACLE_TEST_MODE, oracle["oracle_mode"]["test_mode"], "oracle.test_mode")
    config = {"seed": oracle["seed"], "character": oracle["character"], "ascension": oracle["ascension"],
              "gold": oracle["fixture_gold"], "setup_relics": [r["relic_id"] for r in oracle["setup"]]}
    payload = json.dumps(config, separators=(",", ":")).encode()
    diagnostics = Env()
    output = ctypes.c_void_p()
    try:
        status = fixture.sts2_test_shop_fixture(diagnostics._handle, payload, len(payload), ctypes.byref(output))
        if status or not output.value:
            raise AssertionError(f"Test fixture failed ({status}): {(LIB.sts2_env_last_error(diagnostics._handle) or b'').decode()}")
    finally:
        diagnostics.__del__()
    env = Env.__new__(Env)
    env._handle = output.value
    handles = [env]
    prefixes, results, native_failures, preflight_rejections = 0, [], 0, 0
    try:
        equal(5, env.decision, "shop.source_decision")
        equal(oracle["source"], state(env), "shop.source")
        equal(oracle["params"]["entry_indices"], [s["entry_index"] for s in oracle["steps"]], "shop.steps")
        for index, step in enumerate(oracle["steps"]):
            step_dir = directory / f"step-{index:03d}"
            step_dir.mkdir()
            equal(step["before"], state(env), f"purchase[{index}].before")
            entry = env.export_run_queries()["shop"]["entries"][step["entry_index"]]
            equal(step["entry_index"], entry["entry_index"], "shop.bound_slot")
            operation = {"format": "sts2env.operation", "version": 1, "source_hash": env.state_hash,
                         "kind": "merchant_purchase", "stop": "operation_done",
                         "params": {"entry_index": step["entry_index"], "ignore_cost": False}}
            if not entry["buyable"]:
                # An illegal caller request is distinct from a valid native
                # attempt that fails (e.g. a potion blocked by full slots).
                equal(False, step["success"], "shop.preflight_native_failure")
                equal([], step["choices"], "shop.preflight_choices")
                save_operation_state(env, step_dir, "initial", operation)
                _, diagnostic = native_operation(env, operation, expected_status=5)
                equal(step["after"], state(env), "shop.preflight_unchanged")
                save_operation_state(env, step_dir, "completed")
                results.append({"operation": operation, "rejected_request": diagnostic})
                preflight_rejections += 1
                continue
            selector_source = "FromDeckGeneric" if entry["kind"] == "card_removal" else None
            env, previews, answers, count, checks = execute_checked(env, operation, step["choices"], step_dir,
                                                                  handles, selector_source)
            prefixes += count
            equal("complete", previews[-1]["operation_status"], "shop.operation_complete")
            equal({"kind": "merchant_purchase", "entry_index": step["entry_index"],
                   "success": step["success"], "ignore_cost": False}, previews[-1]["operation_result"], "shop.native_return")
            for preview in previews[:-1]:
                equal(None, preview["operation_result"], "shop.pending_return")
            equal(step["after"], state(env), f"purchase[{index}].after")
            snapshot = env.export_snapshot()
            restored = Env.restore_snapshot(snapshot)
            try:
                equal(snapshot, restored.export_snapshot(), "shop.completed_restore")
                equal(env.export_run_queries(), restored.export_run_queries(), "shop.completed_restore_queries")
            finally:
                restored.__del__()
            equal(None, snapshot["pending_operation"], "shop.completed_pending_operation")
            results.append({"operation": operation, "previews": previews, "answers": answers, **checks})
            native_failures += not step["success"]
        equal(oracle["final"], state(env), "shop.final")
        write_json(directory / "operations.json", results)
        write_json(directory / "final.json", {"snapshot": env.export_snapshot(), "state": state(env)})
        return {"seed": oracle["seed"], "character": oracle["character"], "ascension": oracle["ascension"],
                "case_id": oracle["case_id"], "kind": oracle["kind"], "passed": True,
                "native_prefixes": prefixes, "purchases": len(oracle["steps"]),
                "native_failures": native_failures, "preflight_rejections": preflight_rejections,
                "trace_complete": False}
    finally:
        for handle in reversed(handles):
            handle.__del__()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--seeds", default="RFOP0001,RFOP0002,RFOP0003,RFOP0004")
    parser.add_argument("--operation", choices=("obtain_relic", "transform_cards", "merchant_purchase"), default="obtain_relic")
    parser.add_argument("--relics")
    parser.add_argument("--rng-sources")
    parser.add_argument("--modes")
    parser.add_argument("--card-orders", help="Semicolon-separated ordered deck indices, e.g. '0;2,0,1;9,1,0'")
    parser.add_argument("--purchase-orders", help="Semicolon-separated ordered AllEntries indices; repeated slots test restock")
    parser.add_argument("--gold", type=int, default=None, help="Injected shop fixture gold")
    parser.add_argument("--shop-fixture", type=Path, default=ROOT / "build" / "libenv_api_shop_fixture.so")
    parser.add_argument("--setup-relics", help="Choice-free relic effects executed and compared before the source fixture")
    parser.add_argument("--card-reward-policy", choices=("first", "reroll_once"), default="first")
    parser.add_argument("--character", default="IRONCLAD")
    parser.add_argument("--ascension", type=int, default=0)
    parser.add_argument("--dotnet", default=os.environ.get("DOTNET", str(Path.home() / ".dotnet" / "dotnet")))
    parser.add_argument("--oracle-test-mode", choices=("on", "off"), default="on")
    parser.add_argument("--rng-trace", action="store_true", help="Explicitly request privileged RNG call diagnostics")
    parser.add_argument("--causal-trace", action="store_true", help="Explicitly request privileged model/Hook frames")
    parser.add_argument("--oracle", type=Path, default=ROOT / "tools" / "Sts2Oracle" / "bin" / "Release" / "net9.0" / "Sts2Oracle.dll")
    parser.add_argument("--output-dir", type=Path, default=ROOT / "artifacts" / "env-rf-api-2026-09-30" / "rf-operations")
    args = parser.parse_args()
    global RNG_TRACE, CAUSAL_TRACE
    RNG_TRACE = args.rng_trace
    CAUSAL_TRACE = args.causal_trace
    args.oracle = args.oracle.resolve(strict=True)
    output = args.output_dir.resolve()
    if output.exists():
        parser.error("Output directory already exists; choose a new path to preserve raw evidence")
    output.mkdir(parents=True)
    def unique_list(value):
        result = value.split(",")
        if len(set(result)) != len(result) or not all(result):
            parser.error("Lists must contain nonempty unique entries")
        return result
    seeds = unique_list(args.seeds)
    setup_relics = unique_list(args.setup_relics) if args.setup_relics is not None else []
    if args.operation != "merchant_purchase" and (args.purchase_orders is not None or args.gold is not None):
        parser.error("Merchant arguments require --operation merchant_purchase")
    if args.operation == "merchant_purchase":
        if any(value is not None for value in (args.relics, args.rng_sources, args.modes, args.card_orders)):
            parser.error("Merchant cases accept only --purchase-orders")
        orders = (args.purchase_orders if args.purchase_orders is not None else "0;5;7;10;13;0,0;5,5;7,7;10,10,10,10;13,13").split(";")
        try:
            parsed_orders = [[int(i) for i in order.split(",")] for order in orders]
        except ValueError:
            parser.error("Purchase orders must contain integer indices")
        if len(set(orders)) != len(orders) or any(any(i < 0 or i > 13 for i in order) for order in parsed_orders):
            parser.error("Purchase orders must be unique with indices in 0..13")
        args.gold = args.gold if args.gold is not None else 10000
        if args.gold < 0:
            parser.error("Fixture gold must be nonnegative")
        args.shop_fixture = args.shop_fixture.resolve(strict=True)
        case_ids = [f"purchase:{order}" for order in orders]
        operation_arguments = ["--purchase-orders", ";".join(orders), "--gold", str(args.gold)]
    elif args.operation == "obtain_relic":
        if any(value is not None for value in (args.rng_sources, args.modes, args.card_orders)):
            parser.error("Transform arguments require --operation transform_cards")
        relics = unique_list(args.relics if args.relics is not None else "CALLING_BELL,CAULDRON,ASTROLABE,CLAWS")
        case_ids = relics
        operation_arguments = ["--relics", ",".join(relics)]
    else:
        if args.relics is not None:
            parser.error("--relics requires --operation obtain_relic")
        rngs = unique_list(args.rng_sources if args.rng_sources is not None else "player_transformations,run_niche,event")
        modes = unique_list(args.modes if args.modes is not None else "batch,sequential")
        orders = (args.card_orders if args.card_orders is not None else "0;2,0,1;9,1,0").split(";")
        if set(rngs) - {"player_transformations", "run_niche", "event"} or set(modes) - {"batch", "sequential"}:
            parser.error("Unsupported transform RNG or mode")
        try:
            parsed_orders = [[int(i) for i in order.split(",")] for order in orders]
        except ValueError:
            parser.error("Card orders must contain integer indices")
        if len(set(orders)) != len(orders) or any(len(set(o)) != len(o) or any(i < 0 for i in o) for o in parsed_orders):
            parser.error("Card orders and their indices must be distinct and nonnegative")
        case_ids = [f"transform:{rng}:{mode}:{order}" for rng in rngs for mode in modes for order in orders]
        operation_arguments = ["--rng-sources", ",".join(rngs), "--modes", ",".join(modes), "--card-orders", ";".join(orders)]
    library = Path(os.environ.get("STS2SIM_ENV_LIB", str(ROOT / "build" / "libsts2env_c.so"))).resolve(strict=True)
    host = Path(shutil.which(args.dotnet) or args.dotnet).resolve(strict=True)
    initial = freeze(args, library, host)  # Freeze before dlopen and oracle execution.
    command = [str(host), str(args.oracle), "env-operation", "--seeds", args.seeds,
               "--operation", args.operation, *operation_arguments,
               "--card-reward-policy", args.card_reward_policy,
               "--oracle-test-mode", args.oracle_test_mode,
               "--character", args.character, "--ascension", str(args.ascension)]
    if setup_relics:
        command += ["--setup-relics", ",".join(setup_relics)]
    manifest = {"format": "sts2env.rf_operations.manifest", "version": 1, "command": command,
                "config": {"seeds": seeds, "case_ids": case_ids, "operation": args.operation,
                           "setup_relics": setup_relics, "card_reward_policy": args.card_reward_policy,
                           "fixture_gold": args.gold,
                           "oracle_test_mode": args.oracle_test_mode,
                           "rng_trace_enabled": args.rng_trace,
                           "causal_trace_enabled": args.causal_trace,
                           "character": args.character, "ascension": args.ascension},
                "provenance": initial}
    write_json(output / "manifest.json", manifest)
    sys.path.insert(0, str(ROOT / "python"))
    binding = importlib.import_module("sts2sim_env")
    global Env, Action, LIB, ORACLE_TEST_MODE
    Env, Action, LIB = binding.Env, binding.Action, binding._LIB
    ORACLE_TEST_MODE = args.oracle_test_mode == "on"
    equal(str(library), str(Path(LIB._name).resolve()), "loaded_library")
    equal(initial["runtime"]["env_library"]["sha256"], digest(library), "library_at_dlopen")
    LIB.sts2_env_predict_operation.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t,
                                               ctypes.c_int32, ctypes.c_int32, ctypes.POINTER(ctypes.c_void_p)]
    LIB.sts2_env_predict_operation.restype = ctypes.c_int32
    fixture = None
    if args.operation == "merchant_purchase":
        fixture = ctypes.CDLL(str(args.shop_fixture))
        fixture.sts2_test_shop_fixture.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t,
                                                   ctypes.POINTER(ctypes.c_void_p)]
        fixture.sts2_test_shop_fixture.restype = ctypes.c_int32
    start = time.monotonic()
    with (output / "oracle.jsonl").open("w") as stdout, (output / "oracle.stderr").open("w") as stderr:
        completed = subprocess.run(command, cwd=ROOT, stdout=stdout, stderr=stderr, timeout=120, check=False)
    if completed.returncode:
        raise RuntimeError(f"Oracle exited {completed.returncode}; see {output / 'oracle.stderr'}")
    rows = [json.loads(line) for line in (output / "oracle.jsonl").read_text().splitlines() if line]
    equal([(s, c) for s in seeds for c in case_ids], [(r["seed"], r["case_id"]) for r in rows], "oracle.case_matrix")
    results = []
    for index, oracle in enumerate(rows):
        directory = output / f"case-{index:03d}"
        directory.mkdir(exist_ok=True)
        write_json(directory / "oracle.json", oracle)
        try:
            result = check_merchant_case(oracle, directory, fixture) if fixture else check_case(oracle, directory)
            print(f"PASS {oracle['seed']} {oracle['case_id']}: {result['native_prefixes']} native choice prefixes")
        except Exception as exc:
            result = {"seed": oracle["seed"], "case_id": oracle["case_id"], "kind": oracle["kind"], "passed": False,
                      "error": str(exc), "traceback": traceback.format_exc()}
            print(f"FAIL {oracle['seed']} {oracle['case_id']}: {exc}")
        write_json(directory / "result.json", result)
        results.append(result)
    final = freeze(args, library, host)
    write_json(output / "final-provenance.json", final)
    unchanged = all(initial[key] == final[key] for key in ("git_commit", "source_sha256", "runtime", "environment", "system"))
    summary = {"format": "sts2env.rf_operations.summary", "version": 1, "qualified": unchanged,
               "passed": sum(r["passed"] for r in results), "cases": len(results),
               "native_choice_prefixes": sum(r.get("native_prefixes", 0) for r in results),
               "elapsed_seconds": time.monotonic() - start, "manifest_sha256": digest(output / "manifest.json"),
               "oracle_sha256": digest(output / "oracle.jsonl"), "results": results,
               "oracle_test_mode": args.oracle_test_mode,
               "limitation": "Shipped DLL under recorded headless mode/adapters and injected or natural fixtures; scope only. trace_complete remains explicitly reported, not promoted to complete."}
    write_json(output / "summary.json", summary)
    print(f"{summary['passed']}/{summary['cases']} cases passed; {summary['native_choice_prefixes']} prefixes; runtime/source unchanged={unchanged}")
    print(f"Evidence: {output}")
    return 0 if unchanged and all(r["passed"] for r in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
