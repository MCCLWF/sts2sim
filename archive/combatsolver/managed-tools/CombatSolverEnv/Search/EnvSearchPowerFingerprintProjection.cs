using System.Collections.ObjectModel;
using CombatSolverEnv.Api;
using System.Text.Json;
using MegaCrit.Sts2.Core.Models;
using CombatSolver.Engine.InCombat.Simulation;

namespace CombatSolver;

internal sealed class EnvSearchPowerFingerprintProjection
{
    private EnvSearchPowerFingerprintProjection(ulong hash, Dictionary<int, StateFingerprint> powers, StateFingerprint aggregate)
    { PredictionHash = hash; Powers = new ReadOnlyDictionary<int, StateFingerprint>(powers); Aggregate = aggregate; }
    internal ulong PredictionHash { get; }
    internal IReadOnlyDictionary<int, StateFingerprint> Powers { get; }
    internal StateFingerprint Aggregate { get; }
    internal static EnvSearchPowerFingerprintProjection From(EnvCombatPowerQueries inputs, IReadOnlySet<(uint Owner, string Model)>? retiredSlots = null)
    {
        Dictionary<int, StateFingerprint> powers = [];
        StateFingerprintBuilder aggregate = new();
        aggregate.Add('P');
        foreach (JsonElement power in inputs.Document.GetProperty("powers").EnumerateArray())
        {
            if (power.GetProperty("amount").GetInt32() == 0) continue;
            string modelId = power.GetProperty("power_id").GetString()!;
            StateFingerprintBuilder key = new();
            key.Add(power.GetProperty("owner_combat_id").GetUInt32());
            key.Add(power.GetProperty("applier_combat_id").GetUInt32());
            key.Add(power.GetProperty("target_combat_id").GetUInt32());
            key.Add(power.GetProperty("power_id").GetString());
            key.Add(power.GetProperty("amount").GetInt32());
            key.Add(modelId is "DRAW_CARDS_NEXT_TURN_POWER" or "SUMMON_NEXT_TURN_POWER" or "HELLO_WORLD_POWER"
                ? power.GetProperty("amount_on_turn_start").GetInt32() : 0);
            if (modelId is "WEAK_POWER" or "VULNERABLE_POWER" or "FRAIL_POWER"
                && power.GetProperty("skip_next_duration_tick").GetBoolean()) key.Add('d');
            if (power.GetProperty("ritual_flag").ValueKind != JsonValueKind.Null) key.Add(power.GetProperty("ritual_flag").GetBoolean());
            if (power.GetProperty("surrounded_direction").ValueKind != JsonValueKind.Null) key.Add(power.GetProperty("surrounded_direction").GetInt32());
            int count = 0;
            ulong first = 0, second = 0;
            foreach (JsonElement variable in power.GetProperty("dynamic_vars").EnumerateArray())
            {
                if (variable.GetProperty("is_calculated").GetBoolean()) continue;
                string variableName = variable.GetProperty("name").GetString()!;
                if (variable.GetProperty("is_string").GetBoolean())
                {
                    if (IsPresentationString(modelId, variableName)) continue;
                    throw new EnvProjectionIncompleteException("power_string_var_unclassified:" + modelId + ":" + variableName);
                }
                StateFingerprintBuilder item = new();
                item.Add(variable.GetProperty("name").GetString());
                foreach (JsonElement word in variable.GetProperty("base_value").GetProperty("decimal_bits").EnumerateArray())
                    item.Add(unchecked((int)word.GetUInt32()));
                StateFingerprint fingerprint = item.Finish();
                first = unchecked(first + StateFingerprintBuilder.MixFirst(fingerprint.First));
                second = unchecked(second + StateFingerprintBuilder.MixSecond(fingerprint.Second));
                count++;
            }
            key.Add(count); key.Add(first); key.Add(second);
            StateFingerprint value = key.Finish();
            powers.Add(power.GetProperty("power_ref").GetInt32(), value);
            aggregate.Add(value.First); aggregate.Add(value.Second);
        }
        aggregate.Add(powers.Count);
        ulong retiredFirst = 0, retiredSecond = 0;
        int retiredCount = 0;
        foreach (var slot in retiredSlots ?? new HashSet<(uint Owner, string Model)>())
        {
            StateFingerprintBuilder item = new();
            item.Add(slot.Owner);
            string model = slot.Model;
            if (!EnvModelCatalog.Content.GetProperty("powers").TryGetProperty(model, out JsonElement metadata))
                throw new EnvProjectionIncompleteException("retired_power_type_unmapped:" + model);
            item.Add(metadata.GetProperty("type").GetString());
            StateFingerprint value = item.Finish();
            retiredFirst = unchecked(retiredFirst + StateFingerprintBuilder.MixFirst(value.First));
            retiredSecond = unchecked(retiredSecond + StateFingerprintBuilder.MixSecond(value.Second));
            retiredCount++;
        }
        aggregate.Add('r'); aggregate.Add(retiredCount); aggregate.Add(retiredFirst); aggregate.Add(retiredSecond);
        return new(inputs.PredictionHash, powers, aggregate.Finish());
    }
    // Pinned SemanticStateFieldPolicy belongs to the Mod adapter.
    private static bool IsPresentationString(string model, string variable) => (model, variable) switch
    {
        ("KNOCKDOWN_POWER" or "GUARDED_POWER" or "FLANKING_POWER" or "COVERED_POWER", "Applier") => true,
        ("INTERCEPT_POWER", "Covering") => true,
        ("VITAL_SPARK_POWER" or "GALVANIC_POWER", "AfflictionTitle") => true,
        ("NIGHTMARE_POWER", "Card") => true,
        ("SHRINK_POWER" or "BARRICADE_POWER", "ApplierName") => true,
        ("IMITATION_LEARNING_POWER", "TargetPlayer") => true,
        _ => false,
    };

}

internal sealed partial class SimulatedCombatState
{
    internal static StateFingerprint CaptureOriginalPowerFingerprintForEnvVerification(PowerModel power, CombatPredictionSimulator simulator)
        => GetPowerFingerprint(power, simulator);

    internal StateFingerprint CaptureOriginalPowerAggregateForEnvVerification(CombatPredictionSimulator simulator)
    {
        StateFingerprintBuilder key = new();
        key.Add('P');
        int count = 0;
        foreach (PowerModel power in EffectivePowers())
        {
            if (power.Amount == 0) continue;
            StateFingerprint value = GetPowerFingerprint(power, simulator);
            key.Add(value.First); key.Add(value.Second); count++;
        }
        key.Add(count);
        AddCreatureTypeSet(ref key, 'r', _retiredRootPowerSlots);
        return key.Finish();
    }
}
