# C ABI action input validation report

Date: 2026-09-27. Status: integrated; real ABI validation passed.

## Changes

Added [`env/c_action_validation.h`](../../env/c_action_validation.h) with
`sts2::action_from_c_checked(sts2_action)`. It validates the kind and both
coordinates before narrowing, throws `GameError` on invalid input, and copies
the valid kind, index, target, and coordinates into `Action`.

## Verification

- Root integrated the helper at the common C API conversion used by Step,
  Predict, Preview and PreviewWithResults; ABI record layout is unchanged.
- Release build passed. `python3 python/test_env.py` exited 0 (no output);
  it loads the actual shared library and rejects `kind=258/-1`, coordinates
  `256/-129`, and checks null successors and unchanged source hashes.
- Valid actions retain ordinary Predict/Step/Preview successor equivalence.
- The old `kind=255` test expected an action-legality error; it now correctly
  fails earlier at enum validation. That test uses in-range `kind=0` to retain
  the separate action-legality check. The first failing assertion and final
  correction are recorded in the [integration report](2026-09-27-env-run-query-results.md).

## Integration

`env/c_api.cpp` delegates to `sts2::action_from_c_checked(action)` before dispatch.
Query flags and trace capacity validation are covered by the integration tests.
