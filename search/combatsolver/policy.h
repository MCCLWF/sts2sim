// Translated from CombatSolver 0.47.3; see THIRD_PARTY_NOTICES.md and LICENSE.
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace sts2::combatsolver {

enum class BossHpRelief { None, ActClearHeal, RunEnding };
enum class BossHpStrategy { ProgressionFirst, MinimizeHpLoss };
enum class SolverPotionPolicy { Disabled, Smart, RequireAtLeastOne };
enum class SolverTheftPolicy { PreserveResources, LetEscape };
enum class RouteQualityProjection { Primary, Interim, PotionPolicy, RetentionCost };

struct PostCombatRelicHealProfile {
    int32_t UnconditionalHeal = 0;
    int32_t WoundedHeal = 0;
    int32_t WoundedHpPercent = 0;
    bool HasAnyHeal() const;
    int32_t HealFor(int32_t finalHp, int32_t finalMaxHp) const;
    int32_t MonotoneHealFor(int32_t finalHp, int32_t finalMaxHp) const;
};

struct ActEndingBossPolicy {
    static BossHpRelief ResolveStrategicHpRelief(BossHpRelief encounter,
        BossHpStrategy actTransition, BossHpStrategy finalBoss);
    static int32_t RawHpRequiredForPersistentValue(int32_t value, BossHpRelief relief);
    static int32_t PersistentValueOfRecoveredHp(int32_t recovered, BossHpRelief relief);
    static int32_t StrategicHpDeficit(int32_t lost, int32_t maxHpDeficit, int32_t recovered,
        BossHpRelief relief, int32_t deathSaveRestored = 0);
    static int32_t RankedPostCombatRelicHeal(PostCombatRelicHealProfile profile,
        bool completeVictory, int32_t finalHp, int32_t finalMaxHp);
    static int32_t DeathSavePremium(int32_t restored);
    static int32_t DeathSaveBeamCost(int32_t restored);
};

struct PotionUsePolicy {
    static int32_t RequiredHpSaved(int32_t count);
    static int32_t ExplicitUseCount(int32_t count, int32_t automatic);
    static int32_t AdditionalRequiredUseStrategicHpCost(int32_t cost);
    static int32_t ApplyReplacementCredit(int32_t cost, int32_t count, int32_t credit);
    static int32_t HpSaved(int32_t freeDeficit, int32_t routeDeficit);
    static int32_t SmartRequiredHpSaved(int32_t cost, BossHpRelief relief = BossHpRelief::None);
    static int32_t AmbergrisRequiredHpSaved(int32_t maxHp);
    static int32_t EffectiveStrategicHpCost(int32_t cost, int32_t ambergrisCount, int32_t maxHp);
    static bool MeetsAmbergrisRestriction(bool hasBaseline, int32_t count, int32_t cost,
        int32_t maxHp, int32_t freeHp, int32_t routeHp);
    static bool IsEligible(SolverPotionPolicy policy, int32_t explicitCount, int32_t cost,
        bool freeWon, int32_t freeDeficit, bool anyWon, bool routeWon, int32_t routeDeficit);
};

struct RouteQuality {
    bool Won = false;
    bool Survives = false;
    int32_t DeathSaveUseCount = 0;
    int32_t OutstandingStolenResource = 0;
    int32_t StrategicHpDeficit = 0;
    int32_t GrowthHpCredit = 0;
    int32_t GrowthRewardCount = 0;
    std::optional<int32_t> CombatEndedTurn;
    int32_t ProjectedBattleHpLost = 0;
    int32_t PotionStrategicCost = 0;
    int32_t ProjectedBattlePotionCount = 0;
    int32_t EnemyHp = 0;
    double Score = 0;
    std::optional<SolverTheftPolicy> TheftPolicy;
    int64_t RetentionHealthRisk = 0;

    static RouteQuality ForRetention(int64_t healthRisk, int32_t potionCost);
    static RouteQuality Primary(bool won, int32_t deficit, std::optional<int32_t> endedTurn,
        int32_t growthCredit = 0, int32_t growthCount = 0, int32_t deathSaveCount = 0);
};

struct TheftEncounterStrategy {
    static int CompareRecovery(std::optional<SolverTheftPolicy> policy,
        bool candidateWon, int32_t candidateOutstanding, bool currentWon, int32_t currentOutstanding);
    static bool RecoverySatisfied(std::optional<SolverTheftPolicy> policy, int32_t outstanding);
};

struct RouteQualityPolicy {
    // Negative means candidate is better. Projections intentionally have different key orders.
    static int Compare(const RouteQuality& candidate, const RouteQuality& current,
        RouteQualityProjection projection, std::optional<SolverTheftPolicy> theftPolicy = std::nullopt);
};

struct SolverInterimResultOrdering {
    static bool IsCompleteVictory(int32_t actionCount, bool allEnemiesDead,
        bool playerDead, int32_t projectedPlayerHp);
    static bool IsResourceTradeImprovement(int32_t candidateDeficit, int32_t candidateCost,
        int32_t currentDeficit, int32_t currentCost);
    static bool IsBetter(const RouteQuality& candidate, const RouteQuality& current);
    static bool CanPromoteDisplayedResult(const RouteQuality& candidate, const RouteQuality& current);
};

struct TranspositionLabel {
    int32_t PotionCount;
    int32_t PotionStrategicCost;
    int32_t FutureSoldHp;
    int32_t CumulativePlayerHpLost;
    int32_t ActionCount;
    double Score;
};

// One table entry owns its policy frontier. The state-key implementation is a separate port.
class TranspositionFrontier {
public:
    explicit TranspositionFrontier(TranspositionLabel first) : single_(first) {}
    size_t LabelCount() const { return labels_.empty() ? 1 : labels_.size(); }
    bool TryAccept(TranspositionLabel next);
    // Consumer fast path: for the same complete state, identical cost
    // dimensions produce the same score and the existing label dominates.
    bool HasIdenticalCosts(TranspositionLabel next) const;
private:
    static bool Dominates(TranspositionLabel left, TranspositionLabel right);
    TranspositionLabel single_;
    std::vector<TranspositionLabel> labels_;
};

// Pure profile values only. Experimental ranking-model objects and the coordinator
// are not implemented by this type; the practical solver uses a simpler coordinator.
struct SolverSearchProfile {
    int32_t BeamWidth = 60;
    int32_t MaxExpandedNodes = 120000;
    int32_t MaxCardBranchesPerNode = 32;
    int32_t MaxPileChoiceBranchesPerAction = 18;
    int32_t MaxHandChoiceBranchesPerAction = 24;
    int32_t SoftTimeBudgetMilliseconds = 120000;
    bool SecondRankBand = false;
    bool ContinuousThreatRanking = false;
    bool BaseScoreTacticalTies = false;
    bool AdaptiveNoveltyRefinement = false;
    bool OffensiveRefinementPortfolio = false;
    bool BoundedOffensiveRefinementPortfolio = false;
    bool ReallocatedRefinementPortfolio = true;
    bool StopPortfolioAtHpTarget = true;
    bool BaseScoreOnly = false;
    bool AggressivePowerCommitment = false;
};

struct SearchBudgetWindow {
    int32_t RemainingMilliseconds;
    int64_t RemainingNodes;
    bool CanStart(int32_t minimumMilliseconds) const;
    SolverSearchProfile Limit(SolverSearchProfile profile, int32_t maximumNodes,
        int32_t maximumMilliseconds, int32_t reserveMilliseconds) const;
};

}  // namespace sts2::combatsolver
