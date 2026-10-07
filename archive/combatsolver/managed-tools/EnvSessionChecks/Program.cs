using Sts2EnvCompare;
using CombatSolverEnv.Api;
using System.Text.Json;
using System.Text.Json.Nodes;

// Shared managed handle ownership is exercised without starting Godot. Native
// successors are compared with a sequential reference, including source hashes.
NativeEnv.Configure();
using NativeEnv.Session root = NativeEnv.OpenCombat("DUAL001", "CULTISTS_NORMAL");
ulong rootHash = root.StateHash;
string blankTemplate = NativeEnv.ExportBlankTemplateJson(root);
using (JsonDocument blank = JsonDocument.Parse(blankTemplate))
{
    if (blank.RootElement.GetProperty("format").GetString() != "sts2env.blank_template"
        || blank.RootElement.GetProperty("version").GetInt32() != 1
        || blank.RootElement.GetProperty("snapshot").GetProperty("decision").ValueKind != JsonValueKind.Null
        || blank.RootElement.GetProperty("monster_default").GetProperty("id").GetInt32() != 0
        || root.StateHash != rootHash)
        throw new InvalidOperationException("Blank live-root template changed the source or lost its envelope");
}
try
{
    using NativeEnv.Session invalidBlank = NativeEnv.ImportCombatSnapshot(blankTemplate);
    throw new InvalidOperationException("Blank live-root template was accepted as a combat snapshot");
}
catch (InvalidOperationException error) when (error.Message.Contains("snapshot:", StringComparison.Ordinal)) { }
using NativeEnv.Session reference = root.Predict(new EnvAction(3));
ulong expected = reference.StateHash;
if (expected == rootHash)
    throw new InvalidOperationException("EndTurn did not advance the reference state");

Parallel.For(0, 32, new ParallelOptions { MaxDegreeOfParallelism = 8 }, _ =>
{
    using NativeEnv.Session direct = root.Predict(new EnvAction(3));
    using NativeEnv.Session clone = root.Clone();
    using NativeEnv.Session clonedSuccessor = clone.Predict(new EnvAction(3));
    if (direct.StateHash != expected || clonedSuccessor.StateHash != expected
        || root.StateHash != rootHash || clone.StateHash != rootHash)
        throw new InvalidOperationException("Parallel branches diverged or mutated their source");
    using JsonDocument snapshot = JsonDocument.Parse(root.ExportSnapshotJson());
    if (snapshot.RootElement.GetProperty("state_hash").GetUInt64() != rootHash)
        throw new InvalidOperationException("Concurrent snapshot export changed the source");
    using JsonDocument machines = JsonDocument.Parse(root.ExportMonsterMachinesJson());
    if (machines.RootElement.GetProperty("state_hash").GetUInt64() != rootHash
        || machines.RootElement.GetProperty("runtime_id").GetString() != snapshot.RootElement.GetProperty("runtime_id").GetString())
        throw new InvalidOperationException("Concurrent monster descriptor export returned the wrong source");
});

using (NativeEnv.Session invalidSource = root.Clone())
{
    try
    {
        using NativeEnv.Session invalid = invalidSource.Predict(new EnvAction(2, 9999));
        throw new InvalidOperationException("Invalid hand index was accepted");
    }
    catch (InvalidOperationException error) when (error.Message != "Invalid hand index was accepted")
    {
        if (invalidSource.StateHash != rootHash)
            throw new InvalidOperationException("Rejected action mutated the source");
    }
    // A failed prediction must not poison a later successful one.
    using NativeEnv.Session recovered = invalidSource.Predict(new EnvAction(3));
    if (recovered.StateHash != expected)
        throw new InvalidOperationException("Prediction after an error diverged");
}

NativeEnv.Session disposed = root.Clone();
disposed.Dispose();
disposed.Dispose();
try
{
    disposed.InspectJson();
    throw new InvalidOperationException("Disposed session was accessible");
}
catch (ObjectDisposedException) { }
using EnvPredictionSession publicRoot = EnvPredictionApi.OpenSeededIroncladCombat(
    new EnvSeededCombatOptions("DUAL001", "CULTISTS_NORMAL"));
