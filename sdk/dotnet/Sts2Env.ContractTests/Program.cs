using System.Text.Json;
using Sts2Env;

static void Require(bool condition, string message)
{
    if (!condition) throw new InvalidOperationException(message);
}

static void TestDescriptor(Sts2EnvLibrary library)
{
    Require(library.AbiVersion == 1, "ABI version was not checked as version 1");
    Require(library.ProtocolVersion == 2, "descriptor protocol version was not checked as version 2");
    Require(library.ApiDescriptor.ValueKind == JsonValueKind.Object, "descriptor was not copied as a JSON object");
    Require(library.ApiDescriptor.TryGetProperty("protocol_version", out JsonElement protocol) && protocol.GetUInt32() == 2,
            "copied descriptor omitted protocol_version 2");
    JsonElement symbols = library.ApiDescriptor.GetProperty("exports").GetProperty("symbols");
    Require(symbols.EnumerateArray().Any(symbol => symbol.GetString() == "sts2_env_predict_operation"),
            "descriptor did not advertise the typed-operation prediction ABI");
    Require((int)Sts2DecisionKind.Boundary == 9 && (int)Sts2ActionKind.Resume == 13,
            "SDK omitted the reserved future boundary/resume codes");
}

static Sts2Action FindAction(Sts2Environment env, Sts2ActionKind kind) =>
    env.LegalActions().FirstOrDefault(action => action.Kind == (int)kind);

static void TestLargeSnapshot(Sts2EnvLibrary library, string path)
{
    string json = File.ReadAllText(path);
    Require(System.Text.Encoding.UTF8.GetByteCount(json) > 4 * 1024 * 1024, "large fixture does not cross the old byte limit");
    using var diagnostic = library.CreateEnvironment();
    using var source = diagnostic.RestorePortableSnapshot(new Sts2EnvPortableSnapshot(json));
    ulong hash = source.StateHash;
    var actions = source.LegalActions();
    Require(actions.Length == 6500 && source.ActionMask().Length == 6500 && source.Observe().Length > 2048,
            "SDK truncated a large decision buffer");
    using var restored = source.RestoreSnapshot(source.CaptureSnapshot());
    Require(restored.StateHash == hash && restored.LegalActions().SequenceEqual(actions), "large local restore differs");
    using var predicted = source.Predict(actions[0]);
    using var replayed = restored.Predict(actions[0]);
    Require(predicted.StateHash == replayed.StateHash && source.StateHash == hash, "large prefix prediction shared or changed state");
    using var second = predicted.Predict(predicted.LegalActions()[0]);
    using var completed = second.Predict(second.LegalActions()[0]);
    Require(completed.Decision.KnownKind == Sts2DecisionKind.Map && source.StateHash == hash, "large choice did not complete independently");
    Console.WriteLine("Sts2Env .NET large capacity snapshot and 6500-action prefix passed.");
}

