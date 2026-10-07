using System.Text.Json;
using Sts2EnvCompare;

namespace CombatSolverEnv.Api;

public enum EnvCombatTerminalOutcome { Victory, Defeat }
public readonly record struct EnvCombatTerminalStamp(int PlayerTurn, EnvCombatTerminalOutcome Outcome);

/// <summary>
/// Owned values from shipped game getters on a private clone of a combat
/// prediction. These values do not yet project CombatSolver's policy features
/// or threat calculation, so they cannot be used as a search state key.
/// </summary>
public sealed class EnvCombatGameQueries
{
    private EnvCombatGameQueries(JsonElement document) => Document = document;
    public JsonElement Document { get; }
    public ulong PredictionHash => Document.GetProperty("prediction_hash").GetUInt64();
    public bool PolicyProjectionComplete => false;
    public bool ThreatProjectionComplete => false;

    internal static EnvCombatGameQueries Parse(string json, ulong expectedHash)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(json);
        using JsonDocument parsed = JsonDocument.Parse(json);
        JsonElement document = parsed.RootElement;
        if (document.GetProperty("format").GetString() != "sts2env.combat_prediction_queries"
            || document.GetProperty("version").GetInt32() != 4
            || document.GetProperty("query_semantics").GetString() != "game"
            || document.GetProperty("prediction_hash").GetUInt64() != expectedHash)
            throw new InvalidOperationException("Combat prediction game query protocol or source hash mismatch");
        _ = document.GetProperty("player").GetProperty("energy").GetInt32();
        _ = document.GetProperty("piles").GetArrayLength();
        _ = document.GetProperty("roster").GetArrayLength();
        if (document.TryGetProperty("policy_projection_complete", out _)
            || document.TryGetProperty("threat_projection_complete", out _))
            throw new InvalidOperationException("Game query unexpectedly contains search coverage policy");
        return new(document.Clone());
    }
}

/// <summary>
/// Owned Mod projection for the core of CombatSolver's CanPlayCard rule.
/// Other policy reads and threat/score projection are not covered; this
/// document cannot authorize an Env search request.
/// </summary>
public sealed class EnvCombatPolicyProbe
{
    private EnvCombatPolicyProbe(JsonElement document) => Document = document;
    public JsonElement Document { get; }
    public ulong PredictionHash => Document.GetProperty("prediction_hash").GetUInt64();
    public bool PolicyProjectionComplete => Document.GetProperty("policy_projection_complete").GetBoolean();

    internal static EnvCombatPolicyProbe From(EnvCombatGameQueries queries)
        => new(EnvCardPlayPolicy.Project(queries));
}

/// <summary>
/// Owned privileged document for the combat search scope. It includes hidden
/// RNG and preserves terminal state before after-combat hooks and rewards.
/// It is distinct from the ordinary run snapshot and cannot be restored there.
/// </summary>
public sealed class EnvCombatPredictionSnapshot
{
    private EnvCombatPredictionSnapshot(JsonElement document) => Document = document;
    public JsonElement Document { get; }
    public ulong PredictionHash => Document.GetProperty("prediction_hash").GetUInt64();
    public bool HistoryProjectionComplete => Document.GetProperty("history_projection_complete").GetBoolean();
    public JsonElement History => Document.GetProperty("history");
    /// <summary>Choice request at the prediction boundary, or null otherwise.</summary>
    public EnvChoiceRequest? PendingChoice
    {
        get
        {
            JsonElement inner = Document.GetProperty("inner_snapshot");
            JsonElement choice = inner.GetProperty("choice");
            return choice.ValueKind == JsonValueKind.Null
                ? null : EnvChoiceRequest.Parse(choice, "Combat prediction");
        }
    }
    /// <summary>Owned answer prefix retained by the choice replay boundary.</summary>
    public IReadOnlyList<int> AnswerPrefix
        => Document.GetProperty("inner_snapshot").GetProperty("answers")
            .EnumerateArray().Select(value => value.GetInt32()).ToArray();
    public EnvCombatTerminalStamp? Terminal
    {
        get
        {
            JsonElement terminal = Document.GetProperty("terminal");
            if (terminal.ValueKind == JsonValueKind.Null) return null;
            EnvCombatTerminalOutcome outcome = terminal.GetProperty("outcome").GetString() switch
            {
                "victory" => EnvCombatTerminalOutcome.Victory,
                "defeat" => EnvCombatTerminalOutcome.Defeat,
                _ => throw new InvalidOperationException("Unknown combat prediction terminal outcome"),
            };
            int turn = terminal.GetProperty("player_turn").GetInt32();
            if (turn < 1) throw new InvalidOperationException("Invalid combat prediction terminal turn");
            return new(turn, outcome);
        }
    }

