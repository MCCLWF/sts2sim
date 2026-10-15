# C ABI action input validation

Date: 2026-09-27. Status: helper integrated; real ABI validation passed.

## Goal

Provide a checked conversion from the fixed-width `sts2_action` C ABI record to
the public C++ `Action`. Reject invalid enum values and coordinates before any
narrowing conversion can make them alias valid inputs.

## Scope

- Check `kind` against the inclusive `Action::StartRun..Action::Answer` range.
- Check `col` and `row` against the `int8_t` range before converting them.
- Preserve `index`, `target`, and all valid kind/coordinate values exactly.
- Keep the helper independent so the C API call sites can integrate it without
  changing the C ABI layout.

## Integration and acceptance

The C API call sites should include `env/c_action_validation.h` and use
`sts2::action_from_c_checked` before dispatching to `Step`, `Predict`, or
`PreviewAction`. The integrating change must verify through the real C ABI that
an invalid `kind` such as 258 and out-of-range coordinates are rejected, valid
actions retain their prior behavior, and a rejected request leaves the source
Env unchanged. This helper-only change does not claim the ABI issue is fixed.