static void TestOrdinaryOwnedApi(Sts2EnvLibrary library)
{
    using Sts2Environment env = library.CreateEnvironment();
    string structuralRoot = env.InspectCombatNamed("SDK5266ROOT", "SILENT", 0, "CULTISTS_NORMAL");
    Require(structuralRoot.Contains("state_hash", StringComparison.Ordinal), "named combat inspection returned no state hash");
    ulong sourceHash = env.StateHash;
    Sts2Action endTurn = FindAction(env, Sts2ActionKind.EndTurn);
    Require(endTurn.Kind == (int)Sts2ActionKind.EndTurn, "combat fixture has no EndTurn action");

    using Sts2Environment clone = env.Clone();
    Require(clone.StateHash == sourceHash, "Clone changed the state hash");
    using Sts2Environment predicted = env.Predict(endTurn);
    Require(env.StateHash == sourceHash, "Predict modified the source environment");
    Require(predicted.StateHash != 0, "Predict returned an invalid owned successor");

    Sts2EnvSnapshot snapshot = env.CaptureSnapshot();
    using Sts2Environment restored = env.RestoreSnapshot(snapshot);
    Require(restored.StateHash == sourceHash, "snapshot restore did not round-trip the logical state");
    Require(restored.LegalActions().SequenceEqual(env.LegalActions()), "snapshot restore changed legal actions");

    string combatFacts = env.ExportCombatQueries();
    using JsonDocument combatDocument = JsonDocument.Parse(combatFacts);
    Require(combatDocument.RootElement.ValueKind == JsonValueKind.Object, "combat facts were not copied JSON");
    using JsonDocument runDocument = env.ExportRunFacts();
    Require(runDocument.RootElement.ValueKind == JsonValueKind.Object, "run facts were not copied JSON");
    using JsonDocument varsDocument = env.ExportCalculatedVarFacts();
    Require(varsDocument.RootElement.ValueKind == JsonValueKind.Object, "calculated-variable facts were not copied JSON");
    using JsonDocument orbsDocument = env.ExportOrbFacts();
    Require(orbsDocument.RootElement.ValueKind == JsonValueKind.Object, "orb facts were not copied JSON");
    using JsonDocument powersDocument = env.ExportPowerFacts();
    Require(powersDocument.RootElement.ValueKind == JsonValueKind.Object, "power facts were not copied JSON");

    byte[] mask = env.ActionMask();
    Require(mask.Length == Sts2EnvLibrary.ActionMaskSize, "action mask did not copy all fixed slots");
    int[] observation = env.Observe();
    Require(observation.Length == Sts2EnvLibrary.ObservationSize, "observation did not copy the fixed buffer");
    int firstObservedValue = observation[0];
    observation[0] = unchecked(firstObservedValue + 1);
    Require(env.Observe()[0] == firstObservedValue, "Observe returned a native-backed buffer instead of a copy");

    using Sts2EnvPreview preview = env.Preview(endTurn);
    Require(env.StateHash == sourceHash, "Preview modified the source environment");
    using JsonDocument previewDocument = preview.Result;
    Require(previewDocument.RootElement.TryGetProperty("source_hash", out _), "preview result omitted source_hash");

    // A child keeps the module loaded and remains usable after its source handle
    // is disposed; every branch is independently owned.
    Sts2Environment detachedClone = env.Clone();
    ulong detachedHash = detachedClone.StateHash;
    env.Dispose();
    Require(detachedClone.StateHash == detachedHash, "disposing a source invalidated its independently-owned clone");
    detachedClone.Dispose();
}

static void TestLibraryLifetime(Sts2EnvLibrary owner)
{
    Sts2EnvLibrary library = Sts2EnvLibrary.Load(owner.NativeLibraryPath);
    Sts2Environment env = library.CreateEnvironment();
    library.Dispose();
    env.ResetNamed("SDK5266LIFETIME", "SILENT");
    Require(env.LegalActions().Length > 0, "disposing the library owner unloaded a module still used by an Env handle");
    env.Dispose();
}