    public static EnvCombatPredictionSnapshot Parse(string json)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(json);
        using JsonDocument parsed = JsonDocument.Parse(json);
        JsonElement document = parsed.RootElement;
        RequireFields(document, ["format", "version", "inner_snapshot", "terminal", "history", "power_events", "history_projection_complete", "replay_terminal_base", "prediction_hash"]);
        if (document.GetProperty("format").GetString() != "sts2env.combat_prediction"
            || document.GetProperty("version").GetInt32() != 7)
            throw new InvalidOperationException("Combat prediction snapshot protocol mismatch");
        EnvCombatPredictionSnapshot snapshot = new(document.Clone());
        _ = snapshot.PredictionHash;
        if (snapshot.HistoryProjectionComplete)
            throw new InvalidOperationException("Combat prediction history unexpectedly claims complete coverage");
        ValidateHistory(snapshot.History);
        ValidatePowerEvents(document.GetProperty("power_events"));
        EnvCombatTerminalStamp? terminal = snapshot.Terminal;
        if (terminal.HasValue) RequireFields(document.GetProperty("terminal"), ["player_turn", "outcome"]);
        EnvStateSnapshot inner = EnvStateSnapshot.Parse(document.GetProperty("inner_snapshot").GetRawText());
        JsonElement replay = document.GetProperty("replay_terminal_base");
        bool choice = inner.Decision == 7;
        if (choice)
        {
            RequireFields(replay, ["terminal", "history", "power_events"]);
            ValidateHistory(replay.GetProperty("history"));
            ValidatePowerEvents(replay.GetProperty("power_events"));
            if (terminal.HasValue || replay.GetProperty("terminal").ValueKind != JsonValueKind.Null)
                throw new InvalidOperationException("Terminal combat prediction cannot have a pending choice");
        }
        else if (replay.ValueKind != JsonValueKind.Null)
            throw new InvalidOperationException("Combat prediction has an inactive choice boundary base");
        return snapshot;
    }

    private static void ValidatePowerEvents(JsonElement events)
    {
        if (events.ValueKind != JsonValueKind.Array || events.GetArrayLength() > 100000)
            throw new InvalidOperationException("Invalid power events");
        foreach (JsonElement entry in events.EnumerateArray())
        {
            if (entry.ValueKind != JsonValueKind.Array || entry.GetArrayLength() != 8)
                throw new InvalidOperationException("Invalid power event shape");
            int kind = entry[0].GetInt32();
            if (kind is < 0 or > 2 || entry[1].GetInt32() is < 1 or > ushort.MaxValue
                || entry[2].GetInt32() is < 0 or > ushort.MaxValue
                || entry[4].GetInt32() is < 1 or > ushort.MaxValue
                || (kind == 0 ? entry[5].GetInt32() == entry[6].GetInt32()
                    : entry[5].GetInt32() != entry[6].GetInt32() || !entry[7].GetBoolean()))
                throw new InvalidOperationException("Inconsistent power event");
            _ = entry[3].GetUInt32();
            _ = entry[7].GetBoolean();
        }
    }

    private static void ValidateHistory(JsonElement history)
    {
        RequireFields(history, ["events", "counters"]);
        JsonElement entries = history.GetProperty("events");
        JsonElement counters = history.GetProperty("counters");
        if (entries.ValueKind != JsonValueKind.Array || counters.ValueKind != JsonValueKind.Array
            || counters.GetArrayLength() != 6)
            throw new InvalidOperationException("Combat prediction history shape mismatch");
        foreach (JsonElement eventEntry in entries.EnumerateArray())
        {
            if (eventEntry.ValueKind != JsonValueKind.Array || eventEntry.GetArrayLength() != 20
                || eventEntry[16].ValueKind != JsonValueKind.Array
                || eventEntry[16].EnumerateArray().Any(hit => hit.ValueKind != JsonValueKind.Array || hit.GetArrayLength() != 8)
                || eventEntry[17].ValueKind is not (JsonValueKind.Null or JsonValueKind.Array)
                || eventEntry[17].ValueKind == JsonValueKind.Array && eventEntry[17].GetArrayLength() != 3
                || eventEntry[18].ValueKind is not (JsonValueKind.Null or JsonValueKind.Array)
                || eventEntry[18].ValueKind == JsonValueKind.Array && eventEntry[18].GetArrayLength() != 10
                || eventEntry[19].ValueKind is not (JsonValueKind.Null or JsonValueKind.Array)
                || eventEntry[19].ValueKind == JsonValueKind.Array && eventEntry[19].GetArrayLength() != 2)
                throw new InvalidOperationException("Combat prediction history event shape mismatch");
            int kind = eventEntry[0].GetInt32();
            if (kind is < 0 or > 9)
                throw new InvalidOperationException("Unknown combat execution fact kind");
            if (kind == 9 && (eventEntry[17].ValueKind != JsonValueKind.Array
                || eventEntry[18].ValueKind != JsonValueKind.Array))
                throw new InvalidOperationException("Missing card effect identity");
            if (eventEntry[18].ValueKind == JsonValueKind.Array && (eventEntry[18][9].GetInt32() is < 0 or > 9))
                throw new InvalidOperationException("Unknown card play target type");
        }
    }

    private static void RequireFields(JsonElement value, string[] expected)
    {
        if (value.ValueKind != JsonValueKind.Object)
            throw new InvalidOperationException("Combat prediction snapshot requires an object");
        string[] fields = value.EnumerateObject().Select(field => field.Name).ToArray();
        if (fields.Length != expected.Length || fields.Distinct(StringComparer.Ordinal).Count() != expected.Length
            || expected.Any(field => !fields.Contains(field, StringComparer.Ordinal)))
            throw new InvalidOperationException("Combat prediction snapshot has missing or unexpected fields");
    }
}

