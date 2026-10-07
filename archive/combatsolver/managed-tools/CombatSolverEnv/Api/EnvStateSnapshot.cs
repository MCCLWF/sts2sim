using System.Text.Json;

namespace CombatSolverEnv.Api;

// Declaration order follows the v0.111.0 RunRngType and core/rng.h.
public enum EnvRunRngStream
{
    UpFront, Shuffle, UnknownMapPoint, CombatCardGeneration, CombatPotionGeneration,
    CombatCardSelection, CombatEnergyCosts, CombatTargets, MonsterAi, Niche, CombatOrbs, TreasureRoomRelics
}

public readonly record struct EnvRngState(int Counter, ulong State0, ulong State1, ulong State2, ulong State3);

public enum EnvPlayerRngStream { Rewards, Shops, Transformations }

/// <summary>
/// Immutable complete logical state inspection. Owns its JSON independently of
/// the source session, so it survives source disposal and future predictions.
/// Pool slot identities/allocation cursors, HashSet free links, decimal words,
/// IEEE float bits and choice replay base are retained. Monster machine/callback
/// registry indices belong to the loaded native runtime. This is not a portable
/// game save. The live-game field mapper is separate from this transport.
/// </summary>
public sealed class EnvStateSnapshot
{
    public const int FormatVersion = 2;
    private static readonly string[] RunStreamNames = [
        "up_front", "shuffle", "unknown_map_point", "combat_card_generation", "combat_potion_generation",
        "combat_card_selection", "combat_energy_costs", "combat_targets", "monster_ai", "niche",
        "combat_orbs", "treasure_room_relics" ];
    public JsonElement Document { get; }
    public JsonElement State => Document.GetProperty("state");
    public string SchemaSha256 { get; }
    public uint ModelIdHash { get; }
    public ulong StateHash { get; }
    public int Decision { get; }
    public string RuntimeId { get; }
    /// <summary>Choice request at this boundary, or null at a normal decision.</summary>
    public EnvChoiceRequest? PendingChoice
        => Document.GetProperty("choice").ValueKind == JsonValueKind.Null
            ? null : EnvChoiceRequest.Parse(Document.GetProperty("choice"), "Env snapshot");
    /// <summary>
    /// Owned answer prefix for a pending choice. The returned array is newly
    /// allocated and remains valid after the source session is disposed.
    /// </summary>
    public IReadOnlyList<int> AnswerPrefix
        => Document.GetProperty("answers").EnumerateArray().Select(value => value.GetInt32()).ToArray();

    private EnvStateSnapshot(JsonElement document)
    {
        if (document.GetProperty("format").GetString() != "sts2env.logical_state"
            || document.GetProperty("version").GetInt32() != FormatVersion
            || document.GetProperty("registry_scope").GetString() != "loaded_native_runtime")
            throw new InvalidOperationException("Unsupported Env snapshot format or registry scope");
        SchemaSha256 = document.GetProperty("schema_sha256").GetString() ?? "";
        if (SchemaSha256.Length != 64 || SchemaSha256.Any(c => !Uri.IsHexDigit(c)))
            throw new InvalidOperationException("Invalid Env snapshot schema fingerprint");
        ModelIdHash = document.GetProperty("model_id_hash").GetUInt32();
        RuntimeId = document.GetProperty("runtime_id").GetString() ?? "";
        if (string.IsNullOrEmpty(RuntimeId)) throw new InvalidOperationException("Env snapshot runtime identity missing");
        StateHash = document.GetProperty("state_hash").GetUInt64();
        Decision = document.GetProperty("decision").GetInt32();
        if (Decision is < 0 or > 8 || document.GetProperty("state").ValueKind != JsonValueKind.Object)
            throw new InvalidOperationException("Invalid Env snapshot decision or state");
        bool choice = document.GetProperty("choice").ValueKind != JsonValueKind.Null;
        if (choice != (Decision == 7)
            || choice != (document.GetProperty("replay_base").ValueKind == JsonValueKind.Object)
            || choice != (document.GetProperty("pending").ValueKind == JsonValueKind.Object))
            throw new InvalidOperationException("Incomplete Env snapshot choice replay state");
        if (document.GetProperty("answers").ValueKind != JsonValueKind.Array)
            throw new InvalidOperationException("Invalid Env snapshot answer prefix");
        if (!document.GetProperty("run_rng_streams").EnumerateArray().Select(name => name.GetString())
            .SequenceEqual(RunStreamNames)
            || !document.GetProperty("player_rng_streams").EnumerateArray().Select(name => name.GetString())
                .SequenceEqual(new[] { "rewards", "shops", "transformations" }))
            throw new InvalidOperationException("Unsupported Env snapshot RNG stream ordering");
        Document = document;
    }

    internal static EnvStateSnapshot Parse(string json)
    {
        using JsonDocument document = JsonDocument.Parse(json);
        return new EnvStateSnapshot(document.RootElement.Clone());
    }

    public EnvRngState GetRunRng(EnvRunRngStream stream)
    {
        if (!Enum.IsDefined(stream)) throw new ArgumentOutOfRangeException(nameof(stream));
        JsonElement streams = State.GetProperty("run").GetProperty("rng").GetProperty("rngs");
        if (streams.GetArrayLength() != 12)
            throw new InvalidOperationException("Unsupported Env run RNG stream schema");
        JsonElement rng = streams[(int)stream];
        return new EnvRngState(rng.GetProperty("counter").GetInt32(),
            rng.GetProperty("s0").GetUInt64(), rng.GetProperty("s1").GetUInt64(),
            rng.GetProperty("s2").GetUInt64(), rng.GetProperty("s3").GetUInt64());
    }

    public EnvRngState GetPlayerRng(EnvPlayerRngStream stream)
    {
        if (!Enum.IsDefined(stream)) throw new ArgumentOutOfRangeException(nameof(stream));
        JsonElement streams = State.GetProperty("run").GetProperty("player").GetProperty("rng").GetProperty("rngs");
        if (streams.GetArrayLength() != 3) throw new InvalidOperationException("Unsupported Env player RNG stream schema");
        JsonElement rng = streams[(int)stream];
        return new EnvRngState(rng.GetProperty("counter").GetInt32(),
            rng.GetProperty("s0").GetUInt64(), rng.GetProperty("s1").GetUInt64(),
            rng.GetProperty("s2").GetUInt64(), rng.GetProperty("s3").GetUInt64());
    }
}