static void TestPrivilegedRng(Sts2EnvLibrary library)
{
    using var env = library.CreateEnvironment();
    env.InspectCombatNamed("SDKRNGTRACE", "IRONCLAD", 0, "CULTISTS_NORMAL");
    var action = FindAction(env, Sts2ActionKind.EndTurn);
    env.Step(action); // Exhaust the initial draw pile; the next turn shuffles.
    ulong sourceHash = env.StateHash;
    using var normal = env.PreviewWithResults(action);
    using var rng = env.PreviewWithResults(action, includeRng: true);
    using var both = env.PreviewWithResults(action, includeFuture: true, includeRng: true);
    using var typed = env.PredictOperation(Sts2Operations.Action(action), queryFlags: 2);
    using var causal = env.PreviewWithResults(action, includeRng:true, includeCausal:true);
    using var causalTyped = env.PredictOperation(Sts2Operations.Action(action), queryFlags:6);
    using var normalResult = normal.Result;
    using var rngResult = rng.Result;
    using var bothResult = both.Result;
    using var typedResult = typed.Result;
    using var causalResult = causal.Result;
    using var causalTypedResult = causalTyped.Result;
    Require(!normalResult.RootElement.GetProperty("rng_trace_enabled").GetBoolean()
        && normalResult.RootElement.GetProperty("rng_calls").GetArrayLength()==0, "default preview leaked RNG calls");
    Require(rngResult.RootElement.GetProperty("rng_trace_enabled").GetBoolean()
        && rngResult.RootElement.GetProperty("rng_calls").GetArrayLength()>0, "explicit preview omitted RNG calls");
    Require(!rng.TraceComplete && rngResult.RootElement.GetProperty("rng_trace_visibility").GetString()=="privileged_diagnostic",
        "RNG diagnostics promoted complete/public trace");
    Require(rngResult.RootElement.GetProperty("rng_calls").GetRawText()==bothResult.RootElement.GetProperty("rng_calls").GetRawText(),
        "future queries changed RNG call trace");
    Require(typedResult.RootElement.GetProperty("rng_trace_enabled").GetBoolean(), "typed SDK omitted explicit RNG flags");
    Require(normal.Successor.StateHash==rng.Successor.StateHash && rng.Successor.StateHash==both.Successor.StateHash
        && both.Successor.StateHash==typed.Successor.StateHash && env.StateHash==sourceHash, "RNG SDK changed gameplay state");
    bool rejected=false;
    try {using var invalid=env.PreviewWithResults(action,traceLimit:0,includeRng:true);}
    catch(ArgumentException) {rejected=true;}
    Require(rejected,"SDK accepted RNG without trace");
    Require(!normalResult.RootElement.GetProperty("causal_trace_enabled").GetBoolean()
        && !rngResult.RootElement.GetProperty("frames").EnumerateArray().Any(f=>f.GetProperty("kind").GetString() is "hook" or "model_method"),
        "default/RNG preview leaked hidden causal listener order");
    Require(causalResult.RootElement.GetProperty("causal_trace_enabled").GetBoolean()
        && causalResult.RootElement.GetProperty("frames").EnumerateArray().Any(f=>f.GetProperty("kind").GetString()=="model_method"),
        "SDK omitted requested causal calls");
    Require(causalResult.RootElement.GetProperty("frames").GetRawText()==causalTypedResult.RootElement.GetProperty("frames").GetRawText(),
        "typed/action causal traces differ");
    Require(normal.Successor.CaptureSnapshot().Json==causal.Successor.CaptureSnapshot().Json
        && causal.Successor.CaptureSnapshot().Json==causalTyped.Successor.CaptureSnapshot().Json && env.StateHash==sourceHash,
        "causal diagnostics changed full gameplay state");
    foreach(int flags in new[]{4,6,7}) {
        rejected=false;
        try {using var invalid=env.PredictOperation(Sts2Operations.Action(action),traceLimit:0,queryFlags:flags);}
        catch(ArgumentException) {rejected=true;}
        Require(rejected,"SDK accepted causal diagnostics without trace");
    }
    for(int flags=0;flags<8;flags++) {
        using var branch=env.PredictOperation(Sts2Operations.Action(action),queryFlags:flags);
        using var result=branch.Result;
        Require(branch.Successor.CaptureSnapshot().Json==normal.Successor.CaptureSnapshot().Json
            && result.RootElement.GetProperty("rng_trace_enabled").GetBoolean()==((flags&2)!=0)
            && result.RootElement.GetProperty("causal_trace_enabled").GetBoolean()==((flags&4)!=0)
            && (result.RootElement.GetProperty("run_results").GetProperty("before").GetProperty("future").ValueKind!=JsonValueKind.Null)==((flags&1)!=0),
            "combined SDK flags changed state or diagnostic visibility");
    }
}

