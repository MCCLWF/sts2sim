using System.Collections.ObjectModel;
using System.Text.Json;
using CombatSolverEnv.Api;

namespace CombatSolver;

internal readonly record struct EnvThreatIntent(
    int Type, bool IsAttack, bool Multi, int Repeats, bool Strong, int StatusCount,
    IReadOnlyList<ulong> DamageDecimalBits);

internal readonly record struct EnvThreatCreature(
    int CreatureRef, int CombatId, bool Alive, string ModelId, string NextMoveId,
    bool Stunned, bool IntendsToAttack, IReadOnlyList<EnvThreatIntent> Intents);

internal sealed class EnvSearchThreatProjection
{
    private EnvSearchThreatProjection(ulong predictionHash, IReadOnlyList<EnvThreatCreature> creatures)
    {
        PredictionHash = predictionHash; Creatures = creatures;
    }
    internal ulong PredictionHash { get; }
    internal IReadOnlyList<EnvThreatCreature> Creatures { get; }
    internal bool Complete => false;
    internal void RequireComplete()
        => throw new EnvProjectionIncompleteException("env_search_threat_projection_missing:hooked_damage_and_threat_policy");

    internal static EnvSearchThreatProjection From(EnvCombatGameQueries queries)
    {
        List<EnvThreatCreature> creatures = new();
        foreach (JsonElement item in queries.Document.GetProperty("roster").EnumerateArray())
        {
            JsonElement monster = item.GetProperty("monster");
            if (monster.ValueKind == JsonValueKind.Null) continue;
            List<EnvThreatIntent> intents = new();
            foreach (JsonElement intent in monster.GetProperty("intents").EnumerateArray())
            {
                List<ulong> bits = intent.GetProperty("damage_raw").ValueKind == JsonValueKind.Null
                    ? [] : intent.GetProperty("damage_raw").GetProperty("decimal_bits")
                        .EnumerateArray().Select(value => value.GetUInt64()).ToList();
                intents.Add(new EnvThreatIntent(intent.GetProperty("type").GetInt32(),
                    intent.GetProperty("is_attack").GetBoolean(), intent.GetProperty("multi").GetBoolean(),
                    intent.GetProperty("repeats").GetInt32(), intent.GetProperty("strong").GetBoolean(),
                    intent.GetProperty("status_count").GetInt32(), new ReadOnlyCollection<ulong>(bits)));
            }
            creatures.Add(new EnvThreatCreature(item.GetProperty("creature_ref").GetInt32(),
                item.GetProperty("combat_id").GetInt32(), item.GetProperty("is_alive").GetBoolean(),
                monster.GetProperty("entry").GetString()!, monster.GetProperty("next_move_id").GetString()!,
                monster.GetProperty("stunned").GetBoolean(), monster.GetProperty("intends_to_attack").GetBoolean(),
                new ReadOnlyCollection<EnvThreatIntent>(intents)));
        }
        return new(queries.PredictionHash, new ReadOnlyCollection<EnvThreatCreature>(creatures));
    }
}
