using System.Runtime.InteropServices;
using System.Text.Json;

namespace Sts2Env;

public enum Sts2ActionKind
{
    StartRun = 0,
    MapMove = 1,
    PlayCard = 2,
    EndTurn = 3,
    TakeReward = 4,
    Proceed = 5,
    EventOption = 6,
    RestOption = 7,
    ShopBuy = 8,
    TreasurePick = 9,
    UsePotion = 10,
    DiscardPotion = 11,
    Answer = 12,
    Resume = 13,
}

public enum Sts2DecisionKind
{
    Map = 0,
    Combat = 1,
    Rewards = 2,
    Event = 3,
    RestSite = 4,
    Shop = 5,
    Treasure = 6,
    Choice = 7,
    Ended = 8,
    Boundary = 9,
}

/// <summary>A C ABI action. Kind remains an integer so newer action kinds can pass through unchanged.</summary>
public readonly record struct Sts2Action(int Kind, int Index = -1, int Target = -1, int Col = 0, int Row = 0)
{
    public Sts2Action(Sts2ActionKind kind, int index = -1, int target = -1, int col = 0, int row = 0)
        : this((int)kind, index, target, col, row) { }
}

/// <summary>A decision code. Unknown future values are retained in RawKind.</summary>
public readonly record struct Sts2Decision(int RawKind)
{
    public Sts2DecisionKind? KnownKind => Enum.IsDefined(typeof(Sts2DecisionKind), RawKind)
        ? (Sts2DecisionKind)RawKind : null;
    public bool IsChoice => RawKind == (int)Sts2DecisionKind.Choice;
}

public enum Sts2TransformRngSource
{
    PlayerTransformations,
    RunNiche,
    RunCombatCardSelection,
    Event,
}

public enum Sts2TransformMode
{
    Batch,
    Sequential,
}

public enum Sts2RewardRoom
{
    Monster,
    Elite,
    Boss,
    Treasure,
}

/// <summary>A typed request for pure prediction through the native operation ABI.</summary>
public abstract record Sts2Operation
{
    public Sts2StopCondition Stop { get; init; } = Sts2StopCondition.OperationDone;
    public ulong? ExpectedSourceHash { get; init; }
    internal virtual bool RequiresSourceBinding => false;
    internal abstract string KindName { get; }
    internal abstract void WriteParameters(Utf8JsonWriter writer);
}

public enum Sts2StopCondition { OperationDone, EnemyPhaseDone, SafeOutcome, EnemyPhaseOrOutcome }

public sealed record Sts2ActionOperation(Sts2Action Action) : Sts2Operation
{
    internal override string KindName => "action";
    internal override void WriteParameters(Utf8JsonWriter writer)
    {
        writer.WriteStartObject();
        writer.WriteNumber("kind", Action.Kind);
        writer.WriteNumber("index", Action.Index);
        writer.WriteNumber("target", Action.Target);
        writer.WriteNumber("col", Action.Col);
        writer.WriteNumber("row", Action.Row);
        writer.WriteEndObject();
    }
}

public sealed record Sts2ObtainRelicOperation(string? RelicId, ushort? RelicRef, int InsertIndex = -1) : Sts2Operation
{
    internal override bool RequiresSourceBinding => RelicRef is not null;
    internal override string KindName => "obtain_relic";
    internal override void WriteParameters(Utf8JsonWriter writer)
    {
        writer.WriteStartObject();
        if (RelicId is not null) writer.WriteString("relic_id", RelicId);
        if (RelicRef is ushort relicRef) writer.WriteNumber("relic_ref", relicRef);
        writer.WriteNumber("insert_index", InsertIndex);
        writer.WriteEndObject();
    }
}

public sealed record Sts2TransformCardsOperation : Sts2Operation
{
    internal override bool RequiresSourceBinding => true;
    private readonly ushort[] _cards;

    public Sts2TransformCardsOperation(IEnumerable<ushort> cards, Sts2TransformRngSource rngSource,
                                       Sts2TransformMode mode = Sts2TransformMode.Batch)
    {
        ArgumentNullException.ThrowIfNull(cards);
        _cards = cards.ToArray();
        RngSource = rngSource;
        Mode = mode;
    }