static void TestSnapshotSessionBoundary(Sts2EnvLibrary library)
{
    using Sts2Environment source = library.CreateEnvironment();
    source.InspectCombatNamed("SDK5266SESSION", "SILENT", 0, "CULTISTS_NORMAL");
    Sts2EnvSnapshot snapshot = source.CaptureSnapshot();
    using Sts2EnvLibrary otherSession = Sts2EnvLibrary.Load(library.NativeLibraryPath);
    bool rejected = false;
    try { using Sts2Environment _ = otherSession.RestoreSnapshot(snapshot); }
    catch (SnapshotSessionMismatchException) { rejected = true; }
    Require(rejected, "snapshot references were accepted by a different SDK library session");
    Sts2EnvPortableSnapshot portable = source.CapturePortableSnapshot();
    using Sts2Environment diagnostic = otherSession.CreateEnvironment();
    using Sts2Environment restored = diagnostic.RestorePortableSnapshot(portable);
    Require(restored.CapturePortableSnapshot().Json == portable.Json, "portable SDK snapshot did not round-trip across sessions");
    Sts2Action endTurn = FindAction(source, Sts2ActionKind.EndTurn);
    using Sts2Environment expected = source.Predict(endTurn);
    using Sts2Environment actual = restored.Predict(endTurn);
    Require(expected.CapturePortableSnapshot().Json == actual.CapturePortableSnapshot().Json, "portable SDK continuation differs");
}

static void TestInvalidAndCapacityFailures(Sts2EnvLibrary library)
{
    using Sts2Environment env = library.CreateEnvironment();
    env.InspectCombatNamed("SDK5266FAIL", "SILENT", 0, "CULTISTS_NORMAL");
    ulong sourceHash = env.StateHash;

    bool invalidRejected = false;
    try { using Sts2Environment _ = env.Predict(new Sts2Action(int.MaxValue)); }
    catch (InvalidOperationRequestException) { invalidRejected = true; }
    Require(invalidRejected, "invalid action kind was accepted by the native ABI");
    Require(env.StateHash == sourceHash, "invalid action changed the source state");

    bool invalidSnapshotRejected = false;
    try { using Sts2Environment _ = env.ImportSnapshotJson("{ invalid snapshot"); }
    catch (NativeEnvironmentException) { invalidSnapshotRejected = true; }
    Require(invalidSnapshotRejected, "invalid snapshot JSON was accepted");
    Require(env.StateHash == sourceHash, "invalid snapshot import changed its diagnostic/source handle");

    bool traceCapacityRejected = false;
    foreach (Sts2Action action in env.LegalActions())
    {
        try { using Sts2EnvPreview _ = env.Preview(action, traceLimit: 1); }
        catch (TraceLimitExceededException)
        {
            traceCapacityRejected = true;
            break;
        }
    }
    Require(traceCapacityRejected, "no combat action exercised the native trace-capacity failure path");
    Require(env.StateHash == sourceHash, "trace-capacity failure changed the source state");
    Require(env.LegalActions().Length > 0, "failed preview left the source unusable");
}