EnvStateSnapshot fullRoot = publicRoot.CaptureSnapshot();
using (EnvPredictionSession restored = EnvPredictionApi.RestoreCombatSnapshot(fullRoot))
using (EnvPredictionSession restoredNext = restored.Predict(new(EnvCombatActionKind.EndTurn)))
{
    if (restored.StateHash != rootHash || restoredNext.StateHash != expected)
        throw new InvalidOperationException("Imported root or successor differs from original");
}
JsonObject EditedRoot(Action<JsonObject> edit)
{
    JsonObject document = JsonNode.Parse(fullRoot.Document.GetRawText())!.AsObject();
    document["state_hash"] = null; // Fresh mapper root: native computes canonical hash.
    edit(document);
    return document;
}
void RejectImport(JsonObject document, string reason)
{
    try { using NativeEnv.Session bad = NativeEnv.ImportCombatSnapshot(document.ToJsonString()); throw new Exception("Invalid snapshot accepted"); }
    catch (InvalidOperationException error) when (error.Message.Contains(reason)) { }
    if (publicRoot.StateHash != rootHash) throw new InvalidOperationException("Failed import changed the existing root");
}
JsonObject changed = EditedRoot(document => document["state"]!["player_combat"]!["energy"] = 6);
using (NativeEnv.Session imported = NativeEnv.ImportCombatSnapshot(changed.ToJsonString()))
using (JsonDocument inspect = JsonDocument.Parse(imported.InspectJson()))
{
    if (inspect.RootElement.GetProperty("root").GetProperty("energy").GetInt32() != 6 || imported.StateHash == rootHash)
        throw new InvalidOperationException("Import rebuilt a seed root instead of loading supplied state");
}
JsonObject partialProfile = EditedRoot(document =>
{
    JsonNode runProfile = document["state"]!["run"]!["profile"]!;
    JsonNode playerProfile = document["state"]!["run"]!["player"]!["profile"]!;
    foreach (JsonNode node in new[] { runProfile, playerProfile })
    {
        node["number_of_runs"] = 0;
        node["encounters_seen_low"] = 0;
        node["encounters_seen_high"] = 0;
        node["full_veteran_compatible"] = false;
    }
    runProfile["unlocked_epoch_bits"] = 0x1ffffffffffffffUL;
    playerProfile["unlocked_epoch_bits"] = 3UL;
    playerProfile["encounters_seen_low"] = 4UL;
});
using (NativeEnv.Session imported = NativeEnv.ImportCombatSnapshot(partialProfile.ToJsonString()))
using (NativeEnv.Session clone = imported.Clone())
using (JsonDocument restored = JsonDocument.Parse(clone.ExportSnapshotJson()))
{
    JsonElement run = restored.RootElement.GetProperty("state").GetProperty("run");
    foreach (var (profile, epochs, seenLow) in new[] {
        (run.GetProperty("profile"), 0x1ffffffffffffffUL, 0UL),
        (run.GetProperty("player").GetProperty("profile"), 3UL, 4UL) })
        if (profile.GetProperty("number_of_runs").GetInt32() != 0
            || profile.GetProperty("unlocked_epoch_bits").GetUInt64() != epochs
            || profile.GetProperty("encounters_seen_low").GetUInt64() != seenLow
            || profile.GetProperty("encounters_seen_high").GetUInt64() != 0
            || profile.GetProperty("full_veteran_compatible").GetBoolean())
            throw new InvalidOperationException("Partial profile facts did not survive import and clone");
    ulong before = imported.StateHash;
    try { using NativeEnv.Session successor = imported.Predict(new EnvAction(3)); throw new Exception("Partial profile action was accepted"); }
    catch (InvalidOperationException error) when (error.Message.Contains("live profile actions require", StringComparison.Ordinal)) { }
    if (imported.StateHash != before || clone.StateHash != before)
        throw new InvalidOperationException("Rejected partial-profile action mutated a source");
}
RejectImport(EditedRoot(document => document["schema_sha256"] = new string('0', 64)), "incompatible schema_sha256");
RejectImport(EditedRoot(document => document["state"]!["run"]!["profile"]!["unlocked_epoch_bits"] = 0),
    "full veteran profile facts inconsistent");
RejectImport(EditedRoot(document => document["state"]!["run"]!["profile"]!["encounters_seen_high"] = ulong.MaxValue),
    "profile encounter bit outside catalog");
RejectImport(EditedRoot(document => document["state"]!["run"]!["event_synchronizer"]!["event_count"] = 2),
    "event synchronizer event count");
RejectImport(EditedRoot(document => document["state"]!["run"]!["event_synchronizer"]!["canonical_event"] = 1),
    "empty event synchronizer state");
