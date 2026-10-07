#include <iostream>
#include <stdexcept>
#include <thread>
#include "search/combatsolver/solver.h"
#include "game/All.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PotionCmd.h"

using namespace sts2;
using namespace sts2::combatsolver;
namespace {
void Require(bool condition, const char* reason) { if (!condition) throw std::runtime_error(reason); }
CombatSearchConfig Small() {
    CombatSearchConfig config;
    config.Profile.BeamWidth = 8; config.Profile.MaxExpandedNodes = 500;
    config.Profile.SoftTimeBudgetMilliseconds = 2000;
    config.Profile.ReallocatedRefinementPortfolio = false;
    config.MaxTurns = 4;
    return config;
}
void Replay(const Env& source, const CombatSearchResult& result) {
    auto replay = CombatSearchState::Capture(source);
    for (auto action : result.Actions) {
        auto legal = replay.LegalActions();
        Require(std::find(legal.begin(), legal.end(), action) != legal.end(), "solver returned illegal route action");
        replay = replay.Predict(action);
    }
    Require(replay.NativeHash() == result.FinalSearchHash, "solver mixed route states");
    Require(replay.Outcome() == CombatOutcome::Victory && result.Quality.Won, "solver route did not reach victory");
    Require(replay.Facts().PlayerHp == result.FinalPlayerHp, "solver reported settlement HP instead of search-terminal HP");
}
void CompareUncached(const Env& source, const CombatSearchConfig& config, const CombatSearchResult& cached) {
    auto referenceConfig = config;
    referenceConfig.EvaluationCacheEntries = 0;
    referenceConfig.TransitionCacheEntries = 0;
    referenceConfig.ReuseEndTurnForecast = false;
    referenceConfig.PruneBeforeScoring = false;
    referenceConfig.CollectPhaseTimings = false;
    const auto reference = CombatSolver::Solve(source, referenceConfig);
    Require(cached.Actions == reference.Actions && cached.FinalSearchHash == reference.FinalSearchHash,
        "cache or EndTurn reuse changed the selected route");
    Require(cached.FinalPlayerHp == reference.FinalPlayerHp
        && cached.Quality.Score == reference.Quality.Score
        && RouteQualityPolicy::Compare(cached.Quality, reference.Quality, RouteQualityProjection::Primary) == 0
        && RouteQualityPolicy::Compare(cached.Quality, reference.Quality, RouteQualityProjection::PotionPolicy) == 0,
        "cache changed quality or potion ordering");
    Require(cached.Metrics.ExpandedNodes == reference.Metrics.ExpandedNodes
        && cached.Metrics.Passes == reference.Metrics.Passes, "cache changed node-budget accounting");
}
void CacheEquivalence() {
    int64_t hits = 0, reused = 0;
    for (auto character : {CharacterId::IRONCLAD, CharacterId::SILENT, CharacterId::REGENT,
                           CharacterId::NECROBINDER, CharacterId::DEFECT}) {
        Env source;
        source.ResetCombat({.seed = "CSPERFCACHE", .character = character, .ascension = 10},
            EncounterId::FUZZY_WURM_CRAWLER_WEAK);
        const auto before = source.ExportSnapshotJson();
        auto config = Small();
        config.Profile.ReallocatedRefinementPortfolio = true;
        config.EvaluationCacheEntries = 64;
        config.TransitionCacheEntries = 64;
        config.CollectPhaseTimings = true;
        const auto result = CombatSolver::Solve(source, config);
        CompareUncached(source, config, result);
        auto replay = CombatSearchState::Capture(source);
        for (auto action : result.Actions) {
            const auto legal = replay.LegalActions();
            Require(std::find(legal.begin(), legal.end(), action) != legal.end(), "cached fallback is illegal");
            replay = replay.Predict(action);
        }
        Require(replay.NativeHash() == result.FinalSearchHash, "cached fallback replay diverged");
        Require(result.Metrics.TransitionCacheStored <= config.TransitionCacheEntries,
            "successor cache exceeded its configured entry cap");
        Require(static_cast<size_t>(result.Metrics.TransitionCacheEstimatedBytes) <= config.TransitionCacheBytes,
            "successor cache exceeded its native payload budget");
        if (character == CharacterId::IRONCLAD) {
            auto bounded = config;
            bounded.TransitionCacheBytes = 1;
            const auto limited = CombatSolver::Solve(source, bounded);
            Require(limited.Metrics.TransitionCacheStored == 0 && limited.Metrics.TransitionCacheEstimatedBytes == 0
                && limited.Actions == result.Actions && limited.FinalSearchHash == result.FinalSearchHash,
                "byte budget did not disable state storage without changing the route");
        }
        hits += result.Metrics.TransitionCacheHits + result.Metrics.ForecastCacheHits;
        reused += result.Metrics.EndTurnReused;
        Require(source.ExportSnapshotJson() == before, "cache changed its caller root");
    }
    Require(hits > 0 && reused > 0, "cache equivalence fixtures did not exercise reuse");
}
void WinningRouteAndTerminalRoot() {
    Env source;
    source.ResetCombat({.seed = "CSPROUTE", .character = CharacterId::IRONCLAD}, EncounterId::CULTISTS_NORMAL);
    for (auto enemy : CombatState(&source.sim()).Enemies()) enemy.st().current_hp = 1;
    Player(&source.sim(), 1).Creature().st().current_hp = 7;
    const auto before = source.ExportSnapshotJson();
    const auto result = CombatSolver::Solve(source, Small());
    Replay(source, result);
    Require(source.ExportSnapshotJson() == before, "solver changed caller root");
    auto terminal = CombatSearchState::Capture(source);
    for (auto action : result.Actions) terminal = terminal.Predict(action);
    auto frozen = terminal.CloneEnv();
    const auto complete = CombatSolver::Solve(*frozen, Small());
    Require(complete.Actions.empty() && complete.Quality.Won && complete.FinalSearchHash == frozen->Hash(),
        "already-terminal root generated a continuation");
    auto settled = terminal.SettleOutcome();
    Require(Player(&settled->sim(), 1).Creature().CurrentHp() > result.FinalPlayerHp, "fixture lacks a settlement heal");
}
void PotionCounterfactual() {
    Env source;
    source.ResetCombat({.seed = "CSPOTPOLICY", .character = CharacterId::IRONCLAD}, EncounterId::CULTISTS_NORMAL);
    auto enemies = CombatState(&source.sim()).Enemies();
    enemies.front().st().current_hp = 10;
    for (size_t i = 1; i < enemies.size(); ++i) CreatureCmd::Kill(enemies[i]);
    Player player(&source.sim(), 1);
    PotionCmd::TryToProcure(PotionId::FIRE_POTION, player);
    const auto before = source.ExportSnapshotJson();
    auto config = Small(); config.PotionPolicy = SolverPotionPolicy::Disabled;
    const auto free = CombatSolver::Solve(source, config); Replay(source, free);
    CompareUncached(source, config, free);
    Require(free.Quality.ProjectedBattlePotionCount == 0, "disabled policy used a potion");
    config.PotionPolicy = SolverPotionPolicy::Smart;
    const auto smart = CombatSolver::Solve(source, config); Replay(source, smart);
    CompareUncached(source, config, smart);
    Require(smart.Quality.ProjectedBattlePotionCount == 0, "smart policy spent a potion without enough counterfactual HP benefit");
    config.PotionPolicy = SolverPotionPolicy::RequireAtLeastOne;
    const auto required = CombatSolver::Solve(source, config); Replay(source, required);
    CompareUncached(source, config, required);
    Require(required.Quality.ProjectedBattlePotionCount >= 1, "required policy did not use a potion");
    Require(source.ExportSnapshotJson() == before, "potion baseline or audit changed root");
}
void ChoiceRoot() {
    Env source;
    source.ResetCombat({.seed = "CSPORTCHOICE", .character = CharacterId::SILENT}, EncounterId::CULTISTS_NORMAL);
    for (auto enemy : CombatState(&source.sim()).Enemies()) enemy.st().current_hp = 1;
    Player player(&source.sim(), 1);
    CardPileCmd::Add(CombatState(&source.sim()).CreateCard(CardId::SURVIVOR, player), PileType::Hand);
    auto hand = player.PlayerCombatState().Hand().Cards();
    Operation operation; operation.source_hash = source.Hash(); operation.stop_mask = 2;
    operation.action.kind = Action::PlayCard;
    for (size_t i = 0; i < hand.size(); ++i) if (hand[i].CardId_() == CardId::SURVIVOR) operation.action.index = static_cast<int32_t>(i);
    auto choice = source.PredictOperation(operation);
    Require(choice->decision().kind == DecisionKind::Choice, "fixture lacks pending selection");
    const auto before = choice->ExportSnapshotJson();
    const auto result = CombatSolver::Solve(*choice, Small()); Replay(*choice, result);
    CompareUncached(*choice, Small(), result);
    Require(!result.Actions.empty() && result.Actions.front().kind == Action::Answer, "solver lost root selection prefix");
    Require(choice->ExportSnapshotJson() == before, "solver changed pending choice root");
}
void UnsupportedProfile() {
    Env source;
    source.ResetCombat({.seed = "CSPROFILE", .character = CharacterId::IRONCLAD}, EncounterId::CULTISTS_NORMAL);
    auto config = Small(); config.Profile.ContinuousThreatRanking = true;
    try { CombatSolver::Solve(source, config); throw std::runtime_error("unsupported profile silently ignored"); }
    catch (const std::invalid_argument&) { }
}
void EquivalentFocusPasses() {
    for (auto encounter : {EncounterId::FUZZY_WURM_CRAWLER_WEAK, EncounterId::CULTISTS_NORMAL}) {
        Env source;
        source.ResetCombat({.seed = "CSFOCUSREUSE", .character = CharacterId::IRONCLAD}, encounter);
        for (auto enemy : CombatState(&source.sim()).Enemies()) enemy.st().current_hp = 1;
        const auto before = source.ExportSnapshotJson();
        auto config = Small();
        config.Profile.BeamWidth = 4;
        config.Profile.MaxExpandedNodes = 3000;
        config.Profile.ReallocatedRefinementPortfolio = true;
        config.Profile.StopPortfolioAtHpTarget = false;
        config.Profile.SoftTimeBudgetMilliseconds = 60000;
        const auto optimized = CombatSolver::Solve(source, config);
        config.SkipEquivalentFocusPasses = false;
        const auto reference = CombatSolver::Solve(source, config);
        Replay(source, optimized); Replay(source, reference);
        Require(optimized.Actions == reference.Actions && optimized.FinalSearchHash == reference.FinalSearchHash
            && optimized.Quality.Score == reference.Quality.Score,
            "equivalent focus reuse changed the winning route");
        if (encounter == EncounterId::FUZZY_WURM_CRAWLER_WEAK) {
            Require(optimized.Metrics.EquivalentFocusPassesSkipped > 0
                && optimized.Metrics.ExpandedNodes < reference.Metrics.ExpandedNodes,
                "single-enemy completed pass was unnecessarily repeated");
        } else {
            Require(optimized.Metrics.EquivalentFocusPassesSkipped == 0,
                "focus reuse skipped a member with different targeted actions");
        }
        Require(source.ExportSnapshotJson() == before, "focus reuse changed the caller root");
    }
}
void ParallelPortfolioContracts() {
    Env source;
    source.ResetCombat({.seed = "CSPARALLEL", .character = CharacterId::IRONCLAD}, EncounterId::CULTISTS_NORMAL);
    for (auto enemy : CombatState(&source.sim()).Enemies()) enemy.st().current_hp = 10;
    PotionCmd::TryToProcure(PotionId::FIRE_POTION, Player(&source.sim(), 1));
    const auto before = source.ExportSnapshotJson();
    auto config = Small();
    config.Profile.BeamWidth = 4;
    config.Profile.MaxExpandedNodes = 1500;
    config.Profile.SoftTimeBudgetMilliseconds = 60000;
    config.Profile.ReallocatedRefinementPortfolio = true;
    config.Profile.StopPortfolioAtHpTarget = false;
    int peak = 1;
    for (auto policy : {SolverPotionPolicy::Disabled, SolverPotionPolicy::Smart, SolverPotionPolicy::RequireAtLeastOne}) {
        config.PotionPolicy = policy;
        config.ThreadCount = 1;
        const auto serial = CombatSolver::Solve(source, config);
        for (int threads : {2, 4}) {
            config.ThreadCount = threads;
            const auto parallel = CombatSolver::Solve(source, config);
            const auto repeated = CombatSolver::Solve(source, config);
            Replay(source, parallel);
            Require(parallel.Actions == repeated.Actions && parallel.FinalSearchHash == repeated.FinalSearchHash,
                "parallel tie ordering depends on thread completion order");
            Require(RouteQualityPolicy::Compare(parallel.Quality, serial.Quality, RouteQualityProjection::Primary) == 0,
                "parallel portfolio changed the verified small battle quality");
            Require(parallel.Metrics.ExpandedNodes <= config.Profile.MaxExpandedNodes,
                "parallel tasks multiplied the request node budget");
            Require(parallel.Metrics.TransitionCacheStored <= config.TransitionCacheEntries
                && static_cast<size_t>(parallel.Metrics.TransitionCacheEstimatedBytes) <= config.TransitionCacheBytes,
                "parallel workers multiplied the owned-state cache budget");
            Require(parallel.Quality.ProjectedBattlePotionCount == serial.Quality.ProjectedBattlePotionCount,
                "parallel portfolio changed the potion counterfactual policy");
            peak = std::max(peak, parallel.Metrics.PeakConcurrentPasses);
        }
    }
    Require(peak > 1, "parallel contracts never dispatched overlapping focus tasks");
    const auto coordinator = std::this_thread::get_id();
    config.ThreadCount = 2;
    config.PotionPolicy = SolverPotionPolicy::Disabled;
    config.Cancelled = [&] { return std::this_thread::get_id() != coordinator; };
    const auto cancelled = CombatSolver::Solve(source, config);
    Replay(source, cancelled);
    Require(cancelled.Metrics.StopReason == "cancelled", "worker cancellation did not stop the request");
    Require(source.ExportSnapshotJson() == before, "parallel task changed caller root");
    config.ThreadCount = 0;
    try { CombatSolver::Solve(source, config); throw std::runtime_error("zero thread count accepted"); }
    catch (const std::invalid_argument&) { }
}
}
int main() {
    try {
        WinningRouteAndTerminalRoot(); PotionCounterfactual(); ChoiceRoot(); UnsupportedProfile(); CacheEquivalence();
        EquivalentFocusPasses();
        ParallelPortfolioContracts();
        std::cout << "CombatSolver route replay, terminal root, potion counterfactual, and selection contracts passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
