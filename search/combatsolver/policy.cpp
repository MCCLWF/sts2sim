// Translated from CombatSolver 0.47.3; see THIRD_PARTY_NOTICES.md and LICENSE.
#include "search/combatsolver/policy.h"
#include "search/combatsolver/weights.generated.h"

#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>

namespace sts2::combatsolver {
namespace {
int32_t Add(int32_t a, int32_t b) { return std::bit_cast<int32_t>(uint32_t(a) + uint32_t(b)); }
int32_t Sub(int32_t a, int32_t b) { return std::bit_cast<int32_t>(uint32_t(a) - uint32_t(b)); }
int32_t Mul(int32_t a, int32_t b) { return std::bit_cast<int32_t>(uint32_t(a) * uint32_t(b)); }
int32_t CheckedAdd(int32_t a, int32_t b) {
    const int64_t result = int64_t(a) + b;
    if (result < INT32_MIN || result > INT32_MAX)
        throw std::overflow_error("CombatSolver checked resource burden overflow");
    return static_cast<int32_t>(result);
}
template<class T> int Compare(T a, T b) { return a < b ? -1 : a > b ? 1 : 0; }
int32_t ClampToHeadroom(int32_t capacity, int32_t hp, int32_t maxHp) {
    return std::clamp(capacity, 0, std::max(0, Sub(maxHp, hp)));
}
int CompareScore(double candidate, double current) {
    return candidate > current ? -1 : current > candidate ? 1 : 0;
}
int ComparePrimary(const RouteQuality& candidate, const RouteQuality& current) {
    int comparison = Compare(current.Won, candidate.Won);
    if (comparison != 0) return comparison;
    comparison = Compare(candidate.DeathSaveUseCount, current.DeathSaveUseCount);
    if (comparison != 0) return comparison;
    comparison = Compare(candidate.StrategicHpDeficit, current.StrategicHpDeficit);
    if (comparison != 0) return comparison;
    comparison = Compare(current.GrowthHpCredit, candidate.GrowthHpCredit);
    if (comparison != 0) return comparison;
    comparison = Compare(current.GrowthRewardCount, candidate.GrowthRewardCount);
    return comparison != 0 ? comparison : Compare(candidate.CombatEndedTurn.value_or(INT32_MAX),
        current.CombatEndedTurn.value_or(INT32_MAX));
}
}  // namespace

bool PostCombatRelicHealProfile::HasAnyHeal() const {
    return UnconditionalHeal > 0 || WoundedHeal > 0;
}
int32_t PostCombatRelicHealProfile::HealFor(int32_t finalHp, int32_t finalMaxHp) const {
    int32_t capacity = std::max(0, UnconditionalHeal);
    const int32_t threshold = Mul(std::max(0, finalMaxHp), std::max(0, WoundedHpPercent)) / 100;
    if (WoundedHeal > 0 && finalHp <= threshold) capacity = Add(capacity, WoundedHeal);
    return ClampToHeadroom(capacity, finalHp, finalMaxHp);
}
int32_t PostCombatRelicHealProfile::MonotoneHealFor(int32_t finalHp, int32_t finalMaxHp) const {
    return ClampToHeadroom(std::max(0, UnconditionalHeal), finalHp, finalMaxHp);
}
BossHpRelief ActEndingBossPolicy::ResolveStrategicHpRelief(BossHpRelief encounter,
    BossHpStrategy actTransition, BossHpStrategy finalBoss) {
    if (encounter == BossHpRelief::ActClearHeal && actTransition == BossHpStrategy::MinimizeHpLoss)
        return BossHpRelief::None;
    if (encounter == BossHpRelief::RunEnding && finalBoss == BossHpStrategy::MinimizeHpLoss)
        return BossHpRelief::None;
    return encounter;
}
int32_t ActEndingBossPolicy::RawHpRequiredForPersistentValue(int32_t value, BossHpRelief relief) {
    if (value <= 0) return 0;
    switch (relief) {
    case BossHpRelief::ActClearHeal: return Mul(value, 5);
    case BossHpRelief::RunEnding: return INT32_MAX / 4;
    default: return value;
    }
}
int32_t ActEndingBossPolicy::PersistentValueOfRecoveredHp(int32_t recovered, BossHpRelief relief) {
    return recovered <= 0 ? 0 : recovered / RawHpRequiredForPersistentValue(1, relief);
}
int32_t ActEndingBossPolicy::StrategicHpDeficit(int32_t lost, int32_t maxHpDeficit,
    int32_t recovered, BossHpRelief relief, int32_t deathSaveRestored) {
    return Add(Sub(Add(lost, maxHpDeficit),
        PersistentValueOfRecoveredHp(Sub(recovered, deathSaveRestored), relief)),
        DeathSavePremium(deathSaveRestored));
}
int32_t ActEndingBossPolicy::RankedPostCombatRelicHeal(PostCombatRelicHealProfile profile,
    bool completeVictory, int32_t finalHp, int32_t finalMaxHp) {
    return completeVictory && finalHp > 0 ? profile.MonotoneHealFor(finalHp, finalMaxHp) : 0;
}
int32_t ActEndingBossPolicy::DeathSavePremium(int32_t restored) {
    return Mul(std::max(0, restored), SolverWeights::DeathSavePremiumPercent) / 100;
}
int32_t ActEndingBossPolicy::DeathSaveBeamCost(int32_t restored) {
    restored = std::max(0, restored);
    return Add(restored, DeathSavePremium(restored));
}

int32_t PotionUsePolicy::RequiredHpSaved(int32_t count) {
    return Mul(count, SolverWeights::PotionMinimumHpSaved);
}
int32_t PotionUsePolicy::ExplicitUseCount(int32_t count, int32_t automatic) { return Sub(count, automatic); }
int32_t PotionUsePolicy::AdditionalRequiredUseStrategicHpCost(int32_t cost) {
    return std::max(0, Sub(cost, SolverWeights::PotionMinimumHpSaved));
}
int32_t PotionUsePolicy::ApplyReplacementCredit(int32_t cost, int32_t count, int32_t credit) {
    return cost > 0 && count > 0 && credit > 0 ? std::max(1, Sub(cost, credit)) : cost;
}
int32_t PotionUsePolicy::HpSaved(int32_t freeDeficit, int32_t routeDeficit) {
    return std::max(0, Sub(freeDeficit, routeDeficit));
}
int32_t PotionUsePolicy::SmartRequiredHpSaved(int32_t cost, BossHpRelief relief) {
    return ActEndingBossPolicy::RawHpRequiredForPersistentValue(cost, relief);
}
int32_t PotionUsePolicy::AmbergrisRequiredHpSaved(int32_t maxHp) {
    // Exact decimal ceil(maxHp * 0.40m); integer rational avoids a binary-float boundary.
    const int64_t numerator = int64_t(maxHp) * 2;
    return static_cast<int32_t>((numerator + (numerator > 0 ? 4 : 0)) / 5);
}
int32_t PotionUsePolicy::EffectiveStrategicHpCost(int32_t cost, int32_t count, int32_t maxHp) {
    return Add(cost, Mul(count, Sub(AmbergrisRequiredHpSaved(maxHp), SolverWeights::PotionMinimumHpSaved)));
}
bool PotionUsePolicy::MeetsAmbergrisRestriction(bool hasBaseline, int32_t count, int32_t cost,
    int32_t maxHp, int32_t freeHp, int32_t routeHp) {
    if (count == 0) return true;
    if (!hasBaseline) return false;
    return std::max(0, Sub(routeHp, freeHp)) >= EffectiveStrategicHpCost(cost, count, maxHp);
}
bool PotionUsePolicy::IsEligible(SolverPotionPolicy policy, int32_t count, int32_t cost,
    bool freeWon, int32_t freeDeficit, bool anyWon, bool routeWon, int32_t routeDeficit) {
    switch (policy) {
    case SolverPotionPolicy::Disabled: return count == 0;
    case SolverPotionPolicy::RequireAtLeastOne:
        return count > 0 && (!anyWon || routeWon)
            && (count == 1 || !freeWon
                || HpSaved(freeDeficit, routeDeficit) >= AdditionalRequiredUseStrategicHpCost(cost));
    case SolverPotionPolicy::Smart:
        return count == 0 || (routeWon && !freeWon)
            || HpSaved(freeDeficit, routeDeficit) >= SmartRequiredHpSaved(cost);
    default: throw std::invalid_argument("unknown CombatSolver potion policy");
    }
}

RouteQuality RouteQuality::ForRetention(int64_t healthRisk, int32_t potionCost) {
    return {.PotionStrategicCost = potionCost, .RetentionHealthRisk = healthRisk};
}
RouteQuality RouteQuality::Primary(bool won, int32_t deficit, std::optional<int32_t> endedTurn,
    int32_t growthCredit, int32_t growthCount, int32_t deathSaveCount) {
    return {.Won = won, .DeathSaveUseCount = deathSaveCount, .StrategicHpDeficit = deficit,
        .GrowthHpCredit = growthCredit, .GrowthRewardCount = growthCount, .CombatEndedTurn = endedTurn};
}
int TheftEncounterStrategy::CompareRecovery(std::optional<SolverTheftPolicy> policy,
    bool candidateWon, int32_t candidateOutstanding, bool currentWon, int32_t currentOutstanding) {
    const int victory = Compare(currentWon, candidateWon);
    return victory != 0 ? victory : policy == SolverTheftPolicy::PreserveResources
        ? Compare(candidateOutstanding, currentOutstanding) : 0;
}
bool TheftEncounterStrategy::RecoverySatisfied(std::optional<SolverTheftPolicy> policy, int32_t outstanding) {
    return policy != SolverTheftPolicy::PreserveResources || outstanding == 0;
}
int RouteQualityPolicy::Compare(const RouteQuality& candidate, const RouteQuality& current,
    RouteQualityProjection projection, std::optional<SolverTheftPolicy> theftPolicy) {
    if (projection == RouteQualityProjection::RetentionCost) {
        const int risk = combatsolver::Compare(candidate.RetentionHealthRisk, current.RetentionHealthRisk);
        return risk != 0 ? risk : combatsolver::Compare(candidate.PotionStrategicCost, current.PotionStrategicCost);
    }
    if (projection == RouteQualityProjection::Primary) return ComparePrimary(candidate, current);
    int comparison = combatsolver::Compare(current.Won, candidate.Won);
    if (comparison != 0) return comparison;
    comparison = combatsolver::Compare(current.Survives, candidate.Survives);
    if (comparison != 0) return comparison;
    comparison = combatsolver::Compare(candidate.DeathSaveUseCount, current.DeathSaveUseCount);
    if (comparison != 0) return comparison;
    if (projection == RouteQualityProjection::Interim) {
        if (candidate.TheftPolicy == SolverTheftPolicy::PreserveResources
            && candidate.OutstandingStolenResource != current.OutstandingStolenResource)
            return combatsolver::Compare(candidate.OutstandingStolenResource, current.OutstandingStolenResource);
        if (SolverInterimResultOrdering::IsResourceTradeImprovement(candidate.StrategicHpDeficit,
            candidate.PotionStrategicCost, current.StrategicHpDeficit, current.PotionStrategicCost)) return -1;
        if (SolverInterimResultOrdering::IsResourceTradeImprovement(current.StrategicHpDeficit,
            current.PotionStrategicCost, candidate.StrategicHpDeficit, candidate.PotionStrategicCost)) return 1;
        comparison = combatsolver::Compare(current.GrowthHpCredit, candidate.GrowthHpCredit);
        if (comparison != 0) return comparison;
        comparison = combatsolver::Compare(current.GrowthRewardCount, candidate.GrowthRewardCount);
        if (comparison != 0) return comparison;
        if (candidate.StrategicHpDeficit == current.StrategicHpDeficit
            && candidate.PotionStrategicCost == current.PotionStrategicCost
            && candidate.ProjectedBattlePotionCount == current.ProjectedBattlePotionCount
            && candidate.ProjectedBattleHpLost != current.ProjectedBattleHpLost)
            return combatsolver::Compare(candidate.ProjectedBattleHpLost, current.ProjectedBattleHpLost);
        comparison = combatsolver::Compare(candidate.CombatEndedTurn.value_or(INT32_MAX),
            current.CombatEndedTurn.value_or(INT32_MAX));
        if (comparison != 0) return comparison;
        comparison = combatsolver::Compare(candidate.ProjectedBattlePotionCount, current.ProjectedBattlePotionCount);
        if (comparison != 0) return comparison;
        comparison = combatsolver::Compare(candidate.EnemyHp, current.EnemyHp);
        return comparison != 0 ? comparison : CompareScore(candidate.Score, current.Score);
    }
    comparison = TheftEncounterStrategy::CompareRecovery(theftPolicy, candidate.Won,
        candidate.OutstandingStolenResource, current.Won, current.OutstandingStolenResource);
    if (comparison != 0) return comparison;
    if (projection == RouteQualityProjection::PotionPolicy && candidate.Won && current.Won
        && candidate.StrategicHpDeficit == current.StrategicHpDeficit
        && candidate.PotionStrategicCost == current.PotionStrategicCost
        && candidate.ProjectedBattlePotionCount == current.ProjectedBattlePotionCount
        && candidate.GrowthHpCredit == current.GrowthHpCredit
        && candidate.GrowthRewardCount == current.GrowthRewardCount
        && candidate.ProjectedBattleHpLost != current.ProjectedBattleHpLost)
        return combatsolver::Compare(candidate.ProjectedBattleHpLost, current.ProjectedBattleHpLost);
    comparison = ComparePrimary(candidate, current);
    if (comparison != 0) return comparison;
    if (theftPolicy == SolverTheftPolicy::PreserveResources
        && candidate.OutstandingStolenResource != current.OutstandingStolenResource)
        return combatsolver::Compare(candidate.OutstandingStolenResource, current.OutstandingStolenResource);
    comparison = combatsolver::Compare(candidate.ProjectedBattlePotionCount, current.ProjectedBattlePotionCount);
    return comparison != 0 ? comparison : CompareScore(candidate.Score, current.Score);
}
bool SolverInterimResultOrdering::IsCompleteVictory(int32_t count, bool allDead, bool playerDead, int32_t hp) {
    return count > 0 && allDead && !playerDead && hp > 0;
}
bool SolverInterimResultOrdering::IsResourceTradeImprovement(int32_t candidateDeficit, int32_t candidateCost,
    int32_t currentDeficit, int32_t currentCost) {
    const int32_t candidateBurden = CheckedAdd(candidateDeficit, candidateCost);
    const int32_t currentBurden = CheckedAdd(currentDeficit, currentCost);
    return candidateBurden < currentBurden || (candidateBurden == currentBurden && candidateDeficit < currentDeficit);
}
bool SolverInterimResultOrdering::IsBetter(const RouteQuality& candidate, const RouteQuality& current) {
    return RouteQualityPolicy::Compare(candidate, current, RouteQualityProjection::Interim) < 0;
}
bool SolverInterimResultOrdering::CanPromoteDisplayedResult(const RouteQuality& candidate, const RouteQuality& current) {
    return (!candidate.Won || (candidate.Survives && !current.Survives)
        || candidate.DeathSaveUseCount < current.DeathSaveUseCount
        || (candidate.TheftPolicy == SolverTheftPolicy::PreserveResources
            && candidate.OutstandingStolenResource < current.OutstandingStolenResource)
        || !current.Won || Sub(candidate.ProjectedBattleHpLost, candidate.GrowthHpCredit)
            <= Sub(current.ProjectedBattleHpLost, current.GrowthHpCredit)) && IsBetter(candidate, current);
}

bool TranspositionFrontier::Dominates(TranspositionLabel left, TranspositionLabel right) {
    return left.PotionCount <= right.PotionCount && left.PotionStrategicCost <= right.PotionStrategicCost
        && left.FutureSoldHp <= right.FutureSoldHp && left.CumulativePlayerHpLost <= right.CumulativePlayerHpLost
        && left.ActionCount <= right.ActionCount && left.Score >= right.Score;
}
bool TranspositionFrontier::TryAccept(TranspositionLabel next) {
    if (labels_.empty()) {
        if (Dominates(single_, next)) return false;
        if (Dominates(next, single_)) single_ = next;
        else labels_ = {single_, next};
        return true;
    }
    for (const auto& current : labels_) if (Dominates(current, next)) return false;
    std::erase_if(labels_, [&](const auto& current) { return Dominates(next, current); });
    labels_.push_back(next);
    if (labels_.size() == 1) {
        single_ = next;
        std::vector<TranspositionLabel>().swap(labels_);
    }
    return true;
}
bool TranspositionFrontier::HasIdenticalCosts(TranspositionLabel next) const {
    auto same = [&](const TranspositionLabel& current) {
        return current.PotionCount == next.PotionCount && current.PotionStrategicCost == next.PotionStrategicCost
            && current.FutureSoldHp == next.FutureSoldHp && current.CumulativePlayerHpLost == next.CumulativePlayerHpLost
            && current.ActionCount == next.ActionCount;
    };
    if (labels_.empty()) return same(single_);
    return std::any_of(labels_.begin(), labels_.end(), same);
}
bool SearchBudgetWindow::CanStart(int32_t minimum) const {
    return RemainingMilliseconds > minimum && RemainingNodes > 0;
}
SolverSearchProfile SearchBudgetWindow::Limit(SolverSearchProfile profile, int32_t maxNodes,
    int32_t maxMilliseconds, int32_t reserve) const {
    const int64_t nodes = std::min(int64_t(maxNodes), RemainingNodes);
    profile.MaxExpandedNodes = std::bit_cast<int32_t>(static_cast<uint32_t>(nodes));
    profile.SoftTimeBudgetMilliseconds = std::min(maxMilliseconds, Sub(RemainingMilliseconds, reserve));
    return profile;
}
}  // namespace sts2::combatsolver