RejectImport(EditedRoot(document => document["runtime_id"] = "different-runtime"), "incompatible runtime_id");
RejectImport(EditedRoot(document => document["state"]!["cards"]!["slots"]![0]!["value"]!["id"] = 65535), "unknown enum");
RejectImport(EditedRoot(document => document["state"]!["player_combat"]!["piles"]![0]![0] = 65535), "reference outside pool");
RejectImport(EditedRoot(document => document["state"]!["cards"]!["count"] = 319), "pool count mismatch");
RejectImport(EditedRoot(document => document["state"]!["cards"]!["slots"]![0]!["value"]!.AsObject().Remove("cost_base")), "missing field cost_base");
RejectImport(EditedRoot(document => document["state"]!["cards"]!["slots"]![0]!["value"]!["unexpected"] = 1), "unexpected object fields");
RejectImport(EditedRoot(document => document["state"]!["run"]!["rng"]!["rngs"]![0]!["s0"] = -1), "integer out of range");
// Clearing the supplied hash is allowed for a complete new combat root, but
// a numerical hash is always verified when present.
JsonObject wrongHash = EditedRoot(_ => { }); wrongHash["state_hash"] = 0;
RejectImport(wrongHash, "state hash mismatch");
if (fullRoot.StateHash != rootHash || fullRoot.Decision != 1
    || fullRoot.State.GetProperty("run").GetProperty("rng").GetProperty("rngs").GetArrayLength() != 12)
    throw new InvalidOperationException("Complete snapshot omitted root fields");
JsonElement cardPool = fullRoot.State.GetProperty("cards");
HashSet<int> liveCards = cardPool.GetProperty("slots").EnumerateArray()
    .Select(slot => slot.GetProperty("index").GetInt32()).ToHashSet();
if (liveCards.Count != cardPool.GetProperty("count").GetInt32()
    || fullRoot.State.GetProperty("player_combat").GetProperty("piles").EnumerateArray()
        .SelectMany(pile => pile.EnumerateArray()).Any(reference => !liveCards.Contains(reference.GetInt32())))
    throw new InvalidOperationException("Snapshot card pool cannot resolve its pile references");
EnvRngState shuffle = fullRoot.GetRunRng(EnvRunRngStream.Shuffle);
foreach (var stream in Enum.GetValues<EnvPlayerRngStream>())
{
    JsonElement rng = fullRoot.State.GetProperty("run").GetProperty("player").GetProperty("rng").GetProperty("rngs")[(int)stream];
    if (fullRoot.GetPlayerRng(stream) != new EnvRngState(rng.GetProperty("counter").GetInt32(), rng.GetProperty("s0").GetUInt64(),
        rng.GetProperty("s1").GetUInt64(), rng.GetProperty("s2").GetUInt64(), rng.GetProperty("s3").GetUInt64()))
        throw new InvalidOperationException("Public player RNG inspection lost a stream");
}
foreach ((EnvRngState altered, string field) in new[] {
    (shuffle with { Counter = shuffle.Counter + 1 }, "counter"),
    (shuffle with { State0 = shuffle.State0 ^ 1UL }, "s0"),
    (shuffle with { State1 = shuffle.State1 ^ 1UL }, "s1"),
    (shuffle with { State2 = shuffle.State2 ^ 1UL }, "s2"),
    (shuffle with { State3 = shuffle.State3 ^ 1UL }, "s3") })
{
    try
    {
        EnvRngComparison.RequireMatch(altered, shuffle, EnvRunRngStream.Shuffle, "root");
        throw new Exception($"RNG mismatch accepted: {field}");
    }
    catch (InvalidOperationException error) when (error.Message.StartsWith($"root_rng_mismatch:Shuffle:{field};")) { }
}
using (EnvPredictionSession snapshotSource = publicRoot.Clone())
{
    EnvStateSnapshot captured = snapshotSource.CaptureSnapshot();
    JsonElement machines = snapshotSource.InspectMonsterMachines();
    snapshotSource.Dispose();
    if (captured.GetRunRng(EnvRunRngStream.Shuffle) != shuffle)
        throw new InvalidOperationException("Snapshot aliases the disposed source");
    if (captured.GetPlayerRng(EnvPlayerRngStream.Rewards) != fullRoot.GetPlayerRng(EnvPlayerRngStream.Rewards))
        throw new InvalidOperationException("Player RNG aliases the disposed source");
    if (machines.GetProperty("machines").GetArrayLength() != 2
        || machines.GetProperty("machines")[0].GetProperty("states")[0].GetProperty("on_perform").GetString() != "CalcifiedCultist.IncantationMove"
        || machines.GetProperty("state_hash").GetUInt64() != rootHash)
        throw new InvalidOperationException("Monster descriptors alias the disposed source or lose symbolic callbacks");
}
if (publicRoot.TryPredict(new EnvCombatAction(EnvCombatActionKind.EndTurn), out var publicNext, out var reason)
    != EnvPredictionStatus.Ready || publicNext is null || reason is not null)
    throw new InvalidOperationException("Public API failed to return an owned successor");
