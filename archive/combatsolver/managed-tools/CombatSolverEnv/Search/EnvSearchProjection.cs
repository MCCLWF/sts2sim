using System.Collections.ObjectModel;
using System.Text.Json;
using CombatSolverEnv.Api;

namespace CombatSolver;

internal readonly record struct EnvSearchCard(
    int Ref, int Id, int Upgrade, int Owner, int Pile,
    bool Removed, bool ExhaustOnNextPlay, int Enchantment, int Affliction);

internal readonly record struct EnvSearchCreature(
    int Ref, int CombatId, int Side, int Hp, int MaxHp, int Block,
    bool Attached, bool Player, string MonsterId);

internal readonly record struct EnvSearchRng(string Name, EnvRngState State);

/// <summary>
/// Immutable read boundary for the original Beam's future Env adapter.
/// Values are copied from the owned prediction envelope; no lazy native/game
/// object is retained. This is deliberately a partial projection until every
/// model-specific fingerprint and policy ledger is represented.
/// </summary>
internal sealed class EnvSearchProjection
{
    private EnvSearchProjection(
        ulong predictionHash, ulong stateHash, bool terminal, int turn, int phase,
        int energy, int stars, EnvSearchCreature player,
        IReadOnlyDictionary<int, EnvSearchCard> cards,
        IReadOnlyList<IReadOnlyList<int>> piles,
        IReadOnlyList<EnvSearchCreature> enemies,
        IReadOnlyList<EnvSearchRng> rng,
        int historyCount, int[] historyCounters)
    {
        PredictionHash = predictionHash; StateHash = stateHash; Terminal = terminal;
        Turn = turn; Phase = phase; Energy = energy; Stars = stars; Player = player;
        Cards = cards; Piles = piles; Enemies = enemies; Rng = rng;
        HistoryCount = historyCount; HistoryCounters = new ReadOnlyCollection<int>(historyCounters);
    }

    internal ulong PredictionHash { get; }
    internal ulong StateHash { get; }
    internal bool Terminal { get; }
    internal int Turn { get; }
    internal int Phase { get; }
    internal int Energy { get; }
    internal int Stars { get; }
    internal EnvSearchCreature Player { get; }
    internal IReadOnlyDictionary<int, EnvSearchCard> Cards { get; }
    internal IReadOnlyList<IReadOnlyList<int>> Piles { get; }
    internal IReadOnlyList<EnvSearchCreature> Enemies { get; }
    internal IReadOnlyList<EnvSearchRng> Rng { get; }
    internal int HistoryCount { get; }
    internal IReadOnlyList<int> HistoryCounters { get; }

    // Original BuildStateKey also needs typed model fingerprints, dynamic vars,
    // orbs, policy risk entries and strategy ledgers. Do not silently substitute
    // the Env hash or empty values for those fields.
    internal bool OriginalStateKeyReady => false;
    internal void RequireOriginalStateKeyReady()
        => throw new EnvProjectionIncompleteException(
            "env_search_projection_missing:card_model_fingerprint_or_policy_ledger");

    internal static EnvSearchProjection From(EnvCombatPredictionSnapshot snapshot, EnvSearchHistoryProjection? projectedHistory = null)
    {
        JsonElement document = snapshot.Document;
        JsonElement inner = document.GetProperty("inner_snapshot");
        JsonElement state = inner.GetProperty("state");
        JsonElement playerCombat = state.GetProperty("player_combat");
        int turn = playerCombat.GetProperty("turn_number").GetInt32();
        int phase = playerCombat.GetProperty("phase").GetInt32();
        int energy = playerCombat.GetProperty("energy").GetInt32();
        int stars = playerCombat.GetProperty("stars").GetInt32();

        Dictionary<int, EnvSearchCard> cards = new();
        foreach (JsonElement slot in state.GetProperty("cards").GetProperty("slots").EnumerateArray())
        {
            JsonElement value = slot.GetProperty("value");
            int reference = slot.GetProperty("index").GetInt32();
            cards.Add(reference, new EnvSearchCard(reference, value.GetProperty("id").GetInt32(),
                value.GetProperty("upgrade").GetInt32(), value.GetProperty("owner").GetInt32(),
                value.GetProperty("pile").GetInt32(), value.GetProperty("removed").GetBoolean(),
                value.GetProperty("exhaust_on_next_play").GetBoolean(), value.GetProperty("enchantment").GetInt32(),
                value.GetProperty("affliction").GetInt32()));
        }
        int[] pileRefs(JsonElement pile) => pile.EnumerateArray().Select(value => value.GetInt32()).ToArray();
        IReadOnlyList<IReadOnlyList<int>> piles = new ReadOnlyCollection<IReadOnlyList<int>>(
            playerCombat.GetProperty("piles").EnumerateArray()
                .Select(pile => (IReadOnlyList<int>)new ReadOnlyCollection<int>(pileRefs(pile))).ToArray());

        EnvSearchCreature ReadCreature(JsonElement slot)
        {
            JsonElement value = slot.GetProperty("value");
            JsonElement monster = value.GetProperty("monster");
            return new EnvSearchCreature(slot.GetProperty("index").GetInt32(), value.GetProperty("combat_id").GetInt32(),
                value.GetProperty("side").GetInt32(), value.GetProperty("current_hp").GetInt32(),
                value.GetProperty("max_hp").GetInt32(), value.GetProperty("block").GetInt32(),
                value.GetProperty("attached").GetBoolean(), value.GetProperty("player").GetInt32() != 0,
                monster.GetProperty("id").GetInt32().ToString());
        }
        EnvSearchCreature[] creatures = state.GetProperty("creatures").GetProperty("slots")
            .EnumerateArray().Where(slot => slot.GetProperty("value").GetProperty("combat_id").GetInt32() != 0)
            .Select(ReadCreature).ToArray();
        JsonElement playerSlot = state.GetProperty("creatures").GetProperty("slots").EnumerateArray()
            .Single(slot => slot.GetProperty("value").GetProperty("player").GetInt32() != 0);
        EnvSearchCreature player = ReadCreature(playerSlot);
        EnvSearchCreature[] enemies = creatures.Where(value => !value.Player && value.Side == 2).ToArray();

        EnvStateSnapshot innerSnapshot = EnvStateSnapshot.Parse(inner.GetRawText());
        EnvSearchRng[] rng = Enum.GetValues<EnvRunRngStream>()
            .Where(value => value != EnvRunRngStream.UpFront && value != EnvRunRngStream.UnknownMapPoint
                && value != EnvRunRngStream.TreasureRoomRelics)
            .Select(value => new EnvSearchRng(value.ToString(), innerSnapshot.GetRunRng(value))).ToArray();
        JsonElement history = (projectedHistory ?? EnvSearchHistoryProjection.From(document)).Document;
        int[] counters = history.GetProperty("counters").EnumerateArray().Select(value => value.GetInt32()).ToArray();
        if (counters.Length != 6) throw new EnvProjectionIncompleteException("env_search_projection_history_counter_shape");
        return new(snapshot.PredictionHash, inner.GetProperty("state_hash").GetUInt64(),
            document.GetProperty("terminal").ValueKind != JsonValueKind.Null, turn, phase, energy, stars,
            player, new ReadOnlyDictionary<int, EnvSearchCard>(cards), piles,
            new ReadOnlyCollection<EnvSearchCreature>(enemies), new ReadOnlyCollection<EnvSearchRng>(rng),
            history.GetProperty("events").GetArrayLength(), counters);
    }
}

internal sealed class EnvProjectionIncompleteException(string message) : InvalidOperationException(message);
