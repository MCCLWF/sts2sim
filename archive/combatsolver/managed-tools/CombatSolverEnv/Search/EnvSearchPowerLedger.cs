using System.Collections.Frozen;
using System.Text.Json;
using CombatSolverEnv.Api;

namespace CombatSolver;

// Original search metadata, owned entirely by the C# adapter. Branches share
// immutable root identities and derive their own retired set from Env facts.
internal sealed class EnvSearchPowerLedger
{
    private static readonly FrozenDictionary<int, string> PowerEntries = EnvModelCatalog.Content.GetProperty("powers")
        .EnumerateObject().ToFrozenDictionary(model => model.Value.GetProperty("id").GetInt32(), model => model.Name);
    private readonly FrozenSet<(uint Owner, string Model)> _singleRootSlots;
    private EnvSearchPowerLedger(FrozenSet<(uint Owner, string Model)> slots) => _singleRootSlots = slots;
    internal static EnvSearchPowerLedger CaptureRoot(EnvCombatPowerQueries root)
        => new(root.Document.GetProperty("powers").EnumerateArray()
            .GroupBy(power => (power.GetProperty("owner_combat_id").GetUInt32(), power.GetProperty("power_id").GetString()!))
            .Where(group => group.Count() == 1).Select(group => group.Key).ToFrozenSet());

    internal IReadOnlySet<(uint Owner, string Model)> Retired(EnvCombatPredictionSnapshot snapshot)
    {
        HashSet<(uint Owner, string Model)> result = [];
        foreach (JsonElement fact in snapshot.Document.GetProperty("power_events").EnumerateArray())
        {
            int kind = fact[0].GetInt32();
            // The original listener ledger retires an attached nonzero root
            // on zeroing or removal; creation before attachment is excluded.
            if (!fact[7].GetBoolean() || fact[5].GetInt32() == 0
                || !(kind == 0 && fact[6].GetInt32() == 0 || kind == 2)) continue;
            int id = fact[4].GetInt32();
            if (!PowerEntries.TryGetValue(id, out string? model))
                throw new EnvProjectionIncompleteException("power_event_model_unmapped:" + id);
            var slot = (fact[3].GetUInt32(), model);
            if (_singleRootSlots.Contains(slot)) result.Add(slot);
        }
        return result.ToFrozenSet();
    }
}
