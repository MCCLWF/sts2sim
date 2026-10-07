import os
import sys
import ctypes
import json

sys.path.insert(0, os.path.dirname(__file__))
from sts2sim_env import Env, TraceLimitExceeded, UnsupportedOperation, VectorEnv, _LIB


def main():
    env = Env("M3PYTEST", "IRONCLAD", 0)
    actions = env.legal_actions()
    assert actions
    mask = env.action_mask()
    assert len(mask) == 512
    assert sum(mask) == len(actions)
    assert len(env.observe()) == Env.observation_size
    copy = env.clone()
    assert copy.state_hash == env.state_hash
    copy.step(actions[0])
    assert copy.state_hash != env.state_hash or copy.decision != env.decision
    assert env.state_hash != 0

    # EncounterId.CULTISTS_NORMAL is ordinal 16 in the generated content
    # table. This is the Python-side smoke check for direct combat entry.
    combat = Env()
    snapshot = combat.inspect_combat_named("M3PYCOMBAT", "CULTISTS_NORMAL")
    assert combat.decision == 1  # DecisionKind.Combat
    assert combat.legal_actions()
    assert snapshot["state_hash"] == combat.state_hash
    assert snapshot["root"]["hand"]
    assert len(snapshot["root"]["enemies"]) == 2
    assert "draw" in snapshot["root"] and "player_powers" in snapshot["root"]
    combat.validate_combat_root()
    assert len(snapshot["root"]["hand_states"]) == len(snapshot["root"]["hand"])
    assert combat.inspect_current()["root"] == snapshot["root"]
    root_hash = combat.state_hash
    assert isinstance(combat.export_oracle_combat_state(), dict)
    assert isinstance(combat.export_oracle_run_state(), dict)
    assert combat.event_rng_counter == -1
    assert combat.state_hash == root_hash

    # Restore a complete run snapshot while the environment is outside combat.
    full_run = Env("M3PYRUNSNAP", "SILENT", 0)
    full_run_state = full_run.inspect_current()
    full_run_snapshot = full_run.export_snapshot()
    restored_run = Env.restore_snapshot(full_run_snapshot)
    assert restored_run.state_hash == full_run.state_hash
    assert restored_run.inspect_current() == full_run_state
    full_run.reset_named("M3PYNAMED", "SILENT", 0)
    assert full_run.legal_actions()
    assert isinstance(full_run.export_oracle_run_state(), dict)
    assert full_run.event_rng_counter >= 0  # Current Neow owns an event RNG.
    named_hash = full_run.state_hash
    rewards, _ = full_run.predict_operation("obtain_relic", {"relic_id": "CAULDRON", "insert_index": -1}, trace_limit=0)
    assert isinstance(rewards.export_oracle_rewards(), list)
    assert rewards.export_oracle_rewards()
    assert full_run.state_hash == named_hash
    for profile_owner in ("run", "player"):
        unsupported = json.loads(json.dumps(full_run.export_snapshot()))
        profile = unsupported["state"]["run"]
        if profile_owner == "player":
            profile = profile["player"]
        profile["profile"]["full_veteran_compatible"] = False
        try:
            Env.restore_snapshot(unsupported)
            raise AssertionError("unsupported profile was imported")
        except UnsupportedOperation as error:
            assert "fully unlocked" in str(error)

    full = combat.export_snapshot()
    restored = Env.restore_snapshot(full)
    assert restored.state_hash == root_hash
    # Exercise the documented C ABI alias case: import from the same handle's
    # last_result buffer, which call() would otherwise clear before reading it.
    assert _LIB.sts2_env_export_snapshot(combat._handle) == 0
    raw_result = ctypes.CFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p)(ctypes.cast(_LIB.sts2_env_last_result, ctypes.c_void_p).value)
    pointer = raw_result(combat._handle)
    payload = ctypes.string_at(pointer)
    imported_handle = ctypes.c_void_p()
    assert _LIB.sts2_env_import_snapshot(combat._handle, pointer, len(payload), ctypes.byref(imported_handle)) == 0
    try:
        assert imported_handle.value and _LIB.sts2_env_hash(imported_handle) == root_hash
    finally:
        _LIB.sts2_env_destroy(imported_handle)
    for malformed in (b'{"format":null,"format":null}', b'[01]', b'{"x":"\\uD800"}', b'[' * 66 + b'0' + b']' * 66):
        imported_handle = ctypes.c_void_p(1)
        assert _LIB.sts2_env_import_snapshot(combat._handle, malformed, len(malformed), ctypes.byref(imported_handle)) == -1
        assert imported_handle.value is None and combat.state_hash == root_hash
    bad = dict(full, schema_sha256="0" * 64)
    try:
        Env.restore_snapshot(bad)
        raise AssertionError("incompatible snapshot accepted")
    except RuntimeError as error:
        assert "incompatible schema_sha256" in str(error)

    # The ordinary query API returns JSON and leaves its Env root untouched.
    query_before = combat.inspect_current()
    query_results = [
        combat.export_combat_queries(),
        combat.export_calculated_vars(),
        combat.export_orbs(),
        combat.export_powers(),
    ]
    assert all(isinstance(item, dict) for item in query_results)
    assert combat.state_hash == root_hash

    # Validate original int32 inputs before narrowing native enum widths.
    # Negative encounter formerly indexed outside the static content table.
    for invalid_character in (-1, 65537, 65540, 2147483647):
        assert _LIB.sts2_env_reset(combat._handle, b"BADENUM", invalid_character, 0) == 5
        assert combat.state_hash == root_hash
    for invalid_encounter in (-1, 0, 999, 65536, 2147483647):
        assert _LIB.sts2_env_reset_combat(combat._handle, b"BADENUM", 4, 0, invalid_encounter, 0) == 5
        assert combat.state_hash == root_hash
    assert combat.inspect_current()["root"] == query_before["root"]

    # These assertions exercise only the public Env query/result contract;
    # their values are not treated as independent game/oracle expectations.
    run_queries = combat.export_run_queries()
    assert run_queries["format"] == "sts2env.run_queries" and run_queries["version"] == 1
    assert run_queries["state_hash"] == root_hash
    assert run_queries["decision"] == combat.decision
    assert {"deck", "relics", "potions", "gold", "hp", "max_hp"} <= set(run_queries["player"])
    assert "choice" in run_queries and run_queries["choice"] is None
    assert run_queries["future"] is None
    future_queries = combat.export_run_queries(include_future=True)
    assert future_queries["format"] == "sts2env.run_queries" and future_queries["version"] == 1
    assert future_queries["future"] is not None
    assert combat.state_hash == root_hash

    # Unknown query flag values are rejected by the C ABI without changing the root.
    assert _LIB.sts2_env_export_run_queries(combat._handle, 2) == 5
    assert combat.state_hash == root_hash

    action = combat.legal_actions()[0]
    for invalid_action, invalid_limit in ((type(action)(kind=1), 16384), (action, -1), (action, 1000001)):
        invalid_output = ctypes.c_void_p(1)
        assert _LIB.sts2_env_preview(combat._handle, invalid_action._c(), invalid_limit,
                                     ctypes.byref(invalid_output)) == 5
        assert invalid_output.value is None
        invalid_output = ctypes.c_void_p(1)
        assert _LIB.sts2_env_preview_with_results(combat._handle, invalid_action._c(), invalid_limit,
                                                  0, ctypes.byref(invalid_output)) == 5
        assert invalid_output.value is None
        assert combat.state_hash == root_hash
        if invalid_limit == 16384:
            invalid_output = ctypes.c_void_p(1)
            assert _LIB.sts2_env_try_transition(combat._handle, invalid_action._c(), ctypes.byref(invalid_output)) == 5
            assert invalid_output.value is None
    predicted = combat.predict(action)
    explicit = combat.clone()
    explicit.step(action)
    assert predicted.state_hash == explicit.state_hash
    assert predicted.inspect_current()["state_hash"] == predicted.state_hash
    assert predicted.inspect_current()["decision"] == predicted.decision
    assert combat.state_hash == root_hash

    previewed, preview_result = combat.preview(action)
    untraced, untraced_result = combat.preview(action, trace_limit=0)
    assert isinstance(preview_result, dict) and isinstance(untraced_result, dict)
    assert preview_result["format"] == "sts2env.preview" and preview_result["version"] == 1
    assert untraced_result["version"] == 1
    assert "run_results" not in preview_result and "run_results" not in untraced_result
    assert previewed.state_hash == predicted.state_hash
    assert untraced.state_hash == predicted.state_hash
    assert combat.state_hash == root_hash
    assert combat.inspect_current()["root"] == snapshot["root"]

    with_results, results = combat.preview_with_results(action)
    with_results_untraced, untraced_results = combat.preview_with_results(action, trace_limit=0)
    assert results["format"] == "sts2env.preview" and results["version"] == 2
    assert results["source_hash"] == root_hash
    assert results["successor_hash"] == predicted.state_hash
    for envelope in (results, untraced_results):
        run_results = envelope["run_results"]
        for side, expected_hash in (("before", root_hash), ("after", predicted.state_hash)):
            query = run_results[side]
            assert query["format"] == "sts2env.run_queries" and query["version"] == 1
            assert query["state_hash"] == expected_hash
            assert query["future"] is None
    assert with_results.state_hash == predicted.state_hash
    assert with_results_untraced.state_hash == predicted.state_hash
    assert combat.state_hash == root_hash

    bad_preview_successor = ctypes.c_void_p()
    assert _LIB.sts2_env_preview_with_results(
        combat._handle, action._c(), 16384, 8, ctypes.byref(bad_preview_successor)) == 5
    assert bad_preview_successor.value is None
    assert combat.state_hash == root_hash

    future_preview, future_results = combat.preview_with_results(action, include_future=True)
    assert future_results["version"] == 2
    assert future_results["run_results"]["before"]["state_hash"] == root_hash
    assert future_results["run_results"]["after"]["state_hash"] == predicted.state_hash
    assert future_results["run_results"]["before"]["future"] is not None
    assert future_results["run_results"]["after"]["future"] is not None
    assert future_preview.state_hash == predicted.state_hash
    assert combat.state_hash == root_hash

    # Explicit privileged RNG observations share ordinary execution. Compare
    # complete state, including hidden RNG, instead of trusting result hashes.
    end_turn = next(a for a in combat.legal_actions() if a.kind == 3)
    rng_env = combat.clone()
    rng_env.step(end_turn)  # Exhaust the draw pile so the tested turn shuffles.
    rng_hash = rng_env.state_hash
    normal, normal_result = rng_env.preview_with_results(end_turn)
    rng_branch, rng_result = rng_env.preview_with_results(end_turn, include_rng=True)
    both_branch, both_result = rng_env.preview_with_results(end_turn, include_rng=True, include_future=True)
    assert rng_branch.export_snapshot() == normal.export_snapshot() == both_branch.export_snapshot()
    assert not normal_result["rng_trace_enabled"] and normal_result["rng_calls"] == []
    assert rng_result["rng_trace_enabled"] and rng_result["rng_calls"]
    assert rng_result["rng_trace_visibility"] == "privileged_diagnostic" and not rng_result["trace_complete"]
    assert both_result["rng_calls"] == rng_result["rng_calls"]
    assert rng_result["run_results"]["before"]["future"] is None
    assert both_result["run_results"]["before"]["future"] is not None
    assert not normal_result["causal_trace_enabled"]
    assert not any(f["kind"] in ("model_method", "hook") for f in normal_result["frames"])
    assert not any(f["kind"] in ("model_method", "hook") for f in rng_result["frames"])
    causal_branch, causal = rng_env.preview_with_results(end_turn, include_rng=True, include_causal=True)
    assert causal_branch.export_snapshot() == normal.export_snapshot()
    assert causal["causal_trace_enabled"] and causal["causal_trace_visibility"] == "privileged_diagnostic"
    assert not causal["trace_complete"] and causal["run_results"]["before"]["future"] is None
    assert any(f["kind"] == "model_method" and f["source_model_id"] for f in causal["frames"])
    assert any(f["kind"] == "hook" for f in causal["frames"])
    for i, frame in enumerate(causal["frames"]):
        assert frame["finished"] and frame["sequence"] < frame["finished_sequence"]
        assert -1 <= frame["parent"] < i
        if frame["parent"] >= 0:
            parent = causal["frames"][frame["parent"]]
            assert parent["sequence"] < frame["sequence"] < frame["finished_sequence"] < parent["finished_sequence"]
    params = dict(kind=end_turn.kind, index=end_turn.index, target=end_turn.target, col=end_turn.col, row=end_turn.row)
    typed_branch, typed = rng_env.predict_operation("action", params, include_rng=True)
    assert typed_branch.export_snapshot() == normal.export_snapshot() and typed["rng_trace_enabled"]
    causal_typed_branch, causal_typed = rng_env.predict_operation("action", params, include_rng=True, include_causal=True)
    assert causal_typed_branch.export_snapshot() == normal.export_snapshot()
    assert causal_typed["frames"] == causal["frames"] and causal_typed["rng_calls"] == causal["rng_calls"]
    for flags in range(8):
        flag_branch, flag_result = rng_env.preview_with_results(end_turn, include_future=bool(flags & 1),
            include_rng=bool(flags & 2), include_causal=bool(flags & 4))
        try:
            assert flag_branch.export_snapshot() == normal.export_snapshot()
            assert flag_result["rng_trace_enabled"] == bool(flags & 2)
            assert flag_result["causal_trace_enabled"] == bool(flags & 4)
            assert (flag_result["run_results"]["before"]["future"] is not None) == bool(flags & 1)
        finally:
            flag_branch.__del__()
    request = json.dumps(dict(format="sts2env.operation", version=1, kind="action", source_hash=rng_hash,
                              params=params, stop="operation_done")).encode()
    for flags, limit, status in ((2, 0, 5), (3, 0, 5), (4, 0, 5), (6, 0, 5), (7, 0, 5),
                                 (8, 16384, 5), (-1, 16384, 5), (2, 1, 2), (4, 1, 2), (6, 1, 2)):
        for operation in (False, True):
            successor = ctypes.c_void_p(1)
            result = (_LIB.sts2_env_predict_operation(rng_env._handle, request, len(request), limit, flags,
                      ctypes.byref(successor)) if operation else
                      _LIB.sts2_env_preview_with_results(rng_env._handle, end_turn._c(), limit, flags, ctypes.byref(successor)))
            assert result == status and successor.value is None and rng_env.state_hash == rng_hash
    for method in (lambda: rng_env.preview_with_results(end_turn, trace_limit=0, include_rng=True),
                   lambda: rng_env.predict_operation("action", params, trace_limit=0, include_rng=True),
                   lambda: rng_env.preview_with_results(end_turn, trace_limit=0, include_causal=True),
                   lambda: rng_env.predict_operation("action", params, trace_limit=0, include_causal=True)):
        try:
            method()
            raise AssertionError("SDK accepted RNG without an enabled trace")
        except ValueError:
            pass
    assert combat.state_hash == root_hash and combat.inspect_current()["root"] == snapshot["root"]

    # The C ABI must reject values before enum/int8 narrowing can alias a legal
    # action. This verifies interface validation and source isolation only.
    for invalid in (type(action)(kind=258), type(action)(kind=-1),
                    type(action)(kind=action.kind, index=action.index, target=action.target, col=256),
                    type(action)(kind=action.kind, index=action.index, target=action.target, col=-129),
                    type(action)(kind=action.kind, index=action.index, target=action.target, row=256),
                    type(action)(kind=action.kind, index=action.index, target=action.target, row=-129)):
        invalid_successor = ctypes.c_void_p()
        status = _LIB.sts2_env_try_transition(combat._handle, invalid._c(), ctypes.byref(invalid_successor))
        assert status == 5
        assert invalid_successor.value is None
        assert combat.state_hash == root_hash

    # A tiny positive trace buffer must report capacity exhaustion for an
    # action that emits more than one trace record, without changing the root.
    capacity_exceeded = False
    for candidate in combat.legal_actions():
        try:
            candidate_successor, _ = combat.preview(candidate, trace_limit=1)
        except TraceLimitExceeded:
            capacity_exceeded = True
            break
        else:
            candidate_successor.__del__()
    assert capacity_exceeded
    assert combat.state_hash == root_hash
    assert combat.inspect_current()["root"] == snapshot["root"]

    for invalid_limit in (-1, 1_000_001):
        try:
            combat.preview(action, trace_limit=invalid_limit)
            raise AssertionError("invalid trace limit was accepted")
        except ValueError:
            pass

    try:
        # A well-formed enum value that is illegal at this combat decision
        # reaches the ordinary legality check after ABI range validation.
        combat.predict(type(action)(kind=0))
    except RuntimeError as error:
        assert "not legal" in str(error)
    else:
        raise AssertionError("invalid prediction action was accepted")
    assert combat.state_hash == root_hash

    batch = VectorEnv(2)
    batch.reset(["M3VEC_A", "M3VEC_B"])
    assert len(batch.observations()) == 2
    assert all(len(item) == Env.observation_size for item in batch.observations())
    assert batch.hashes()[0] != batch.hashes()[1]


if __name__ == "__main__":
    main()