static void TestTypedOperationPrediction(Sts2EnvLibrary library)
{
    using Sts2Environment env = library.CreateEnvironment();
    env.InspectCombatNamed("SDK5266OPERATION", "SILENT", 0, "CULTISTS_NORMAL");
    ulong sourceHash = env.StateHash;
    Sts2Action endTurn = FindAction(env, Sts2ActionKind.EndTurn);

    using Sts2EnvPreview operationPreview = env.PredictOperation(Sts2Operations.Action(endTurn));
    Require(env.StateHash == sourceHash, "typed operation prediction modified its source Env");
    using JsonDocument operationResult = operationPreview.Result;
    Require(operationPreview.OperationResult is null, "ordinary action unexpectedly exposed an operation result");
    Require(operationPreview.OperationStatus == "complete", "typed preview did not expose operation status");
    Require(!operationPreview.TraceComplete, "SDK promoted the intentionally partial trace to complete");
    JsonElement operation = operationResult.RootElement.GetProperty("operation");
    Require(operation.GetProperty("kind").GetString() == "action", "typed operation result omitted its action kind");
    Require(operation.GetProperty("source_hash").GetUInt64() == sourceHash,
            "SDK did not bind the typed request to the source Env hash");
    Require(operation.GetProperty("params").GetProperty("kind").GetInt32() == endTurn.Kind,
            "typed action parameters were not serialized to UTF-8 JSON correctly");

    bool invalidRequestRejected = false;
    try { using Sts2EnvPreview _ = env.PredictOperation(Sts2Operations.Action(new Sts2Action(int.MaxValue))); }
    catch (InvalidOperationRequestException) { invalidRequestRejected = true; }
    Require(invalidRequestRejected, "native invalid-operation status 5 was not mapped to its typed exception");

    bool unsupportedRejected = false;
    try { using Sts2EnvPreview _ = env.PredictOperation(Sts2Operations.GenerateRewards(Sts2RewardRoom.Monster)); }
    catch (UnsupportedOperationException) { unsupportedRejected = true; }
    Require(unsupportedRejected, "native unsupported-operation status 3 was not mapped to its typed exception");
    Require(env.StateHash == sourceHash, "rejected typed operation modified its source Env");

    ushort[] mutableReferences = [7, 9];
    Sts2TransformCardsOperation transform = Sts2Operations.TransformCards(
        mutableReferences, Sts2TransformRngSource.PlayerTransformations, sourceHash, Sts2TransformMode.Sequential);
    mutableReferences[0] = 100;
    Require(transform.Cards[0] == 7, "transform operation builder retained a caller-mutable reference array");
    using Sts2Environment changed = env.Predict(endTurn);
    bool staleRejected = false;
    try { using Sts2EnvPreview _ = changed.PredictOperation(transform); }
    catch (InvalidOperationRequestException) { staleRejected = true; }
    Require(staleRejected, "SDK refreshed an old instance reference to a different state's hash");
}

static Sts2Environment? FindNaturalChoice(Sts2EnvLibrary library)
{
    // The first-run Neow/event path can produce a genuine nested card-choice
    // request. Try a fixed seed set and follow legal native actions only.
    for (int seedIndex = 0; seedIndex < 96; seedIndex++)
    {
        Sts2Environment current = library.CreateEnvironment();
        try
        {
            current.ResetNamed($"SDK5266CHOICE{seedIndex:D2}", "SILENT");
            for (int depth = 0; depth < 320; depth++)
            {
                if (current.Decision.IsChoice) return current;
                Sts2Action[] actions = current.LegalActions();
                if (actions.Length == 0) break;

                int decision = current.Decision.RawKind;
                IEnumerable<Sts2Action> candidates = decision switch
                {
                    (int)Sts2DecisionKind.Event => actions,
                    (int)Sts2DecisionKind.Map => actions.Take(2),
                    (int)Sts2DecisionKind.Combat => actions
                        .Where(action => action.Kind == (int)Sts2ActionKind.PlayCard)
                        .Take(1)
                        .DefaultIfEmpty(actions.FirstOrDefault(action => action.Kind == (int)Sts2ActionKind.EndTurn)),
                    _ => actions.Take(1),
                };

                Sts2Environment? next = null;
                foreach (Sts2Action action in candidates)
                {
                    if (action.Kind == 0 && action == default) continue;
                    Sts2Environment branch;
                    try { branch = current.TryTransition(action); }
                    catch (Sts2EnvException) { continue; }
                    if (branch.Decision.IsChoice)
                    {
                        current.Dispose();
                        return branch;
                    }
                    if (next is null) next = branch;
                    else branch.Dispose();
                }
                if (next is null) break;
                current.Dispose();
                current = next;
            }
        }
        catch (Sts2EnvException)
        {
            // A seed whose ordinary first route reaches unsupported content is
            // discarded; other deterministic seeds still exercise the contract.
        }
        current.Dispose();
    }
    return null;
}

