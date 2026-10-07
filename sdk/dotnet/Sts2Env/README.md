# Sts2Env .NET SDK

Standalone .NET 9 bindings for the ordinary `sts2_env_*` C ABI. The SDK has no reference to the game assemblies, Godot, or the archived CombatSolver projects. It loads the requested native library directly through `NativeLibrary`, checks `sts2_env_abi_version() == 1` and descriptor `protocol_version == 2`, and only then resolves the operational ABI functions.

## Build and contract test

```bash
dotnet build sdk/dotnet/Sts2Env/Sts2Env.csproj -c Release
dotnet build sdk/dotnet/Sts2Env.ContractTests/Sts2Env.ContractTests.csproj -c Release
dotnet run --project sdk/dotnet/Sts2Env.ContractTests/Sts2Env.ContractTests.csproj -c Release -- /absolute/path/to/libsts2env_c.so
```

The contract test loads the real native library. It checks descriptor versioning, source immutability for prediction and preview, clone and owned-successor lifetimes, discovered-length buffer copying, run/combat/calculated/orb/power fact JSON, snapshot round trips, invalid action/snapshot rejection, trace-capacity failure with no returned successor, typed-operation success and status mapping, a natural nested choice and Answer-prefix continuation, and library lifetime while Env handles remain alive. It follows legal actions from a deterministic set of fresh Neow runs to locate a genuine Choice; it does not inject synthetic cards or state.

## Basic use

```csharp
using Sts2Env;

using var library = Sts2EnvLibrary.Load("/absolute/path/to/libsts2env_c.so");
using var env = library.CreateEnvironment();
env.InspectCombatNamed("DUAL001", "SILENT", 0, "CULTISTS_NORMAL");

ulong sourceHash = env.StateHash;
Sts2Action endTurn = env.LegalActions()
    .Single(action => action.Kind == (int)Sts2ActionKind.EndTurn);
using Sts2EnvPreview prediction = env.Preview(endTurn);
using var trace = prediction.Result;
Console.WriteLine(trace.RootElement.GetProperty("stop_reason").GetString());
Console.WriteLine(env.StateHash == sourceHash); // true: Preview owns a separate successor.

using Sts2EnvPreview typed = env.PredictOperation(Sts2Operations.Action(endTurn));
using var typedResult = typed.Result;
Console.WriteLine(typedResult.RootElement.GetProperty("operation").GetProperty("kind").GetString());
Console.WriteLine(typed.OperationStatus); // complete / needs_choice / paused
Console.WriteLine(typed.OperationResult is null); // ordinary actions have no typed side-result

Sts2EnvSnapshot snapshot = env.CaptureSnapshot();
using Sts2Environment restored = env.RestoreSnapshot(snapshot);
using var combatFacts = restored.ExportCombatFacts();
```

## Ownership and data lifetime

Each `Sts2Environment` owns exactly one native Env handle through `SafeHandle`; `Clone`, `Predict`, `Preview`, and snapshot restore return independent owned instances. Disposing a library defers unloading its native module until its outstanding Env handles are released. Calls on one Env instance are serialized.

Every native output buffer is copied before the next ABI call: legal actions become a managed array, and the action mask and observation use the ABI length-discovery contract before becoming managed arrays. Diagnostic/result strings become managed UTF-8 strings. `JsonDocument` fact/result properties parse those copied strings, so callers own and dispose the returned documents. Preview results are copied from the owned successor before returning.

`Sts2EnvSnapshot` is bound to the `Sts2EnvLibrary` instance that captured it. The typed restore API rejects snapshots from another SDK session, preventing runtime-local pool references from being mixed across sessions. Raw JSON import remains available for untrusted restore input, but the native snapshot validator decides whether it belongs to the loaded runtime. Raw fact/diagnostic JSON may contain pool references; treat them as local to the exact Env state and native runtime, never as cross-session identities.

