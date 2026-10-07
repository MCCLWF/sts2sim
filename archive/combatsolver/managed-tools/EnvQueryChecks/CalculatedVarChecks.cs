using System.Text.Json;
using CombatSolver;
using CombatSolver.Engine.Common;
using CombatSolver.Engine.InCombat.Simulation;
using CombatSolverEnv.Api;
using MegaCrit.Sts2.Core.Combat;
using MegaCrit.Sts2.Core.Entities.Cards;
using MegaCrit.Sts2.Core.Entities.Creatures;
using MegaCrit.Sts2.Core.Entities.Players;
using MegaCrit.Sts2.Core.Localization.DynamicVars;

internal static class CalculatedVarChecks
{
    internal static void CheckBindingRejection(Player player, CombatState combat)
    {
        var body = player.PlayerCombatState!.AllCards.Single(card => card.Id.Entry == "BODY_SLAM");
        CalculatedVar variable = body.DynamicVars.CalculatedDamage;
        var binding = variable._multiplierCalc;
        var owner = variable._owner;
        void Reject(string reason)
        {
            try
            {
                EnvLiveCards.Capture(combat, player, combat.Creatures.ToArray(), new Dictionary<MegaCrit.Sts2.Core.Models.CardModel, int>());
                throw new InvalidOperationException("unverified calculated binding accepted");
            }
            catch (InvalidOperationException error) when (error.Message.Contains(reason, StringComparison.Ordinal)) { }
        }
        try
        {
            variable._multiplierCalc = static (_, _) => 42m;
            Reject("live_card_calculated_binding_mismatch:BODY_SLAM");
            variable._multiplierCalc = binding;
            variable._owner = null;
            Reject("live_card_calculated_formula_unmapped:BODY_SLAM");
        }
        finally { variable._multiplierCalc = binding; variable._owner = owner; }
        Console.WriteLine("calculated live binding checks: changed delegate and owner rejected; original fields restored");
    }
    internal static void Compare(Player player, CombatState combat, EnvLiveCards captured,
        CombatPredictionSimulator search, EnvCombatPredictionSession env, string name, string output)
    {
        ulong hash = env.StateHash;
        string before = env.CaptureSnapshot().Document.GetRawText();
        EnvCombatCalculatedVars actual = env.CaptureCalculatedVars();
        object Bits(decimal value) => new { decimal_bits = decimal.GetBits(value).Select(word => unchecked((uint)word)).ToArray() };
        Creature?[] targets = [null, .. combat.Creatures];
        int rows = 0, values = 0;
        object[] cards = player.PlayerCombatState!.AllPiles.SelectMany((pile, pileIndex) => pile.Cards.SelectMany((card, index) =>
            card.DynamicVars.Values.OfType<CalculatedVar>().Select(variable =>
            {
                PredictedCard predicted = search.State.FindCard(card) ?? throw new InvalidOperationException("calculated_native_card_missing");
                int reference = captured.Component.GetProperty("piles")[pileIndex][index].GetInt32();
                CalculatedVar predictedVar = (CalculatedVar)predicted.Preview.DynamicVars[variable.Name];
                decimal calculationBase = card.DynamicVars.CalculationBase.BaseValue;
                decimal calculationExtra = variable is CalculatedDamageVar
                    ? card.DynamicVars.ExtraDamage.BaseValue
                    : card.DynamicVars.CalculationExtra.BaseValue;
                rows++;
                return (object)new { card_ref = reference, card_id = card.Id.Entry, pile_index = pileIndex, index,
                    variable = variable.Name, type = variable switch { CalculatedBlockVar => 1, CalculatedDamageVar => 2, _ => 3 },
                    base_value = Bits(variable.BaseValue), calculation_base = Bits(calculationBase),
                    calculation_extra = Bits(calculationExtra), values = targets.Select(target =>
                    {
                        int targetReference = target is null ? 0 : combat.Creatures.ToList().IndexOf(target) + 1;
                        decimal game = variable.Calculate(target);
                        decimal rawMultiplier = variable._multiplierCalc!(card, target);
                        if (!CalculatedVarSpecRegistry.TryCalculate(predictedVar, search, predicted, target, out decimal prediction))
                            throw new InvalidOperationException("original_calculated_spec_missing:" + card.Id.Entry);
                        if (!decimal.GetBits(game).SequenceEqual(decimal.GetBits(actual.GetValue(reference, variable.Name,
                            targetReference, EnvCalculatedValueSemantics.Game)))
                            || !decimal.GetBits(prediction).SequenceEqual(decimal.GetBits(actual.GetValue(reference, variable.Name,
                            targetReference, EnvCalculatedValueSemantics.OriginalPrediction))))
                            throw new InvalidOperationException($"calculated_value_mismatch:{name}:{card.Id.Entry}:{variable.Name}:{targetReference}");
                        values++;
                        return new { target_ref = targetReference, game_value = Bits(game), raw_multiplier = Bits(rawMultiplier) };
                    }).ToArray() };
            }))).ToArray();
        JsonElement expected = JsonSerializer.SerializeToElement(new {
            format = "sts2env.combat_calculated_vars", version = 2, prediction_hash = hash,
            query_semantics = "game", cards });
        if (!JsonElement.DeepEquals(expected, actual.Document) || env.StateHash != hash
            || env.CaptureSnapshot().Document.GetRawText() != before
            || !JsonElement.DeepEquals(actual.Document, env.CaptureCalculatedVars().Document))
            throw new InvalidOperationException("calculated_document_mismatch_or_mutated_source:" + name);
        using EnvCombatPredictionSession sibling = env.Clone();
        if (!JsonElement.DeepEquals(actual.Document, sibling.CaptureCalculatedVars().Document))
            throw new InvalidOperationException("calculated_clone_mismatch:" + name);
        File.WriteAllText(Path.Combine(output, name + ".native-calculated.json"), expected.GetRawText());
        File.WriteAllText(Path.Combine(output, name + ".public-calculated.json"), actual.Document.GetRawText());
        Console.WriteLine($"calculated vars {name}: cards={rows}; target_values={values}; game_and_original_prediction_exact_bits=true; policy_complete=false");
    }
}
