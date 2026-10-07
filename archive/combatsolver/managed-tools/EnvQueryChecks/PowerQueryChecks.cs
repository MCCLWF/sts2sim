extern alias Derived;
using System.Reflection;
using System.Text.Json;
using CombatSolver;
using CombatSolver.Engine.InCombat.Simulation;
using CombatSolverEnv.Api;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Models;
using MegaCrit.Sts2.Core.Models.Powers;
using MegaCrit.Sts2.Core.Localization.DynamicVars;

internal static class PowerQueryChecks
{
    internal static void Compare(CombatState combat, EnvLiveCreatures captured,
        CombatPredictionSimulator native, EnvCombatPredictionSession env, string name, string output)
    {
        ulong hash = env.StateHash;
        string before = env.CaptureSnapshot().Document.GetRawText();
        EnvCombatPowerQueries actual = env.CapturePowerQueries();
        var projected = Derived::CombatSolver.EnvSearchPowerFingerprintProjection.From(
            Derived::CombatSolverEnv.Api.EnvCombatPowerQueries.Parse(actual.Document.GetRawText(), hash));
        MethodInfo capture = typeof(SimulatedCombatState).GetMethod("GetPowerFingerprint", BindingFlags.Static | BindingFlags.Public | BindingFlags.NonPublic)!;
        var original = (SimulatedCombatState)native.State.CombatState;
        Creature[] roster = combat.Creatures.ToArray();
        var slots = captured.Component.GetProperty("powers").GetProperty("slots").EnumerateArray().ToList();
        object Bits(decimal value) => new { decimal_bits = decimal.GetBits(value).Select(word => unchecked((uint)word)).ToArray() };
        uint RefId(int reference) => reference == 0 ? uint.MaxValue : roster[reference - 1].CombatId ?? uint.MaxValue;
        object[] powers = combat.IterateHookListeners().OfType<PowerModel>().Select(power =>
        {
            int id = EnvModelCatalog.Id(power, "powers");
            int index = slots.FindIndex(slot => slot.GetProperty("value").GetProperty("id").GetInt32() == id
                && RefId(slot.GetProperty("value").GetProperty("owner").GetInt32()) == (power.Owner.CombatId ?? uint.MaxValue));
            if (index < 0) throw new InvalidOperationException("native_power_instance_missing:" + power.Id.Entry);
            int reference = slots[index].GetProperty("index").GetInt32();
            slots.RemoveAt(index);
            StateFingerprint expectedKey = (StateFingerprint)capture.Invoke(null, [power, native])!;
            if (power.Amount != 0 && projected.Powers[reference] != expectedKey) throw new InvalidOperationException("original_power_fingerprint_mismatch:" + name + ":" + power.Id.Entry);
            var metadata = EnvModelCatalog.Content.GetProperty("powers").GetProperty(power.Id.Entry);
            return (object)new {
                power_ref = reference, power_id = power.Id.Entry,
                owner_combat_id = power.Owner.CombatId ?? uint.MaxValue,
                applier_combat_id = power.Applier?.CombatId ?? uint.MaxValue,
                target_combat_id = power.Target?.CombatId ?? uint.MaxValue,
                amount = power.Amount,
                amount_on_turn_start = power.AmountOnTurnStart,
                skip_next_duration_tick = power.SkipNextDurationTick,
                ritual_flag = power is RitualPower ritual ? (bool?)ritual._wasJustAppliedByEnemy : null,
                surrounded_direction = power is SurroundedPower surrounded ? (int?)PowerPredictionStateSupport.SurroundedFacing(native, surrounded) : null,
                dynamic_vars = metadata.GetProperty("variables").EnumerateArray().Select(definition => definition.GetProperty("name").GetString()!)
                    .Select(variable => new { name = variable, is_calculated = power.DynamicVars[variable] is CalculatedVar,
                        is_string = power.DynamicVars[variable] is StringVar, base_value = Bits(power.DynamicVars[variable].BaseValue) }).ToArray()
            };
        }).ToArray();
        JsonElement expected = JsonSerializer.SerializeToElement(new {
            format = "sts2env.combat_power_queries", version = 1, prediction_hash = hash,
            query_semantics = "game", powers });
        if (projected.Aggregate != original.CaptureOriginalPowerAggregateForEnvVerification(native))
            throw new InvalidOperationException("original_power_root_aggregate_mismatch:" + name);
        if (!JsonElement.DeepEquals(actual.Document, expected) || env.StateHash != hash
            || env.CaptureSnapshot().Document.GetRawText() != before)
            throw new InvalidOperationException("power_inputs_or_purity_mismatch:" + name);
        using EnvCombatPredictionSession sibling = env.Clone();
        if (!JsonElement.DeepEquals(actual.Document, sibling.CapturePowerQueries().Document)
            || !JsonElement.DeepEquals(actual.Document, env.CapturePowerQueries().Document))
            throw new InvalidOperationException("power_query_clone_or_purity_mismatch:" + name);
        File.WriteAllText(Path.Combine(output, name + ".native-powers.json"), expected.GetRawText());
        File.WriteAllText(Path.Combine(output, name + ".public-powers.json"), actual.Document.GetRawText());
        Console.WriteLine($"original power fingerprints {name}: powers={powers.Length}; actual_original_encoder=true; full_key=false");
    }
}