    public IReadOnlyList<ushort> Cards => Array.AsReadOnly(_cards);
    public Sts2TransformRngSource RngSource { get; }
    public Sts2TransformMode Mode { get; }
    internal override string KindName => "transform_cards";
    internal override void WriteParameters(Utf8JsonWriter writer)
    {
        string rngSource = RngSource switch
        {
            Sts2TransformRngSource.PlayerTransformations => "player_transformations",
            Sts2TransformRngSource.RunNiche => "run_niche",
            Sts2TransformRngSource.RunCombatCardSelection => "run_combat_card_selection",
            Sts2TransformRngSource.Event => "event",
            _ => throw new ArgumentOutOfRangeException(nameof(RngSource)),
        };
        string mode = Mode switch
        {
            Sts2TransformMode.Batch => "batch",
            Sts2TransformMode.Sequential => "sequential",
            _ => throw new ArgumentOutOfRangeException(nameof(Mode)),
        };
        writer.WriteStartObject();
        writer.WriteStartArray("cards");
        foreach (ushort card in _cards) writer.WriteNumberValue(card);
        writer.WriteEndArray();
        writer.WriteString("rng_source", rngSource);
        writer.WriteString("mode", mode);
        writer.WriteEndObject();
    }
}

public sealed record Sts2GenerateRewardsOperation(Sts2RewardRoom Room) : Sts2Operation
{
    internal override string KindName => "generate_rewards";
    internal override void WriteParameters(Utf8JsonWriter writer)
    {
        string room = Room switch
        {
            Sts2RewardRoom.Monster => "MONSTER",
            Sts2RewardRoom.Elite => "ELITE",
            Sts2RewardRoom.Boss => "BOSS",
            Sts2RewardRoom.Treasure => "TREASURE",
            _ => throw new ArgumentOutOfRangeException(nameof(Room)),
        };
        writer.WriteStartObject();
        writer.WriteString("room", room);
        writer.WriteEndObject();
    }
}

public sealed record Sts2RerollCardRewardOperation(ushort RewardRef) : Sts2Operation
{
    internal override bool RequiresSourceBinding => true;
    internal override string KindName => "reroll_card_reward";
    internal override void WriteParameters(Utf8JsonWriter writer)
    {
        writer.WriteStartObject();
        writer.WriteNumber("reward_ref", RewardRef);
        writer.WriteEndObject();
    }
}

public sealed record Sts2MerchantPurchaseOperation(int EntryIndex, bool IgnoreCost = false) : Sts2Operation
{
    internal override bool RequiresSourceBinding => true;
    internal override string KindName => "merchant_purchase";
    internal override void WriteParameters(Utf8JsonWriter writer)
    {
        writer.WriteStartObject();
        writer.WriteNumber("entry_index", EntryIndex);
        writer.WriteBoolean("ignore_cost", IgnoreCost);
        writer.WriteEndObject();
    }
}

/// <summary>Execution facts returned by a typed operation.</summary>
public sealed record Sts2OperationResult(string? Kind, Sts2MerchantPurchaseResult? MerchantPurchase);

/// <summary>Result facts for the native merchant purchase operation.</summary>
public sealed record Sts2MerchantPurchaseResult(int EntryIndex, bool Success, bool IgnoreCost);

/// <summary>Convenience constructors for requests; source hashes are supplied by the Env call.</summary>
public static class Sts2Operations
{
    public static Sts2ActionOperation Action(Sts2Action action) => new(action);
    public static Sts2ObtainRelicOperation ObtainRelic(string relicId, int insertIndex = -1) => new(relicId, null, insertIndex);
    public static Sts2ObtainRelicOperation ObtainRelicReference(ushort relicRef, ulong sourceHash, int insertIndex = -1) =>
        new(null, relicRef, insertIndex) { ExpectedSourceHash = sourceHash };
    public static Sts2TransformCardsOperation TransformCards(IEnumerable<ushort> cards, Sts2TransformRngSource rngSource,
                                                               ulong sourceHash, Sts2TransformMode mode = Sts2TransformMode.Batch) =>
        new(cards, rngSource, mode) { ExpectedSourceHash = sourceHash };
    public static Sts2GenerateRewardsOperation GenerateRewards(Sts2RewardRoom room) => new(room);
    public static Sts2RerollCardRewardOperation RerollCardReward(ushort rewardRef, ulong sourceHash) =>
        new(rewardRef) { ExpectedSourceHash = sourceHash };
    public static Sts2MerchantPurchaseOperation MerchantPurchase(int entryIndex, ulong sourceHash, bool ignoreCost = false) =>
        new(entryIndex, ignoreCost) { ExpectedSourceHash = sourceHash };
}

