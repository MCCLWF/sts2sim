using System.Text.Json;
using CombatSolverEnv.Api;
using Sts2EnvCompare;

namespace CombatSolver;

// Preserve the original PlanAction identity until the execution boundary.
// Vanilla passes null for a Self card's target, while CombatSolver retains
// the player's combat ID in its policy plan and history.
internal static class EnvSearchActionMapper
{
    internal static EnvCombatAction Map(PlanAction planned, uint? playerCombatId,
        EnvCombatPredictionSession source)
    {
        NativePlanAction identity = new(planned.Kind.ToString(), planned.CardId, planned.CardOccurrence,
            planned.TargetCombatId, planned.PotionId, planned.PotionSlot,
            planned.CardStateKey, planned.CardStateOccurrence,
            planned.Choice != null || planned.NestedChoices is { Count: > 0 }
                || planned.TurnStartChoices is { Count: > 0 });
        using JsonDocument inspection = JsonDocument.Parse(source.InspectJson());
        if (!inspection.RootElement.TryGetProperty("root", out JsonElement root))
            throw new InvalidOperationException("combat_prediction_inspection_has_no_root");
        EnvAction mapped = NativeEnv.NativePlanToEnvAction(identity, root);
        EnvCombatAction action = new((EnvCombatActionKind)mapped.Kind, mapped.Index, mapped.Target);
        if (action.Kind == EnvCombatActionKind.PlayCard
            && planned.TargetCombatId is { } target && target == playerCombatId)
        {
            IReadOnlyList<EnvCombatAction> legal = source.LegalActions();
            if (!legal.Contains(action))
            {
                EnvCombatAction vanillaSelf = action with { TargetCombatId = -1 };
                if (!legal.Contains(vanillaSelf))
                    throw new InvalidOperationException("self_target_has_no_matching_env_action");
                return vanillaSelf;
            }
        }
        return action;
    }
}