static void TestBoundaryContinuation(Sts2EnvLibrary library)
{
    using Sts2Environment env = library.CreateEnvironment();
    env.InspectCombatNamed("SDKBOUNDARY", "IRONCLAD", 0, "CULTISTS_NORMAL");
    ulong sourceHash = env.StateHash;
    Sts2Action endTurn = FindAction(env, Sts2ActionKind.EndTurn);
    using Sts2EnvPreview paused = env.PredictOperation(Sts2Operations.Action(endTurn)
        with { Stop = Sts2StopCondition.EnemyPhaseDone });
    Require(paused.Successor.Decision.RawKind == (int)Sts2DecisionKind.Boundary,
            "SDK boundary stop did not expose the suspended decision");
    Sts2Action[] legal = paused.Successor.LegalActions();
    Require(legal.Length == 1 && legal[0].Kind == (int)Sts2ActionKind.Resume,
            "boundary offered gameplay actions before the native operation completed");
    Sts2EnvSnapshot snapshot = paused.Successor.CaptureSnapshot();
    using Sts2Environment restored = paused.Successor.RestoreSnapshot(snapshot);
    using Sts2EnvPreview resumed = restored.Preview(legal[0]);
    using JsonDocument result = resumed.Result;
    Require(result.RootElement.GetProperty("continuation_of").GetProperty("kind").GetString() == "action",
            "SDK continuation lost original typed operation context");
    using Sts2Environment ordinary = env.Predict(endTurn);
    Require(resumed.Successor.StateHash == ordinary.StateHash, "boundary replay differed from ordinary end turn");
    Require(env.StateHash == sourceHash, "SDK boundary prediction modified source");
}

static void TestChoiceContinuation(Sts2EnvLibrary library)
{
    using Sts2Environment choice = FindNaturalChoice(library)
        ?? throw new InvalidOperationException("No natural first-run path reached a nested Choice within the fixed seed/depth budget");
    Require(choice.Decision.IsChoice, "natural choice fixture did not stop at Choice");
    Sts2Action[] answers = choice.LegalActions();
    Require(answers.Length > 0 && answers.All(action => action.Kind == (int)Sts2ActionKind.Answer),
            "Choice legal actions were not exposed as Answer actions");

    Sts2EnvSnapshot choiceSnapshot = choice.CaptureSnapshot();
    using Sts2Environment restoredChoice = choice.RestoreSnapshot(choiceSnapshot);
    Require(restoredChoice.Decision.IsChoice && restoredChoice.StateHash == choice.StateHash,
            "choice snapshot did not preserve the pending request and replay root");

    ulong before = restoredChoice.StateHash;
    using Sts2Environment answerSuccessor = restoredChoice.TryTransition(answers[0]);
    Require(restoredChoice.StateHash == before, "predicting an Answer modified the pending-choice source");

    using Sts2EnvPreview replayed = restoredChoice.Preview(answers[0]);
    using JsonDocument trace = replayed.Result;
    Require(trace.RootElement.TryGetProperty("choice_replayed", out JsonElement replayedFlag) && replayedFlag.GetBoolean(),
            "Answer preview omitted its choice-prefix replay marker");
    using var rngReplay = restoredChoice.PreviewWithResults(answers[0], includeRng:true);
    using var rngTrace = rngReplay.Result;
    Require(rngTrace.RootElement.GetProperty("choice_replayed").GetBoolean()
        && rngTrace.RootElement.GetProperty("trace_semantics").GetString()=="operation_prefix"
        && rngReplay.Successor.StateHash==answerSuccessor.StateHash,
        "RNG Answer observation changed choice-prefix replay");
}

if (args.Length < 1 || args.Length > 2)
{
    Console.Error.WriteLine("Usage: Sts2Env.ContractTests <absolute-path-to-libsts2env_c.so> [large-portable-snapshot.json]");
    return 2;
}

try
{
    using var library = Sts2EnvLibrary.Load(args[0]);
    TestDescriptor(library);
    if (args.Length > 1) TestLargeSnapshot(library, args[1]);
    TestOrdinaryOwnedApi(library);
    TestPrivilegedRng(library);
    TestLibraryLifetime(library);
    TestSnapshotSessionBoundary(library);
    TestInvalidAndCapacityFailures(library);
    TestTypedOperationPrediction(library);
    TestChoiceContinuation(library);
    TestBoundaryContinuation(library);
    Console.WriteLine("Sts2Env .NET contract tests passed.");
    return 0;
}
catch (Exception exception)
{
    Console.Error.WriteLine($"Sts2Env contract test failed: {exception}");
    return 1;
}
