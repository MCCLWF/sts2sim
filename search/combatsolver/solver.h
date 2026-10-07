#pragma once

#include <functional>
#include <string>

#include "search/combatsolver/env_state.h"
#include "search/combatsolver/policy.h"

namespace sts2::combatsolver {

struct CombatSearchConfig {
    SolverSearchProfile Profile;
    SolverPotionPolicy PotionPolicy = SolverPotionPolicy::Smart;
    int32_t MaxTurns = 64;
    int32_t MaxActionsPerTurn = 32;
    int32_t MaxChoiceLeavesPerAction = 24;
    int32_t ForecastChoiceLeaves = 8;
    bool StopAfterStableWidth = true;
    bool CollectPhaseTimings = false;
    int32_t EvaluationCacheEntries = 100000;
    int32_t TransitionCacheEntries = 8192;
    size_t TransitionCacheBytes = 512 * 1024 * 1024;
    bool ReuseEndTurnForecast = true;
    bool PruneBeforeScoring = true;
    bool SkipEquivalentFocusPasses = true;
    int32_t ThreadCount = 1;
    std::function<bool()> Cancelled;
};

struct CombatSearchPassMetrics {
    int32_t BeamWidth = 0;
    int32_t FocusTarget = -1;
    bool AllowsPotions = false;
    bool Completed = false;
    int64_t ExpandedNodes = 0;
    int64_t NativePredictions = 0;
    double ElapsedMilliseconds = 0;
    std::optional<RouteQuality> BestQuality;
};

struct CombatSearchMetrics {
    int64_t ExpandedNodes = 0;
    int64_t Transitions = 0;
    int64_t ForecastTransitions = 0;
    int64_t RouteReplayTransitions = 0;
    int64_t ChoiceAnswers = 0;
    int64_t TranspositionPruned = 0;
    int64_t EarlyTranspositionPruned = 0;
    int32_t EquivalentFocusPassesSkipped = 0;
    int32_t PeakConcurrentPasses = 1;
    int64_t DuplicateActionsPruned = 0;
    int64_t ActionTruncated = 0;
    int64_t ChoiceTruncated = 0;
    int32_t Passes = 0;
    int32_t SearchedTurns = 0;
    int64_t BoardCaptures = 0;
    int64_t BoardCacheHits = 0;
    int64_t ForecastCacheHits = 0;
    int64_t TransitionCacheHits = 0;
    int64_t TransitionCacheStored = 0;
    int64_t NativePredictions = 0;
    int64_t EndTurnReused = 0;
    int64_t TransitionCacheEstimatedBytes = 0;
    double BoardMilliseconds = 0;
    double TransitionMilliseconds = 0;
    double ForecastMilliseconds = 0;
    double LegalActionsMilliseconds = 0;
    double RetainMilliseconds = 0;
    double ElapsedMilliseconds = 0;
    std::string StopReason;
    std::vector<CombatSearchPassMetrics> PassDetails;
};

struct CombatSearchResult {
    uint64_t RootHash = 0;
    std::vector<Action> Actions;
    RouteQuality Quality;
    int32_t FinalPlayerHp = 0;
    int32_t FinalPlayerMaxHp = 0;
    uint64_t FinalSearchHash = 0;
    CombatSearchMetrics Metrics;
};

// Practical native port: upstream weights/route/potion/transposition policies,
// with a simpler per-turn Beam organization. Quality equivalence is evaluated
// with matched-root combat outcomes, not assumed from the shared primitives.
class CombatSolver {
public:
    static CombatSearchResult Solve(const Env& root, const CombatSearchConfig& config = {});
};

}  // namespace sts2::combatsolver