using (publicNext)
{
    if (publicNext.StateHash != expected || publicRoot.StateHash != rootHash)
        throw new InvalidOperationException($"Public API state differs: successor={publicNext.StateHash}/{expected}, source={publicRoot.StateHash}/{rootHash}");
}
if (publicRoot.TryPredict(new EnvCombatAction(EnvCombatActionKind.PlayCard, 9999), out var failed, out reason)
    != EnvPredictionStatus.Error || failed is not null || string.IsNullOrEmpty(reason)
    || publicRoot.StateHash != rootHash)
    throw new InvalidOperationException("Public API failure contract was violated");
Console.WriteLine("Env session checks passed: 64 parallel predictions, source isolation, failure recovery, disposal.");
Console.WriteLine("Public prediction API checks passed: owned successor and explicit failure without source mutation.");
CombatPredictionContractChecks.Run(fullRoot, EnvStateSnapshot.Parse(reference.ExportSnapshotJson()));
if (args.Length == 1)
{
    EnvStateSnapshot scalars = EnvStateSnapshot.Parse(File.ReadAllText(Path.Combine(args[0], "scalars.json")));
    if (scalars.GetRunRng(EnvRunRngStream.UpFront).State0 != ulong.MaxValue
        || scalars.State.GetProperty("combat").GetProperty("gold_proportion").GetProperty("f32_bits").GetUInt32() != 0x80000000)
        throw new InvalidOperationException("Exact scalar snapshot values lost precision");
    // Slot order follows pool refs, not hand order; find the modified instance.
    bool foundDecimal = scalars.State.GetProperty("cards").GetProperty("slots").EnumerateArray().Any(slot =>
    {
        return slot.GetProperty("value").GetProperty("var_overrides").EnumerateArray().Any(overridden =>
        {
            JsonElement words = overridden.GetProperty("value").GetProperty("decimal_bits");
            return words[0].GetUInt32() == 123 && words[3].GetUInt32() == 0x80020000;
        });
    });
    if (!foundDecimal) throw new InvalidOperationException("Decimal words lost scale or sign");
    using JsonDocument choiceDocument = JsonDocument.Parse(File.ReadAllText(Path.Combine(args[0], "choice.json")));
    string? choiceFormat = choiceDocument.RootElement.GetProperty("format").GetString();
    if (choiceFormat == "sts2env.combat_prediction")
    {
        EnvCombatPredictionSnapshot predictionChoice = EnvCombatPredictionSnapshot.Parse(
            choiceDocument.RootElement.GetRawText());
        EnvChoiceRequest? request = predictionChoice.PendingChoice;
        if (request is null || request.OptionCount != 3 || request.MinSelect != 0 || request.MaxSelect != 1
            || request.Prompt != "count" || !request.Subset || !request.CanSkip
            || !request.Cards.SequenceEqual([22, 23, 24]) || request.Selected.Count != 0
            || predictionChoice.AnswerPrefix.Count != 0)
            throw new InvalidOperationException("Typed prediction choice checkpoint projection mismatch");
    }
    else
    {
        EnvStateSnapshot choice = EnvStateSnapshot.Parse(choiceDocument.RootElement.GetRawText());
        try { using var foreign = EnvPredictionApi.RestoreCombatSnapshot(choice); throw new Exception("Foreign runtime snapshot accepted"); }
        catch (InvalidOperationException error) when (error.Message.Contains("incompatible runtime_id")) { }
        if (choice.Decision != 7 || choice.Document.GetProperty("pending").GetProperty("kind").GetInt32() != 2
            || choice.Document.GetProperty("replay_base").GetProperty("cards").GetProperty("slots").GetArrayLength() == 0)
            throw new InvalidOperationException("Choice replay state incomplete");
        EnvChoiceRequest? request = choice.PendingChoice;
        if (request is null || request.OptionCount != 3 || request.MinSelect != 0 || request.MaxSelect != 1
            || request.Prompt != "count" || !request.Subset || !request.CanSkip
            || !request.Cards.SequenceEqual([22, 23, 24]) || request.Selected.Count != 0
            || choice.AnswerPrefix.Count != 0)
            throw new InvalidOperationException("Typed choice checkpoint projection mismatch");
        string invalidVersion = choice.Document.GetRawText().Replace("\"version\":2", "\"version\":3");
        try { EnvStateSnapshot.Parse(invalidVersion); throw new Exception("Future format accepted"); }
        catch (InvalidOperationException) { }
        string invalidOrdering = choice.Document.GetRawText().Replace("\"up_front\",\"shuffle\"", "\"shuffle\",\"up_front\"");
        try { EnvStateSnapshot.Parse(invalidOrdering); throw new Exception("Changed RNG stream order accepted"); }
        catch (InvalidOperationException) { }
    }
}
Console.WriteLine("Complete snapshot API checks passed: all run streams, exact scalars, choice base, concurrent export, owned JSON.");
Console.WriteLine("Snapshot import checks passed: owned restore, supplied state, strict fields/schema/runtime/refs/hash and failure isolation.");
