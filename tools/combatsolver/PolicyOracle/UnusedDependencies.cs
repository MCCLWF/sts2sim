// Compile-only shims for dependencies outside this pure-value differential.
// They contain no policy logic. Reaching one fails rather than yielding a dummy value.
using MegaCrit.Sts2.Core.Models;

namespace CombatSolver {
    internal sealed class ContextualRankingModel;
    internal sealed record SearchPolicySnapshot(SolverSearchProfile Profile, int? BudgetOverrideMilliseconds);
    internal readonly record struct SearchRequestWorkSnapshot(long ExpandedNodes);
    internal sealed class SearchRequestWorkTotals {
        public SearchRequestWorkSnapshot Snapshot() => throw new NotSupportedException("ledger ownership not in pure policy probe");
    }
    internal sealed class PotionValuationRegistry {
        public static PotionValuationRegistry Default { get; } = new();
        public int StrategicHpCost(PotionModel potion, bool renewable) => throw new NotSupportedException("model valuation not in pure policy probe");
        public bool RequiresOpeningUse(PotionModel potion) => throw new NotSupportedException("model valuation not in pure policy probe");
    }
    internal sealed class SimulatedCombatState {
        public int OutstandingStolenResource(CombatSolver.Engine.InCombat.Simulation.CombatPredictionSimulator simulator)
            => throw new NotSupportedException("history mapping not in pure policy probe");
    }
}
namespace CombatSolver.Engine.InCombat.Simulation {
    internal sealed class CombatPredictionSimulator;
}