/// <summary>
/// Independent engine branches for combat search. No search algorithm is run.
/// Every Clone/Predict returns a new owner; failures preserve the source.
/// Terminal branches have no legal actions and reject further prediction.
/// The installed Mod's complete same-policy backend remains separately gated.
/// </summary>
public sealed class EnvCombatPredictionSession : IDisposable
{
    private readonly NativeCombatPrediction.Session _session;
    internal EnvCombatPredictionSession(NativeCombatPrediction.Session session) => _session = session;
    public ulong StateHash => _session.Hash;
    public EnvCombatTerminalStamp? Terminal => CaptureSnapshot().Terminal;
    public EnvChoiceRequest? PendingChoice => CaptureSnapshot().PendingChoice;
    public IReadOnlyList<int> AnswerPrefix => CaptureSnapshot().AnswerPrefix;
    public EnvCombatPredictionSnapshot CaptureSnapshot() => EnvCombatPredictionSnapshot.Parse(_session.ExportSnapshotJson());
    /// <summary>Diagnostic structural combat view, including terminal branches. The source is unchanged.</summary>
    public string InspectJson() => _session.InspectJson();
    public EnvCombatGameQueries CaptureGameQueries() => EnvCombatGameQueries.Parse(_session.ExportQueriesJson(), StateHash);
    public EnvCombatPolicyProbe CapturePolicyProbe() => EnvCombatPolicyProbe.From(CaptureGameQueries());
    public EnvCombatCalculatedVars CaptureCalculatedVars() => EnvCombatCalculatedVars.Parse(_session.ExportCalculatedVarsJson(), StateHash);
    public EnvCombatOrbQueries CaptureOrbs() => EnvCombatOrbQueries.Parse(_session.ExportOrbsJson(), StateHash);
    public EnvCombatPowerQueries CapturePowerQueries() => EnvCombatPowerQueries.Parse(_session.ExportPowerQueriesJson(), StateHash);
    public IReadOnlyList<EnvCombatAction> LegalActions() => _session.LegalActions();
    public EnvCombatPredictionSession Clone() => new(_session.Clone());

    public EnvCombatPredictionSession Predict(EnvCombatAction action)
    {
        if (!Enum.IsDefined(action.Kind)) throw new ArgumentOutOfRangeException(nameof(action), "Unknown combat action kind");
        return new(_session.Predict(action));
    }

    public EnvPredictionStatus TryPredict(EnvCombatAction action, out EnvCombatPredictionSession? successor, out string? reason)
    {
        successor = null;
        reason = null;
        try { successor = Predict(action); return EnvPredictionStatus.Ready; }
        catch (EnvPredictionUnportedException error) { reason = error.Message; return EnvPredictionStatus.Unported; }
        catch (InvalidOperationException error) when (error is not ObjectDisposedException)
        { reason = error.Message; return EnvPredictionStatus.Error; }
        catch (ArgumentOutOfRangeException error) { reason = error.Message; return EnvPredictionStatus.Error; }
    }

    public void Dispose() => _session.Dispose();
}
