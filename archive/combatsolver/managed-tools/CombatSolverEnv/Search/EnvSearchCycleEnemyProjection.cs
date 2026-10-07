using System.Collections.Frozen;
using System.Text.Json;
using CombatSolverEnv.Api;

namespace CombatSolver;

// The original cycle mask follows KnownEnemies, which starts in root combat
// order and can later retain dead members. Until spawn/escape and revival
// ledgers are mapped, only a stable root roster is accepted here.
internal sealed class EnvSearchCycleEnemyProjection
{
    private static readonly FrozenDictionary<int, string> MonsterTypes =
        EnvModelCatalog.Content.GetProperty("monsters").EnumerateObject()
            .ToFrozenDictionary(model => model.Value.GetProperty("id").GetInt32(),
                model => model.Value.GetProperty("type").GetString()!);

    private readonly (int Reference, int CombatId, int MonsterId)[] _known;
    private readonly HashSet<int> _rootEnemyPoolRefs;
    private readonly int[] _rootEscapedRefs;

    private EnvSearchCycleEnemyProjection(
        (int Reference, int CombatId, int MonsterId)[] known,
        HashSet<int> rootEnemyPoolRefs, int[] rootEscapedRefs)
    { _known = known; _rootEnemyPoolRefs = rootEnemyPoolRefs; _rootEscapedRefs = rootEscapedRefs; }

    internal static EnvSearchCycleEnemyProjection CaptureRoot(EnvStateSnapshot root)
    {
        JsonElement state = root.State;
        Dictionary<int, JsonElement> creatures = CreatureSlots(state);
        int[] active = state.GetProperty("combat").GetProperty("enemies")
            .EnumerateArray().Select(value => value.GetInt32()).ToArray();
        if (active.Length > 64 || active.Distinct().Count() != active.Length)
            throw new EnvProjectionIncompleteException("env_cycle_enemy_root_roster_unmapped");
        var known = new (int Reference, int CombatId, int MonsterId)[active.Length];
        for (int index = 0; index < active.Length; index++)
        {
            if (!creatures.TryGetValue(active[index], out JsonElement creature)
                || creature.GetProperty("side").GetInt32() != 2)
                throw new EnvProjectionIncompleteException("env_cycle_enemy_root_identity_unmapped");
            int combatId = creature.GetProperty("combat_id").GetInt32();
            int monsterId = creature.GetProperty("monster").GetProperty("id").GetInt32();
            if (combatId <= 0 || !MonsterTypes.TryGetValue(monsterId, out string? type)
                || type.EndsWith(".WaterfallGiant", StringComparison.Ordinal))
                throw new EnvProjectionIncompleteException("env_cycle_enemy_root_model_unmapped");
            known[index] = (active[index], combatId, monsterId);
        }
        if (known.Select(enemy => enemy.CombatId).Distinct().Count() != known.Length)
            throw new EnvProjectionIncompleteException("env_cycle_enemy_root_combat_id_duplicate");
        HashSet<int> poolRefs = creatures.Where(item => item.Value.GetProperty("side").GetInt32() == 2)
            .Select(item => item.Key).ToHashSet();
        int[] escaped = state.GetProperty("combat").GetProperty("escaped")
            .EnumerateArray().Select(value => value.GetInt32()).ToArray();
        return new(known, poolRefs, escaped);
    }

    internal ulong CaptureAliveMask(EnvCombatPredictionSnapshot snapshot)
    {
        JsonElement state = snapshot.Document.GetProperty("inner_snapshot").GetProperty("state");
        JsonElement combat = state.GetProperty("combat");
        int[] escaped = combat.GetProperty("escaped").EnumerateArray().Select(value => value.GetInt32()).ToArray();
        if (!escaped.SequenceEqual(_rootEscapedRefs))
            throw new EnvProjectionIncompleteException("env_cycle_enemy_escape_unmapped");
        Dictionary<int, JsonElement> creatures = CreatureSlots(state);
        HashSet<int> poolRefs = creatures.Where(item => item.Value.GetProperty("side").GetInt32() == 2)
            .Select(item => item.Key).ToHashSet();
        if (!poolRefs.SetEquals(_rootEnemyPoolRefs))
            throw new EnvProjectionIncompleteException("env_cycle_enemy_spawn_or_pool_reuse_unmapped");
        HashSet<int> active = combat.GetProperty("enemies").EnumerateArray()
            .Select(value => value.GetInt32()).ToHashSet();
        if (active.Count != combat.GetProperty("enemies").GetArrayLength()
            || active.Except(_known.Select(enemy => enemy.Reference)).Any())
            throw new EnvProjectionIncompleteException("env_cycle_enemy_active_roster_unmapped");
        ulong mask = 0;
        for (int index = 0; index < _known.Length; index++)
        {
            (int reference, int combatId, int monsterId) = _known[index];
            if (!creatures.TryGetValue(reference, out JsonElement creature)
                || creature.GetProperty("combat_id").GetInt32() != combatId
                || creature.GetProperty("monster").GetProperty("id").GetInt32() != monsterId)
                throw new EnvProjectionIncompleteException("env_cycle_enemy_identity_changed");
            if (!active.Contains(reference)) continue;
            // EffectiveEnemyHp may keep a zero-HP monster reviving. The
            // generic snapshot lacks the original search's death-phase ledger.
            if (creature.GetProperty("current_hp").GetInt32() <= 0)
                throw new EnvProjectionIncompleteException("env_cycle_enemy_effective_hp_unmapped");
            mask |= 1UL << index;
        }
        return mask;
    }

    private static Dictionary<int, JsonElement> CreatureSlots(JsonElement state)
        => state.GetProperty("creatures").GetProperty("slots").EnumerateArray()
            .ToDictionary(slot => slot.GetProperty("index").GetInt32(),
                slot => slot.GetProperty("value"));
}