Use `CapturePortableSnapshot()` and `RestorePortableSnapshot(Sts2EnvPortableSnapshot)` for another
native session or process. This explicit protocol resolves immutable machine, event and stun callback
identities and includes localization keys; it requires the identical native build, math runtime and
floating point controls. Native import validates the full state and stationary-prefix replay before
publishing an owned successor. Re-query `StateHash` on the restored Env before submitting new instance
requests; the portable wire hash is a separate integrity value. This is simulator-state transport,
not live-game capture or cross-build migration. The ordinary session-bound snapshot contract is unchanged.

`PredictOperation` accepts the typed `Sts2Operations` builders for ordinary actions, relic acquisition by model or live reference, card transforms with an explicit native RNG source and batch/sequential mode, reward settlement, card-reward rerolls, and merchant purchases. Requests with instance refs or shop indices require the original facts-query `sourceHash`; submission rejects a changed state instead of rebinding old refs. Canonical relic and ordinary action requests can capture the current hash under the Env lock. Clients never inject RNG state.

An action request can use `with { Stop = Sts2StopCondition.EnemyPhaseDone }` or `SafeOutcome`/`EnemyPhaseOrOutcome`. A suspended successor has decision `Boundary` and only a `Resume` action. Its snapshot includes the original command, answers and checkpoint skip ordinal. Resuming replays that prefix from the stationary root; it may expose another Choice or boundary. `GenerateRewards` is permitted only at a winning SafeOutcome and continues the original action through vanilla combat and room settlement. It does not reroll an already generated reward set. Continuation previews retain the original typed context as `continuation_of`.

## Errors and operation semantics

- Native status 1 becomes `UnportedContentException`; stop exploring that branch.
- Preview status 2 becomes `TraceLimitExceededException`; no successor is returned.
- Typed operation statuses 3, 4, and 5 become `UnsupportedOperationException`, `OperationCapacityExceededException`, and `InvalidOperationRequestException`, respectively.
- Native status -1 and other unknown statuses become `NativeEnvironmentException` with a copied native error message.
- ABI-version and descriptor-protocol mismatch fail library loading before regular Env functions are bound.
- `Step` mutates the owned Env. `TryTransition`/`Predict` and `Preview` create independent successors; failed prediction/import does not return a successor. A preview answering a Choice contains the entire operation-prefix replay, as described by the native trace JSON.

`ExportRunFacts`, `ExportCombatFacts`, `ExportCalculatedVarFacts`, `ExportOrbFacts`, and `ExportPowerFacts` return disposable `JsonDocument` values over copied native JSON. `Inspect*` and `ExportOracle*` methods expose copied raw diagnostic JSON without imposing a schema on consumer-specific fields.

`Sts2EnvPreview.OperationResult` exposes known typed execution facts. Merchant purchases currently return `Sts2MerchantPurchaseResult` with the stable backend entry index, native success flag, and cost policy; unknown future result kinds remain available in `ResultJson`. `OperationStatus` and `TraceComplete` are also available without hand-written JSON parsing. The trace flag remains explicitly partial until the causal event contract is complete.

Privileged RNG call diagnostics require an explicit request and a nonzero trace limit:
`PreviewWithResults(action, includeRng: true)` or `PredictOperation(operation, queryFlags: 2)`.
`PreviewWithResults(action, includeCausal: true)` or operation flag 4 also requests actual
model/Hook call frames. These expose hidden listener identities/order and are explicitly privileged.
Frames freeze model ID, actual override/default status and state-local reference kind at entry,
and record parent plus normal/unwound end order. They do not yet provide complete method values
or object birth/retirement identities. Combine bits 1/2/4 as needed (7 requests all).
Typed-operation flag 1 requests privileged future facts; flag 3 requests future and RNG. Ordinary run-fact queries
still accept only flags 0/1. RNG records include typed bit patterns, before/after state, nested calls
and execution order. Local/copied streams remain unclassified and `TraceComplete` remains false;
these diagnostics are separate from agent observations.
