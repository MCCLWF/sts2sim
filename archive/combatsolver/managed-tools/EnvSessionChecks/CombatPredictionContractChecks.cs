using CombatSolverEnv.Api;
using Sts2EnvCompare;
using System.Text.Json;
using System.Text.Json.Nodes;

internal static class CombatPredictionContractChecks
{
    internal static void Run(EnvStateSnapshot ordinaryRoot, EnvStateSnapshot ordinaryNext)
    {
        using EnvCombatPredictionSession source = EnvPredictionApi.OpenCombatPrediction(ordinaryRoot);
        ulong hash = source.StateHash;
        string captured = source.CaptureSnapshot().Document.GetRawText();
        if (hash == ordinaryRoot.StateHash || source.Terminal.HasValue
            || source.CaptureSnapshot().Document.GetProperty("inner_snapshot").GetProperty("state_hash").GetUInt64() != ordinaryRoot.StateHash)
            throw new InvalidOperationException("Combat prediction did not preserve the supplied root in a distinct hash domain");
        if (source.PendingChoice is not null || source.AnswerPrefix.Count != 0)
            throw new InvalidOperationException("Normal combat prediction exposed a choice checkpoint");
        IReadOnlyList<EnvCombatAction> actions = source.LegalActions();
        if (actions.Count == 0 || source.StateHash != hash || captured != source.CaptureSnapshot().Document.GetRawText())
            throw new InvalidOperationException("Combat prediction legal query changed its source");
        EnvCombatGameQueries rootQueries = source.CaptureGameQueries();
        EnvCombatPolicyProbe rootPolicy = source.CapturePolicyProbe();
        EnvCombatCalculatedVars rootCalculated = source.CaptureCalculatedVars();
        EnvCombatOrbQueries rootOrbs = source.CaptureOrbs();
        EnvCombatPowerQueries rootPowers = source.CapturePowerQueries();
        if (rootQueries.PredictionHash != hash || rootQueries.PolicyProjectionComplete || rootQueries.ThreatProjectionComplete
            || rootPolicy.PredictionHash != hash || rootPolicy.PolicyProjectionComplete
            || rootCalculated.PredictionHash != hash || rootCalculated.PolicyProjectionComplete
            || rootOrbs.PredictionHash != hash || rootOrbs.OriginalStateKeyComplete
            || rootPowers.PredictionHash != hash
            || source.StateHash != hash || captured != source.CaptureSnapshot().Document.GetRawText()
            || rootQueries.Document.GetRawText() != source.CaptureGameQueries().Document.GetRawText())
            throw new InvalidOperationException("Combat prediction game getter query changed its source or protocol");
        using EnvCombatPredictionSession reference = source.Predict(new(EnvCombatActionKind.EndTurn));
        ulong nextHash = reference.StateHash;
        if (reference.Terminal.HasValue || reference.CaptureSnapshot().Document.GetProperty("inner_snapshot").GetProperty("state_hash").GetUInt64() != ordinaryNext.StateHash)
            throw new InvalidOperationException("Nonterminal combat scope changed ordinary turn semantics");
        // Repeated initialization, simultaneous reads and owned successors all
        // cross the actual P/Invoke boundary and the dedicated C handle type.
        Parallel.For(0, 16, new ParallelOptions { MaxDegreeOfParallelism = 4 }, _ =>
        {
            using EnvCombatPredictionSession clone = source.Clone();
            using EnvCombatPredictionSession next = clone.Predict(new(EnvCombatActionKind.EndTurn));
            using EnvCombatPredictionSession restored = EnvPredictionApi.RestoreCombatPrediction(source.CaptureSnapshot());
            if (!JsonElement.DeepEquals(rootCalculated.Document, clone.CaptureCalculatedVars().Document))
                throw new InvalidOperationException("Combat calculated query parallel/source isolation failed");
            if (!JsonElement.DeepEquals(rootOrbs.Document, clone.CaptureOrbs().Document))
                throw new InvalidOperationException("Combat orb query parallel/source isolation failed");
            if (!JsonElement.DeepEquals(rootPowers.Document, clone.CapturePowerQueries().Document))
                throw new InvalidOperationException("Combat power key query parallel/source isolation failed");
            if (!JsonElement.DeepEquals(rootPolicy.Document, clone.CapturePolicyProbe().Document)
                || rootPolicy.Document.GetProperty("format").GetString() != "combatsolver.env_policy_probe")
                throw new InvalidOperationException("Mod card play policy parallel/source isolation failed");
            if (clone.StateHash != hash || restored.StateHash != hash || next.StateHash != nextHash
                || source.StateHash != hash)
                throw new InvalidOperationException("Combat prediction parallel branches lost isolation");
        });
        if (source.TryPredict(new(EnvCombatActionKind.PlayCard, 9999), out var failed, out var reason)
            != EnvPredictionStatus.Error || failed is not null || string.IsNullOrEmpty(reason)
            || source.StateHash != hash)
            throw new InvalidOperationException("Combat prediction failure returned an owner or changed its source");
        if (source.TryPredict(new((EnvCombatActionKind)258), out failed, out reason)
            != EnvPredictionStatus.Error || failed is not null || string.IsNullOrEmpty(reason))
            throw new InvalidOperationException("Wide invalid action enum was truncated or accepted");
        using (EnvCombatPredictionSession recovered = source.Predict(new(EnvCombatActionKind.EndTurn)))
            if (recovered.StateHash != nextHash) throw new InvalidOperationException("Combat prediction error poisoned a later transition");

        void RejectNative(Action<JsonObject> edit, string diagnostic)
        {
            JsonObject document = JsonNode.Parse(captured)!.AsObject();
            edit(document);
            try
            {
                using NativeCombatPrediction.Session invalid = NativeCombatPrediction.Restore(document.ToJsonString());
                throw new Exception("Invalid prediction snapshot accepted");
            }
            catch (InvalidOperationException error) when (error.Message.Contains(diagnostic, StringComparison.Ordinal)) { }
            if (source.StateHash != hash) throw new InvalidOperationException("Rejected prediction import changed its source");
        }
        RejectNative(document => document["prediction_hash"] = 0, "hash mismatch");
        RejectNative(document => document["version"] = 4, "incompatible envelope");
        RejectNative(document => document["unknown"] = 1, "unexpected object fields");
        RejectNative(document => document.Remove("replay_terminal_base"), "unexpected object fields");
        RejectNative(document => document["replay_terminal_base"] = new JsonObject
        {
            ["terminal"] = null,
            ["history"] = document["history"]!.DeepClone(),
            ["power_events"] = document["power_events"]!.DeepClone()
        }, "choice boundary replay state");
        RejectNative(document => document["history_projection_complete"] = true, "unsupported complete history claim");
        RejectNative(document => document["history"]!["counters"]![0] = 1, "history counters mismatch");
        JsonObject malformedAttack = JsonNode.Parse(captured)!.AsObject();
        malformedAttack["history"]!["events"]!.AsArray().Add(
            JsonNode.Parse("[3,0,0,0,1,0,0,0,0,-1,1,false,0,false,false,false,[],null,null,null]"));
        try
        {
            using NativeCombatPrediction.Session invalid = NativeCombatPrediction.Restore(malformedAttack.ToJsonString());
            throw new Exception("Forged attack history accepted");
        }
        catch (InvalidOperationException error) when (error.Message.Contains("attack hit count mismatch", StringComparison.Ordinal)) { }
        RejectNative(document => document["terminal"] = new JsonObject { ["player_turn"] = 1, ["outcome"] = "victory" }, "terminal prediction flags");
        RejectNative(document => document["terminal"] = new JsonObject { ["player_turn"] = 1, ["outcome"] = "unknown" }, "unknown terminal outcome");
        try
        {
            using NativeEnv.Session invalid = NativeEnv.ImportCombatSnapshot(captured);
            throw new Exception("Prediction envelope accepted by ordinary Env import");
        }
        catch (InvalidOperationException error) when (error.Message.Contains("snapshot:", StringComparison.Ordinal)) { }
        try
        {
            EnvCombatPredictionSnapshot.Parse("{\"format\":\"sts2env.combat_prediction\",\"version\":5,\"prediction_hash\":0,\"terminal\":null}");
            throw new Exception("Partial public prediction snapshot accepted");
        }
        catch (InvalidOperationException error) when (error.Message.Contains("missing or unexpected fields", StringComparison.Ordinal)) { }

        EnvCombatPredictionSession disposed = source.Clone();
        EnvCombatPredictionSnapshot ownedSnapshot = disposed.CaptureSnapshot();
        EnvCombatGameQueries ownedQueries = disposed.CaptureGameQueries();
        EnvCombatPolicyProbe ownedPolicy = disposed.CapturePolicyProbe();
        EnvCombatCalculatedVars ownedCalculated = disposed.CaptureCalculatedVars();
        EnvCombatOrbQueries ownedOrbs = disposed.CaptureOrbs();
        EnvCombatPowerQueries ownedPowers = disposed.CapturePowerQueries();
        disposed.Dispose();
        disposed.Dispose();
        if (ownedQueries.PredictionHash != hash || ownedPolicy.PredictionHash != hash || ownedCalculated.PredictionHash != hash
            || ownedOrbs.PredictionHash != hash || ownedPowers.PredictionHash != hash
            || !JsonElement.DeepEquals(ownedPowers.Document, rootPowers.Document))
            throw new InvalidOperationException("Game getter query aliases a disposed owner");
        using (EnvCombatPredictionSession restored = EnvPredictionApi.RestoreCombatPrediction(ownedSnapshot))
            if (restored.StateHash != hash) throw new InvalidOperationException("Prediction snapshot aliases a disposed owner");
        try { disposed.LegalActions(); throw new Exception("Disposed prediction owner accessible"); }
        catch (ObjectDisposedException) { }
        try { disposed.CaptureGameQueries(); throw new Exception("Disposed prediction query accessible"); }
        catch (ObjectDisposedException) { }
        try { disposed.CapturePolicyProbe(); throw new Exception("Disposed policy probe accessible"); }
        catch (ObjectDisposedException) { }
        try { disposed.CaptureCalculatedVars(); throw new Exception("Disposed calculated query accessible"); }
        catch (ObjectDisposedException) { }
        try { disposed.CaptureOrbs(); throw new Exception("Disposed orb query accessible"); }
        catch (ObjectDisposedException) { }
        try { disposed.CapturePowerQueries(); throw new Exception("Disposed power key query accessible"); }
        catch (ObjectDisposedException) { }
        try { disposed.TryPredict(new(EnvCombatActionKind.EndTurn), out _, out _); throw new Exception("Disposed prediction failure was swallowed"); }
        catch (ObjectDisposedException) { }
        Console.WriteLine("Combat prediction API checks passed: independent hash domain, complete restore, 16 parallel branches, failure recovery, strict protocol and owned disposal.");
    }
}