internal static class Sts2OperationJson
{
    internal static byte[] Serialize(Sts2Operation operation, ulong sourceHash)
    {
        ArgumentNullException.ThrowIfNull(operation);
        using var stream = new MemoryStream();
        using (var writer = new Utf8JsonWriter(stream))
        {
            writer.WriteStartObject();
            writer.WriteString("format", "sts2env.operation");
            writer.WriteNumber("version", 1);
            writer.WriteString("kind", operation.KindName);
            writer.WriteNumber("source_hash", sourceHash);
            writer.WriteString("stop", operation.Stop switch {
                Sts2StopCondition.OperationDone => "operation_done",
                Sts2StopCondition.EnemyPhaseDone => "enemy_phase_done",
                Sts2StopCondition.SafeOutcome => "safe_outcome",
                Sts2StopCondition.EnemyPhaseOrOutcome => "enemy_phase_or_outcome",
                _ => throw new ArgumentOutOfRangeException(nameof(operation.Stop)),
            });
            writer.WritePropertyName("params");
            operation.WriteParameters(writer);
            writer.WriteEndObject();
        }
        return stream.ToArray();
    }
}

/// <summary>A snapshot captured from one loaded native-library session.</summary>
public sealed class Sts2EnvSnapshot
{
    internal Sts2EnvSnapshot(string json, Sts2EnvLibrary owner)
    {
        Json = json;
        Owner = owner;
    }

    public string Json { get; }
    internal Sts2EnvLibrary Owner { get; }
}

/// <summary>Full state portable to a process loading the identical native build.</summary>
public sealed record Sts2EnvPortableSnapshot(string Json);

/// <summary>Owned branch plus a copied factual preview document.</summary>
public sealed class Sts2EnvPreview : IDisposable
{
    internal Sts2EnvPreview(Sts2Environment successor, string resultJson)
    {
        Successor = successor;
        ResultJson = resultJson;
    }

    public Sts2Environment Successor { get; }
    public string ResultJson { get; }
    public JsonDocument Result => JsonDocument.Parse(ResultJson);

    /// <summary>
    /// Returns the known typed operation facts without exposing a borrowed
    /// JsonElement. Unknown future operation kinds remain available through
    /// <see cref="ResultJson"/> and are represented with a null typed payload.
    /// </summary>
    public Sts2OperationResult? OperationResult
    {
        get
        {
            using JsonDocument document = JsonDocument.Parse(ResultJson);
            JsonElement root = document.RootElement;
            if (!root.TryGetProperty("operation_result", out JsonElement result)
                || result.ValueKind == JsonValueKind.Null) return null;
            string? kind = result.TryGetProperty("kind", out JsonElement kindValue)
                ? kindValue.GetString() : null;
            Sts2MerchantPurchaseResult? purchase = null;
            if (kind == "merchant_purchase")
            {
                purchase = new Sts2MerchantPurchaseResult(
                    result.GetProperty("entry_index").GetInt32(),
                    result.GetProperty("success").GetBoolean(),
                    result.GetProperty("ignore_cost").GetBoolean());
            }
            return new Sts2OperationResult(kind, purchase);
        }
    }

    public string OperationStatus => ReadString("operation_status");
    public bool TraceComplete => ReadBoolean("trace_complete");

    private string ReadString(string name)
    {
        using JsonDocument document = JsonDocument.Parse(ResultJson);
        return document.RootElement.GetProperty(name).GetString()
            ?? throw new InvalidOperationException($"Preview result property '{name}' was null.");
    }

    private bool ReadBoolean(string name)
    {
        using JsonDocument document = JsonDocument.Parse(ResultJson);
        return document.RootElement.GetProperty(name).GetBoolean();
    }

    public void Dispose() => Successor.Dispose();
}

[StructLayout(LayoutKind.Sequential, Pack = 4)]
internal struct NativeAction
{
    public int Kind;
    public int Index;
    public int Target;
    public int Col;
    public int Row;

    public readonly Sts2Action ToManaged() => new(Kind, Index, Target, Col, Row);
}
