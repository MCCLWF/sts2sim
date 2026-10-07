#include "search/combatsolver/solver.h"
#include "search/combatsolver/weights.generated.h"

#include <algorithm>
#include <chrono>
#include <future>
#include <memory>
#include <mutex>
#include <numeric>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include "game/All.h"
#include "engine/sim/state_hash.h"
#include "game/MonsterMoves/Intents/AttackIntent.h"

namespace sts2::combatsolver {
namespace {
using Clock = std::chrono::steady_clock;

class PhaseTimer {
public:
    PhaseTimer(bool enabled, double& total) : total_(enabled ? &total : nullptr) {
        if (total_) start_ = Clock::now();
    }
    ~PhaseTimer() {
        if (total_) *total_ += std::chrono::duration<double, std::milli>(Clock::now() - start_).count();
    }
private:
    double* total_;
    Clock::time_point start_;
};

struct Path {
    std::shared_ptr<const Path> Parent;
    Action Step;
    int32_t Count;
};
struct Forecast {
    int32_t Hp;
    bool Dies;
    bool UnresolvedChoice;
};
struct Board {
    uint64_t RetentionKey = 0;
    double VulnerableValue = 0;
    int64_t FocusPressure = 0;
    int32_t EnemyHp = 0;
    int32_t PersistentBuff = 0;
    int32_t DelayedDamage = 0;
    int32_t Clutter = 0;
    int32_t DeathSaves = 0;
    int32_t DeathSaveHp = 0;
    std::vector<uint64_t> HandCardKeys;
};
struct Node {
    CombatSearchState State;
    std::shared_ptr<const Path> Route;
    CombatFacts Facts;
    Board Features;
    uint64_t Hash;
    int32_t Potions = 0;
    int32_t PotionCost = 0;
    int32_t Ambergris = 0;
    int32_t HpLost = 0;
    int32_t HpRecovered = 0;
    int32_t LastTarget = -1;
    double Score = 0;
    std::optional<Forecast> Threat;
    std::unique_ptr<CombatSearchState> EndTurn;
    explicit Node(CombatSearchState state) : State(std::move(state)), Facts(State.Facts()), Hash(State.NativeHash()) {}
    int32_t ActionCount() const { return Route ? Route->Count : 0; }
};
using Nodes = std::vector<std::unique_ptr<Node>>;

struct EdgeKey {
    uint64_t State;
    Action Step;
    bool operator==(const EdgeKey&) const = default;
};
struct EdgeHash {
    size_t operator()(const EdgeKey& key) const {
        StateHasher hash;
        hash(key.State); hash(key.Step.kind); hash(key.Step.index); hash(key.Step.target);
        hash(key.Step.coord.col); hash(key.Step.coord.row);
        return hash.finish();
    }
};

uint64_t CardBucket(CardModel card) {
    auto value = card.st();
    value.clone_of = 0; value.deck_version = 0; value.affliction_instance = 0;
    StateHasher key; key(value); return key.finish();
}

// The registry values are translated from PotionValuationRegistry.cs.
int32_t PotionCost(PotionModel potion) {
    if (potion.Rarity() == PotionRarity::Token) return 0;
    const auto id = potion.PotionId_();
    switch (id) {
    case PotionId::GLOWWATER_POTION: case PotionId::SWIFT_POTION: case PotionId::GAMBLERS_BREW:
    case PotionId::DUPLICATOR: case PotionId::OROBIC_ACID: case PotionId::POT_OF_GHOULS:
        return SolverWeights::PotionHighValueHpSaved;
    case PotionId::DISTILLED_CHAOS: case PotionId::CLARITY: case PotionId::RADIANT_TINCTURE:
    case PotionId::CURE_ALL: case PotionId::LIQUID_MEMORIES: case PotionId::BOTTLED_POTENTIAL:
    case PotionId::TOUCH_OF_INSANITY: return SolverWeights::PotionElevatedValueHpSaved;
    default: return SolverWeights::PotionMinimumHpSaved;
    }
}

class SearchRun {
public:
    SearchRun(const Env& source, const CombatSearchConfig& config, SearchRun* donor = nullptr)
        : config_(config), start_(donor ? donor->start_ : Clock::now()), root_(CombatSearchState::Capture(source)),
          initial_(root_.Facts()), donor_(donor) {
        if (config.Profile.BeamWidth < 1 || config.Profile.MaxExpandedNodes < 1
            || config.Profile.SoftTimeBudgetMilliseconds < 1 || config.MaxTurns < 1
            || config.MaxActionsPerTurn < 1 || config.MaxChoiceLeavesPerAction < 1 || config.ForecastChoiceLeaves < 1)
            throw std::invalid_argument("CombatSolver budgets and widths must be positive");
        if (config.ThreadCount < 1) throw std::invalid_argument("CombatSolver thread count must be positive");
        if (config.EvaluationCacheEntries < 0 || config.TransitionCacheEntries < 0)
            throw std::invalid_argument("CombatSolver cache limits must be nonnegative");
        if (config.PotionPolicy != SolverPotionPolicy::Disabled && config.PotionPolicy != SolverPotionPolicy::Smart
            && config.PotionPolicy != SolverPotionPolicy::RequireAtLeastOne)
            throw std::invalid_argument("unknown CombatSolver potion policy");
        const auto& profile = config.Profile;
        if (profile.SecondRankBand || profile.ContinuousThreatRanking || profile.BaseScoreTacticalTies
            || profile.AdaptiveNoveltyRefinement || profile.OffensiveRefinementPortfolio
            || profile.BoundedOffensiveRefinementPortfolio || profile.BaseScoreOnly || profile.AggressivePowerCommitment)
            throw std::invalid_argument("experimental upstream ranking profile is not supported by the native consumer");
        if (profile.MaxCardBranchesPerNode < 1 || profile.MaxPileChoiceBranchesPerAction < 1
            || profile.MaxHandChoiceBranchesPerAction < 1)
            throw std::invalid_argument("CombatSolver branch limits must be positive");
        auto inspected = root_.CloneEnv();
        Player player(&inspected->sim(), 1);
        for (auto enemy : CombatState(&inspected->sim()).Enemies())
            if (enemy.IsAlive() && enemy.IsPrimaryEnemy()) focusTargets_.push_back(enemy.CombatId());
        for (auto potion : player.Potions()) {
            hasPotions_ |= potion.Usage() != PotionUsage::Automatic;
            if (potion.PotionId_() == PotionId::FAIRY_IN_A_BOTTLE) ++initialFairies_;
        }
        if (auto tail = player.GetRelic(RelicId::LIZARD_TAIL)) initialTailReady_ = !tail.IsUsedUp();
        for (auto relic : player.Relics()) {
            if (relic.RelicId_() == RelicId::BURNING_BLOOD || relic.RelicId_() == RelicId::BLACK_BLOOD)
                postCombatHeal_.UnconditionalHeal += relic.DynamicVars().Heal().IntValue();
            if (relic.RelicId_() == RelicId::MEAT_ON_THE_BONE) {
                postCombatHeal_.WoundedHeal = relic.DynamicVars().Heal().IntValue();
                postCombatHeal_.WoundedHpPercent = relic.DynamicVars()["HpThreshold"].IntValue();
            }
        }
        // Encounter metadata, rather than a search-owned hardcoded encounter list.
        if (data(inspected->state().combat.encounter).room_type == RoomType::Boss) {
            const auto& run = inspected->state().run;
            if (run.current_act_index < static_cast<int32_t>(run.acts.size()) - 1) relief_ = BossHpRelief::ActClearHeal;
            // Final-act second-boss identification is pending; conservative HP valuation.
        }
        auto first = MakeRoot();
        initialBuff_ = first->Features.PersistentBuff;
    }

    CombatSearchResult Solve() {
        if (root_.Outcome() != CombatOutcome::Ongoing) {
            auto terminal = MakeRoot(); Score(*terminal);
            CombatSearchResult result;
            result.RootHash = result.FinalSearchHash = root_.NativeHash();
            result.Quality = Quality(*terminal); result.FinalPlayerHp = terminal->Facts.PlayerHp;
            result.FinalPlayerMaxHp = terminal->Facts.PlayerMaxHp;
            metrics_.StopReason = "already_terminal"; metrics_.ElapsedMilliseconds = Elapsed();
            result.Metrics = metrics_; return result;
        }
        // Both searches share the same request clock and expansion ledger. The
        // potion-free baseline is a real counterfactual, not a guessed HP score.
        const bool audit = hasPotions_ && config_.PotionPolicy != SolverPotionPolicy::Disabled;
        const int64_t freeNodeCap = audit ? std::max<int64_t>(1, config_.Profile.MaxExpandedNodes / 2)
            : config_.Profile.MaxExpandedNodes;
        const int64_t freeTimeCap = audit ? std::max<int64_t>(1, config_.Profile.SoftTimeBudgetMilliseconds / 2)
            : config_.Profile.SoftTimeBudgetMilliseconds;
        auto free = Portfolio(false, freeNodeCap, freeTimeCap);
        std::unique_ptr<Node> selected = std::move(free);
        std::optional<RouteQuality> baseline;
        int32_t baselineHp = initial_.PlayerHp;
        if (selected) { baseline = Quality(*selected); baselineHp = selected->Facts.PlayerHp; }
        if (audit && !RequestStopped()) {
            auto withPotions = Portfolio(true, config_.Profile.MaxExpandedNodes, config_.Profile.SoftTimeBudgetMilliseconds,
                baseline, baselineHp);
            if (withPotions && (!selected || Eligible(*withPotions, baseline, baselineHp))) {
                const auto q = Quality(*withPotions);
                if (!selected || config_.PotionPolicy == SolverPotionPolicy::RequireAtLeastOne
                    || RouteQualityPolicy::Compare(q, Quality(*selected), RouteQualityProjection::PotionPolicy) < 0)
                    selected = std::move(withPotions);
            }
        }
        if (config_.PotionPolicy == SolverPotionPolicy::RequireAtLeastOne && (!selected || selected->Potions < 1))
            throw std::runtime_error("CombatSolver potion policy unsatisfied within the request budget");
        if (!selected || !selected->Route) throw std::runtime_error("CombatSolver budget produced no executable route");
        CombatSearchResult result;
        result.RootHash = root_.NativeHash(); result.FinalSearchHash = selected->Hash;
        result.Quality = Quality(*selected); result.FinalPlayerHp = selected->Facts.PlayerHp;
        result.FinalPlayerMaxHp = selected->Facts.PlayerMaxHp;
        for (auto path = selected->Route; path; path = path->Parent) result.Actions.push_back(path->Step);
        std::reverse(result.Actions.begin(), result.Actions.end());
        // Replay the selected path from the immutable root, including every
        // Answer. This verifies that retention and metadata did not mix branches.
        auto replay = root_.Fork();
        for (auto action : result.Actions) { replay = replay.Predict(action); ++metrics_.RouteReplayTransitions; }
        if (replay.NativeHash() != result.FinalSearchHash) throw std::runtime_error("CombatSolver selected route replay mismatch");
        // Include request-cache teardown in measured latency. The selected
        // route and every live branch own their mutable state independently.
        successors_.clear(); boards_.clear(); forecasts_.clear(); table_.clear();
        metrics_.ElapsedMilliseconds = Elapsed();
        if (metrics_.StopReason.empty()) metrics_.StopReason = "frontier_exhausted";
        result.Metrics = metrics_;
        return result;
    }

private:
    double Elapsed() const { return std::chrono::duration<double, std::milli>(Clock::now() - start_).count(); }
    bool RequestStopped() {
        // User callbacks need not be thread safe. The coordinator serializes
        // cancellation checks while workers use the same request clock.
        auto& host = donor_ ? *donor_ : *this;
        if (host.config_.Cancelled) {
            std::lock_guard lock(host.cancelMutex_);
            if (host.cancelled_ || (host.config_.Cancelled && host.config_.Cancelled())) {
                host.cancelled_ = true; metrics_.StopReason = "cancelled"; return true;
            }
        }
        if (metrics_.ExpandedNodes >= config_.Profile.MaxExpandedNodes) { metrics_.StopReason = "node_budget"; return true; }
        if (Elapsed() >= config_.Profile.SoftTimeBudgetMilliseconds) { metrics_.StopReason = "time_budget"; return true; }
        return false;
    }
    bool PassStopped(int64_t nodes, int64_t ms) { return RequestStopped() || metrics_.ExpandedNodes >= nodes || Elapsed() >= ms; }

    Board CaptureBoard(const CombatSearchState& state) const {
        PhaseTimer timer(config_.CollectPhaseTimings, metrics_.BoardMilliseconds);
        const auto key = config_.EvaluationCacheEntries ? state.NativeHash() : 0;
        if (config_.EvaluationCacheEntries) {
            if (donor_) if (auto found = donor_->boards_.find(key); found != donor_->boards_.end()) {
                ++metrics_.BoardCacheHits;
                return found->second;
            }
            if (auto found = boards_.find(key); found != boards_.end()) {
                ++metrics_.BoardCacheHits;
                return found->second;
            }
        }
        ++metrics_.BoardCaptures;
        auto owned = state.CloneEnv();
        Player player(&owned->sim(), 1);
        Board result;
        // A diversity bucket, never a correctness/transposition key. Keep two
        // variants because identity, history, and pile order can matter to rules.
        // Full Env hashes still govern exact pruning and route replay.
        StateHasher retention;
        const auto facts = state.Facts();
        retention(facts.PlayerTurn); retention(facts.Round); retention(facts.Energy);
        retention(facts.Outcome);
        const auto& g = owned->state();
        retention(g.run.rng); retention(g.combat.encounter_rng);
        auto creatureKey = [&](Creature creature) {
            auto value = creature.st();
            value.powers.clear();
            retention(value);
            for (auto power : creature.Powers()) retention(power.st());
        };
        creatureKey(player.Creature());
        retention(player.PlayerCombatState().Stars());
        for (auto relic : player.Relics()) retention(relic.st());
        for (auto potion : player.Potions()) retention(potion.st());
        int vulnerable = 0, focusVulnerable = 0, attackWindow = 0;
        for (auto enemy : CombatState(&owned->sim()).Enemies()) {
            creatureKey(enemy);
            result.EnemyHp += std::max(0, enemy.CurrentHp());
            if (enemy.IsDead() && state.Outcome() == CombatOutcome::Ongoing && enemy.IsPrimaryEnemy())
                result.EnemyHp += std::max(0, enemy.MaxHp()); // a still-attached revive window is not a free kill
            for (auto power : enemy.Powers())
                if (power.PowerId_() == PowerId::POISON_POWER || power.PowerId_() == PowerId::DOOM_POWER)
                    result.DelayedDamage += std::max(0, power.Amount());
            if (enemy.IsAlive()) {
                const auto turns = std::max(0, enemy.GetPowerAmount(PowerId::VULNERABLE_POWER));
                vulnerable += turns;
                int64_t incoming = 0;
                for (const auto& intent : enemy.Monster().NextMove().Intents())
                    if (AttackIntent::IsAttack(intent)) incoming += static_cast<int64_t>(
                        AttackIntent::GetSingleDamage(intent, {player.Creature()}, enemy))
                        * AttackIntent::Repeats(intent, enemy.Monster());
                const int64_t pressure = static_cast<int64_t>(std::max(0, enemy.MaxHp() - enemy.CurrentHp()))
                    * (16 + std::min<int64_t>(64, incoming)) * 1024 / std::max(1, enemy.MaxHp());
                if (pressure > result.FocusPressure) { result.FocusPressure = pressure; focusVulnerable = turns; }
            }
        }
        for (auto power : player.Creature().Powers())
            if (power.TypeForCurrentAmount() == PowerType::Buff && !power.IsTemporaryPower())
                result.PersistentBuff += std::max(0, power.Amount());
        for (auto pile : player.PlayerCombatState().AllPiles()) {
            std::vector<uint64_t> cards;
            for (auto card : pile.Cards()) {
                cards.push_back(CardBucket(card));
                if (pile.Type() == PileType::Hand) result.HandCardKeys.push_back(cards.back());
                if (pile.Type() != PileType::Exhaust && card.Type() == CardType::Attack) {
                    auto vars = card.DynamicVars();
                    attackWindow += vars.ContainsKey("Damage") ? std::max(1, vars.Damage().IntValue()) : 1;
                }
            }
            // Draw order controls future information. Other piles receive a
            // bounded heuristic quota, with physical identities kept in Env.
            if (pile.Type() != PileType::Draw) std::sort(cards.begin(), cards.end());
            retention(pile.Type()); retention(static_cast<uint32_t>(cards.size()));
            for (auto card : cards) retention(card);
            if (pile.Type() == PileType::Exhaust) continue;
            for (auto card : pile.Cards()) if (card.Type() == CardType::Status || card.Type() == CardType::Curse) ++result.Clutter;
        }
        int fairies = 0;
        for (auto potion : player.Potions()) if (potion.PotionId_() == PotionId::FAIRY_IN_A_BOTTLE) ++fairies;
        result.DeathSaves = std::max(0, initialFairies_ - fairies);
        if (initialTailReady_) if (auto tail = player.GetRelic(RelicId::LIZARD_TAIL); tail && tail.IsUsedUp()) ++result.DeathSaves;
        // Final ordering prioritizes the real consumed resource count. This
        // estimate is used only in intermediate ranking until heal tracing lands.
        result.DeathSaveHp = result.DeathSaves * std::max(1, state.Facts().PlayerMaxHp / 2);
        attackWindow = std::min(attackWindow, SolverWeights::VulnerableAttackWindowCap);
        result.VulnerableValue = static_cast<double>(focusVulnerable) * attackWindow
            * SolverWeights::VulnerableAttackMultiplierBeamValue
            + static_cast<double>(std::max(0, vulnerable - focusVulnerable)) * attackWindow
            * SolverWeights::OffTargetVulnerableAttackMultiplierBeamValue;
        result.RetentionKey = retention.finish();
        if (boards_.size() < static_cast<size_t>(config_.EvaluationCacheEntries)) boards_.emplace(key, result);
        return result;
    }
    std::unique_ptr<Node> MakeRoot() const {
        auto node = std::make_unique<Node>(root_.Fork());
        node->Features = CaptureBoard(node->State);
        return node;
    }

    CombatSearchState Predict(const CombatSearchState& state, Action action, bool store = true,
        const CombatSearchState* prepared = nullptr) {
        const EdgeKey key{state.NativeHash(), action};
        // The coordinator does not modify its caches while joining a batch.
        // Cache entries are immutable inputs; Fork deep-copies every mutable
        // Env continuation before a worker executes it.
        if (donor_) if (auto found = donor_->successors_.find(key); found != donor_->successors_.end()) {
            ++metrics_.TransitionCacheHits;
            return found->second.Fork();
        }
        if (config_.TransitionCacheEntries) {
            if (auto found = successors_.find(key); found != successors_.end()) {
                ++metrics_.TransitionCacheHits;
                if (store) found->second.NativeHash();
                return found->second.Fork();
            }
        }
        auto successor = prepared ? prepared->Fork() : state.Predict(action);
        if (prepared) ++metrics_.EndTurnReused;
        else ++metrics_.NativePredictions;
        if (store && successor.DecisionPoint().kind == DecisionKind::Combat
            && successors_.size() < static_cast<size_t>(config_.TransitionCacheEntries)) {
            // Only completed ordinary continuations enter the state cache:
            // Clone drops their obsolete replay base. Choice/outcome states
            // retain exact value caches but execute without state caching.
            const size_t bytes = successor.CloneStorageEstimate() + sizeof(EdgeKey) + 64;
            if (bytes > config_.TransitionCacheBytes - std::min(config_.TransitionCacheBytes,
                static_cast<size_t>(metrics_.TransitionCacheEstimatedBytes))) return successor;
            // Each returned branch owns its mutable data. Cache the complete
            // continuation (including replay base/choices) and fork on reuse.
            successor.NativeHash();
            successors_.emplace(key, successor.Fork());
            ++metrics_.TransitionCacheStored;
            metrics_.TransitionCacheEstimatedBytes += bytes;
        }
        return successor;
    }

    std::unique_ptr<Node> Transition(const Node& parent, Action action) {
        PhaseTimer timer(config_.CollectPhaseTimings, metrics_.TransitionMilliseconds);
        auto node = std::make_unique<Node>(Predict(parent.State, action, true,
            action.kind == Action::EndTurn ? parent.EndTurn.get() : nullptr));
        ++metrics_.Transitions;
        if (action.kind == Action::Answer) ++metrics_.ChoiceAnswers;
        node->Route = std::make_shared<Path>(Path{parent.Route, action, parent.ActionCount() + 1});
        node->Potions = parent.Potions; node->PotionCost = parent.PotionCost; node->Ambergris = parent.Ambergris;
        node->HpLost = parent.HpLost + std::max(0, parent.Facts.PlayerHp - node->Facts.PlayerHp);
        node->HpRecovered = parent.HpRecovered + std::max(0, node->Facts.PlayerHp - parent.Facts.PlayerHp);
        node->LastTarget = action.target >= 0 ? action.target : parent.LastTarget;
        if (action.kind == Action::UsePotion || action.kind == Action::DiscardPotion) {
            auto before = parent.State.CloneEnv();
            auto potion = Player(&before->sim(), 1).GetPotionAtSlotIndex(action.index);
            node->PotionCost += PotionCost(potion);
            if (action.kind == Action::UsePotion) {
                ++node->Potions;
                if (potion.PotionId_() == PotionId::AMBERGRIS) ++node->Ambergris;
            }
        }
        return node;
    }

    Nodes ResolveChoices(std::unique_ptr<Node> node, int64_t nodeCap, int64_t timeCap) {
        Nodes pending, leaves;
        pending.push_back(std::move(node));
        while (!pending.empty() && !PassStopped(nodeCap, timeCap)) {
            auto current = std::move(pending.back()); pending.pop_back();
            if (current->State.DecisionPoint().kind != DecisionKind::Choice) {
                leaves.push_back(std::move(current));
                if (leaves.size() >= static_cast<size_t>(config_.MaxChoiceLeavesPerAction)) {
                    metrics_.ChoiceTruncated += pending.size(); break;
                }
                continue;
            }
            if (current->ActionCount() > 4096) throw std::runtime_error("CombatSolver choice prefix exceeded 4096 actions");
            ++metrics_.ExpandedNodes;
            auto actions = current->State.LegalActions();
            if (actions.empty()) throw std::runtime_error("CombatSolver selection has no legal answer");
            // LIFO reversal preserves the Env's original answer order.
            const auto& request = current->State.DecisionPoint().request;
            const auto profileLimit = request.source.find("Hand") != std::string::npos
                ? config_.Profile.MaxHandChoiceBranchesPerAction : config_.Profile.MaxPileChoiceBranchesPerAction;
            const size_t count = std::min(actions.size(), static_cast<size_t>(std::min(profileLimit, config_.MaxChoiceLeavesPerAction)));
            metrics_.ChoiceTruncated += actions.size() - count;
            for (size_t i = count; i > 0; --i) pending.push_back(Transition(*current, actions[i - 1]));
        }
        return leaves;
    }

    Forecast StandPat(Node& node) {
        if (node.Threat) return *node.Threat;
        if (node.Facts.Outcome != CombatOutcome::Ongoing)
            return {node.Facts.PlayerHp, node.Facts.Outcome == CombatOutcome::Defeat, false};
        PhaseTimer timer(config_.CollectPhaseTimings, metrics_.ForecastMilliseconds);
        const auto key = config_.EvaluationCacheEntries ? node.State.NativeHash() : 0;
        if (config_.EvaluationCacheEntries) {
            if (donor_) if (auto found = donor_->forecasts_.find(key); found != donor_->forecasts_.end()) {
                ++metrics_.ForecastCacheHits;
                node.Threat = found->second;
                return *node.Threat;
            }
            if (auto found = forecasts_.find(key); found != forecasts_.end()) {
                ++metrics_.ForecastCacheHits;
                node.Threat = found->second;
                return *node.Threat;
            }
        }
        auto end = Predict(node.State, {.kind = Action::EndTurn}, false);
        ++metrics_.Transitions; ++metrics_.ForecastTransitions;
        std::vector<CombatSearchState> pending;
        pending.push_back(std::move(end));
        int32_t worstHp = INT32_MAX;
        bool dies = false, unresolved = false;
        int leaves = 0, answers = 0;
        while (!pending.empty()) {
            auto current = std::move(pending.back()); pending.pop_back();
            if (current.DecisionPoint().kind != DecisionKind::Choice) {
                worstHp = std::min(worstHp, current.Facts().PlayerHp);
                dies |= current.Outcome() == CombatOutcome::Defeat;
                // The unbranched full EndTurn successor is already computed.
                // Retained nodes can use an independent fork as their real
                // EndTurn edge; choices still expand from their exact prefix.
                if (answers == 0 && config_.ReuseEndTurnForecast)
                    node.EndTurn = std::make_unique<CombatSearchState>(std::move(current));
                if (++leaves >= config_.ForecastChoiceLeaves) { unresolved |= !pending.empty(); break; }
            } else {
                auto actions = current.LegalActions();
                if (actions.empty()) throw std::runtime_error("CombatSolver forecast choice has no legal answer");
                if (++answers > 64) { unresolved = true; break; }
                const size_t count = std::min(actions.size(), static_cast<size_t>(config_.ForecastChoiceLeaves));
                unresolved |= count != actions.size();
                for (size_t i = count; i > 0; --i) {
                    pending.push_back(Predict(current, actions[i - 1], false));
                    ++metrics_.Transitions; ++metrics_.ForecastTransitions; ++metrics_.ChoiceAnswers;
                }
            }
        }
        if (worstHp == INT32_MAX) { worstHp = node.Facts.PlayerHp; unresolved = true; }
        node.Threat = Forecast{worstHp, dies, unresolved};
        if (forecasts_.size() < static_cast<size_t>(config_.EvaluationCacheEntries)) forecasts_.emplace(key, *node.Threat);
        return *node.Threat;
    }

    void Score(Node& node) {
        node.Features = CaptureBoard(node.State);
        const auto threat = StandPat(node);
        node.Score = node.Facts.Outcome == CombatOutcome::Defeat || threat.Dies || threat.Hp <= 0
            ? SolverWeights::DeathPenalty : threat.Hp * SolverWeights::Hp;
        node.Score += (node.Facts.PlayerMaxHp - initial_.PlayerMaxHp) * SolverWeights::Hp;
        node.Score -= node.HpLost * SolverWeights::Hp;
        node.Score -= ActEndingBossPolicy::DeathSaveBeamCost(node.Features.DeathSaveHp) * SolverWeights::Hp;
        if (node.Facts.Outcome == CombatOutcome::Victory) node.Score += SolverWeights::VictoryBonus;
        node.Score += node.Features.EnemyHp * SolverWeights::EnemyHp;
        node.Score += node.Features.Clutter * SolverWeights::LiveDeckClutterPenalty;
        node.Score += node.Features.DelayedDamage * SolverWeights::DelayedDamageBeamValue;
        node.Score += node.Features.VulnerableValue;
        node.Score += std::min(SolverWeights::CurrentEnergyBeamCap, std::max(0, node.Facts.Energy))
            * SolverWeights::CurrentEnergyBeamValue;
        node.Score += std::clamp(node.Features.PersistentBuff - initialBuff_, 0,
            SolverWeights::StandardPersistentBuffDeltaBeamCap) * SolverWeights::StandardPersistentBuffDeltaBeamValue;
        node.Score += node.ActionCount() * SolverWeights::ActionPenalty;
        if (threat.UnresolvedChoice) node.Score += SolverWeights::RiskPenalty;
    }

    RouteQuality Quality(const Node& node) const {
        const bool won = node.Facts.Outcome == CombatOutcome::Victory;
        const auto threat = node.Threat.value_or(Forecast{node.Facts.PlayerHp,
            node.Facts.Outcome == CombatOutcome::Defeat, false});
        const int heal = ActEndingBossPolicy::RankedPostCombatRelicHeal(postCombatHeal_, won,
            node.Facts.PlayerHp, node.Facts.PlayerMaxHp);
        const int deficit = ActEndingBossPolicy::StrategicHpDeficit(node.HpLost,
            std::max(0, initial_.PlayerMaxHp - node.Facts.PlayerMaxHp), node.HpRecovered + heal,
            relief_, node.Features.DeathSaveHp);
        return {.Won = won, .Survives = node.Facts.Outcome != CombatOutcome::Defeat && !threat.Dies && threat.Hp > 0,
            .DeathSaveUseCount = node.Features.DeathSaves, .StrategicHpDeficit = deficit,
            .CombatEndedTurn = won ? std::optional(node.Facts.PlayerTurn) : std::nullopt,
            .ProjectedBattleHpLost = node.HpLost,
            .PotionStrategicCost = PotionUsePolicy::EffectiveStrategicHpCost(node.PotionCost, node.Ambergris, initial_.PlayerMaxHp),
            .ProjectedBattlePotionCount = node.Potions, .EnemyHp = node.Features.EnemyHp, .Score = node.Score};
    }
    bool Eligible(const Node& node, std::optional<RouteQuality> baseline, int32_t baselineHp) const {
        if (node.Potions == 0) return config_.PotionPolicy != SolverPotionPolicy::RequireAtLeastOne;
        const auto quality = Quality(node);
        if (!PotionUsePolicy::MeetsAmbergrisRestriction(baseline.has_value(), node.Ambergris,
            node.PotionCost, initial_.PlayerMaxHp, baselineHp, node.Facts.PlayerHp)) return false;
        if (config_.PotionPolicy == SolverPotionPolicy::Smart && baseline) {
            return (quality.Won && !baseline->Won) || PotionUsePolicy::HpSaved(baseline->StrategicHpDeficit,
                quality.StrategicHpDeficit) >= PotionUsePolicy::SmartRequiredHpSaved(quality.PotionStrategicCost, relief_);
        }
        return PotionUsePolicy::IsEligible(config_.PotionPolicy, node.Potions, quality.PotionStrategicCost,
            baseline && baseline->Won, baseline ? baseline->StrategicHpDeficit : initial_.PlayerHp,
            quality.Won || (baseline && baseline->Won), quality.Won, quality.StrategicHpDeficit);
    }

    static bool Rank(const std::unique_ptr<Node>& a, const std::unique_ptr<Node>& b) {
        if (a->Score != b->Score) return a->Score > b->Score;
        if (a->PotionCost != b->PotionCost) return a->PotionCost < b->PotionCost;
        return a->ActionCount() < b->ActionCount();
    }
    void Retain(Nodes& nodes) {
        PhaseTimer timer(config_.CollectPhaseTimings, metrics_.RetainMilliseconds);
        const size_t width = static_cast<size_t>(beamWidth_);
        if (nodes.empty()) return;
        std::stable_sort(nodes.begin(), nodes.end(), Rank);
        std::vector<size_t> selected;
        std::unordered_set<size_t> indices;
        std::unordered_set<int32_t> potions, targets;
        std::unordered_map<uint64_t, int32_t> buckets;
        auto select = [&](size_t i) {
            if (selected.size() >= width || indices.contains(i) || buckets[nodes[i]->Features.RetentionKey] >= 2) return;
            indices.insert(i); ++buckets[nodes[i]->Features.RetentionKey]; selected.push_back(i);
        };
        select(0);
        // Reserve an offensive lane so a small HP advantage cannot crowd every
        // faster kill out of the beam (especially scaling enemies).
        std::vector<size_t> offense(nodes.size());
        std::iota(offense.begin(), offense.end(), 0);
        std::stable_sort(offense.begin(), offense.end(), [&](size_t a, size_t b) {
            if (nodes[a]->Features.EnemyHp != nodes[b]->Features.EnemyHp)
                return nodes[a]->Features.EnemyHp < nodes[b]->Features.EnemyHp;
            return Rank(nodes[a], nodes[b]);
        });
        for (auto i : offense) {
            if (selected.size() >= std::max<size_t>(2, width / 8)) break;
            select(i);
        }
        std::stable_sort(offense.begin(), offense.end(), [&](size_t a, size_t b) {
            if (nodes[a]->Features.FocusPressure != nodes[b]->Features.FocusPressure)
                return nodes[a]->Features.FocusPressure > nodes[b]->Features.FocusPressure;
            return Rank(nodes[a], nodes[b]);
        });
        for (auto i : offense) {
            if (selected.size() >= std::max<size_t>(2, width / 4)) break;
            select(i);
        }
        // Keep actual resource/target alternatives within a bounded quota.
        for (size_t i = 0; i < nodes.size() && selected.size() < std::max<size_t>(1, width / 3); ++i) {
            if (potions.insert(nodes[i]->Potions).second || targets.insert(nodes[i]->LastTarget).second) select(i);
        }
        auto healthy = std::min_element(nodes.begin(), nodes.end(), [](const auto& a, const auto& b) {
            const int ah = a->Threat ? a->Threat->Hp : a->Facts.PlayerHp, bh = b->Threat ? b->Threat->Hp : b->Facts.PlayerHp;
            return ah != bh ? ah > bh : a->Score > b->Score;
        });
        select(static_cast<size_t>(healthy - nodes.begin()));
        for (size_t i = 0; i < nodes.size() && selected.size() < width; ++i) select(i);
        Nodes retained;
        for (auto i : selected) retained.push_back(std::move(nodes[i]));
        std::stable_sort(retained.begin(), retained.end(), Rank);
        nodes = std::move(retained);
    }

    std::unique_ptr<Node> Pass(bool allowPotions, int64_t nodeCap, int64_t timeCap,
        std::optional<RouteQuality> baseline = std::nullopt, int32_t baselineHp = 0) {
        ++metrics_.Passes;
        table_.clear();
        focusFilters_.clear();
        passCompleted_ = false;
        Nodes turns; turns.push_back(MakeRoot());
        std::unique_ptr<Node> best;
        auto consider = [&](std::unique_ptr<Node> node) {
            if (!EligibleForPass(*node, allowPotions, baseline, baselineHp)) return;
            if (!best || SolverInterimResultOrdering::IsBetter(Quality(*node), Quality(*best))) best = std::move(node);
        };
        for (int turn = 0; turn < config_.MaxTurns && !turns.empty() && !PassStopped(nodeCap, timeCap); ++turn) {
            metrics_.SearchedTurns = std::max(metrics_.SearchedTurns, turn + 1);
            Nodes nextTurns;
            for (int depth = 0; depth < config_.MaxActionsPerTurn && !turns.empty() && !PassStopped(nodeCap, timeCap); ++depth) {
                Nodes cards;
                for (const auto& parent : turns) {
                    if (PassStopped(nodeCap, timeCap)) break;
                    ++metrics_.ExpandedNodes;
                    std::vector<Action> actions;
                    {
                        PhaseTimer timer(config_.CollectPhaseTimings, metrics_.LegalActionsMilliseconds);
                        actions = parent->State.LegalActions();
                    }
                    // Focus changes only the action filter below. Record any
                    // target that could actually remove an action, including
                    // enemies introduced after the root. A completed normal
                    // pass with no such filter has exactly the same search as
                    // that focused member at this width and potion policy.
                    if (focusTarget_ < 0 && config_.SkipEquivalentFocusPasses) {
                        int32_t firstTarget = -1;
                        bool multipleTargets = false;
                        for (auto action : actions) {
                            if (action.kind != Action::PlayCard || action.target <= 0) continue;
                            if (firstTarget < 0) firstTarget = action.target;
                            else if (firstTarget != action.target) multipleTargets = true;
                        }
                        if (multipleTargets)
                            for (auto action : actions)
                                if (action.kind == Action::PlayCard && action.target > 0)
                                    focusFilters_.insert(action.target);
                    }
                    std::unordered_set<uint64_t> cardActions;
                    int cardBranches = 0;
                    // Independent focused members avoid losing every route
                    // that finishes one enemy before distributing damage.
                    const bool focused = focusTarget_ >= 0 && std::any_of(actions.begin(), actions.end(), [&](auto action) {
                        return action.kind == Action::PlayCard && action.target == focusTarget_;
                    });
                    for (auto action : actions) {
                        if (PassStopped(nodeCap, timeCap)) break;
                        if (!allowPotions && action.kind == Action::UsePotion) continue;
                        if (focused && action.kind == Action::PlayCard && action.target > 0 && action.target != focusTarget_) continue;
                        if (action.kind == Action::PlayCard) {
                            StateHasher key; key(parent->Features.HandCardKeys.at(action.index)); key(action.target);
                            // Approximate action symmetry: execute the first
                            // physical occurrence and preserve that exact Env
                            // branch. Never reuse another identity's successor.
                            if (!cardActions.insert(key.finish()).second) { ++metrics_.DuplicateActionsPruned; continue; }
                            if (++cardBranches > config_.Profile.MaxCardBranchesPerNode) { ++metrics_.ActionTruncated; continue; }
                        }
                        auto leaves = ResolveChoices(Transition(*parent, action), nodeCap, timeCap);
                        for (auto& child : leaves) {
                            if (config_.PruneBeforeScoring && child->Facts.Outcome == CombatOutcome::Ongoing) {
                                const auto found = table_.find(child->Hash);
                                const TranspositionLabel costs{child->Potions, child->PotionCost, 0,
                                    child->HpLost, child->ActionCount(), 0};
                                if (found != table_.end() && found->second.HasIdenticalCosts(costs)) {
                                    ++metrics_.TranspositionPruned; ++metrics_.EarlyTranspositionPruned;
                                    continue;
                                }
                            }
                            Score(*child);
                            if (child->Facts.Outcome != CombatOutcome::Ongoing) { consider(std::move(child)); continue; }
                            TranspositionLabel label{child->Potions, child->PotionCost, 0, child->HpLost, child->ActionCount(), child->Score};
                            auto found = table_.find(child->Hash);
                            if (found != table_.end() && !found->second.TryAccept(label)) { ++metrics_.TranspositionPruned; continue; }
                            if (found == table_.end() && table_.size() < 1000000) table_.emplace(child->Hash, TranspositionFrontier(label));
                            // Keep a real executable partial fallback without borrowing a branch.
                            if (EligibleForPass(*child, allowPotions, baseline, baselineHp)
                                && (!best || SolverInterimResultOrdering::IsBetter(Quality(*child), Quality(*best)))) {
                                auto fallback = std::make_unique<Node>(child->State.Fork());
                                fallback->Route = child->Route; fallback->Facts = child->Facts; fallback->Features = child->Features;
                                fallback->Potions = child->Potions; fallback->PotionCost = child->PotionCost; fallback->Ambergris = child->Ambergris;
                                fallback->HpLost = child->HpLost; fallback->HpRecovered = child->HpRecovered;
                                fallback->Score = child->Score; fallback->Threat = child->Threat;
                                consider(std::move(fallback));
                            }
                            if (child->Facts.PlayerTurn > parent->Facts.PlayerTurn) nextTurns.push_back(std::move(child));
                            else cards.push_back(std::move(child));
                        }
                        if (cards.size() > static_cast<size_t>(beamWidth_) * 8) Retain(cards);
                        if (nextTurns.size() > static_cast<size_t>(beamWidth_) * 8) Retain(nextTurns);
                    }
                }
                Retain(cards); turns = std::move(cards);
            }
            Retain(nextTurns); turns = std::move(nextTurns);
        }
        passCompleted_ = !PassStopped(nodeCap, timeCap);
        return best;
    }
    std::unique_ptr<Node> Portfolio(bool allowPotions, int64_t nodeCap, int64_t timeCap,
        std::optional<RouteQuality> baseline = std::nullopt, int32_t baselineHp = 0) {
        beamWidth_ = config_.Profile.BeamWidth;
        std::unique_ptr<Node> best;
        while (!PassStopped(nodeCap, timeCap)) {
            const auto before = best ? std::optional(Quality(*best)) : std::nullopt;
            std::vector<int32_t> members{-1};
            if (config_.Profile.ReallocatedRefinementPortfolio) members.insert(members.end(), focusTargets_.begin(), focusTargets_.end());
            bool normalCompleted = false;
            std::unordered_set<int32_t> normalFocusFilters;
            for (size_t member = 0; member < members.size(); ) {
                if (PassStopped(nodeCap, timeCap)) break;
                const auto target = members[member];
                if (target >= 0 && config_.SkipEquivalentFocusPasses && normalCompleted
                    && !normalFocusFilters.contains(target)) {
                    ++metrics_.EquivalentFocusPassesSkipped;
                    ++member;
                    continue;
                }
                if (target >= 0 && config_.ThreadCount > 1 && members.size() - member > 1) {
                    std::vector<int32_t> batch;
                    while (member < members.size() && batch.size() < static_cast<size_t>(config_.ThreadCount)) {
                        const auto next = members[member++];
                        if (config_.SkipEquivalentFocusPasses && normalCompleted && !normalFocusFilters.contains(next))
                            ++metrics_.EquivalentFocusPassesSkipped;
                        else batch.push_back(next);
                    }
                    if (batch.empty()) continue;
                    const int64_t remaining = nodeCap - metrics_.ExpandedNodes;
                    // Reservations are disjoint. Early completions return their
                    // unused budget to the next batch; concurrent work can never
                    // spend more than this request's remaining expansion cap.
                    const auto quota = std::min<int64_t>(remaining / batch.size(),
                        std::max<int64_t>(SolverWeights::MinimumTurnLayerExpandedNodes, remaining / 3));
                    if (quota == 0) break;
                    metrics_.PeakConcurrentPasses = std::max(metrics_.PeakConcurrentPasses, static_cast<int32_t>(batch.size()));
                    struct WorkerResult {
                        std::unique_ptr<Node> Candidate;
                        CombatSearchMetrics Metrics;
                        bool Completed;
                        std::unordered_map<uint64_t, Board> Boards;
                        std::unordered_map<uint64_t, Forecast> Forecasts;
                    };
                    auto source = root_.CloneEnv();
                    std::vector<std::future<WorkerResult>> futures;
                    const auto width = beamWidth_;
                    for (auto focus : batch) {
                        futures.push_back(std::async(std::launch::async,
                            [this, &source, focus, width, quota, allowPotions, timeCap, baseline, baselineHp,
                             count = batch.size()] {
                                auto config = config_;
                                config.ThreadCount = 1;
                                // The large owned-state cache remains request
                                // owned and read-only during this batch. Workers
                                // fork its values and only keep small value caches.
                                config.TransitionCacheEntries = 0;
                                config.EvaluationCacheEntries /= static_cast<int32_t>(count);
                                SearchRun worker(*source, config, this);
                                worker.beamWidth_ = width; worker.focusTarget_ = focus;
                                const auto began = Clock::now();
                                auto candidate = worker.Pass(allowPotions, quota, timeCap, baseline, baselineHp);
                                worker.metrics_.ElapsedMilliseconds = std::chrono::duration<double, std::milli>(Clock::now() - began).count();
                                return WorkerResult{std::move(candidate), std::move(worker.metrics_), worker.passCompleted_,
                                    std::move(worker.boards_), std::move(worker.forecasts_)};
                            }));
                    }
                    // Collect in target order, so scheduling does not decide
                    // which equal-quality route wins a tie.
                    std::vector<WorkerResult> results;
                    for (auto& future : futures) results.push_back(future.get());
                    for (size_t i = 0; i < results.size(); ++i) {
                        auto& result = results[i];
                        MergeWorkerMetrics(result.Metrics);
                        metrics_.PassDetails.push_back({width, batch[i], allowPotions, result.Completed,
                            result.Metrics.ExpandedNodes, result.Metrics.NativePredictions, result.Metrics.ElapsedMilliseconds,
                            result.Candidate ? std::optional(Quality(*result.Candidate)) : std::nullopt});
                        if (result.Candidate && (!best || SolverInterimResultOrdering::IsBetter(Quality(*result.Candidate), Quality(*best))))
                            best = std::move(result.Candidate);
                        // All workers have joined before the coordinator
                        // updates a cache they read. Preserve exact value work
                        // across later widths without sharing mutable branches.
                        for (auto& [key, value] : result.Boards) {
                            if (boards_.size() >= static_cast<size_t>(config_.EvaluationCacheEntries)) break;
                            boards_.try_emplace(key, std::move(value));
                        }
                        for (auto& [key, value] : result.Forecasts) {
                            if (forecasts_.size() >= static_cast<size_t>(config_.EvaluationCacheEntries)) break;
                            forecasts_.try_emplace(key, value);
                        }
                    }
                    if (best && config_.Profile.StopPortfolioAtHpTarget && Quality(*best).Won
                        && Quality(*best).StrategicHpDeficit <= 0) break;
                    continue;
                }
                ++member;
                focusTarget_ = target;
                const auto slice = std::min<int64_t>(nodeCap, metrics_.ExpandedNodes
                    + std::max<int64_t>(SolverWeights::MinimumTurnLayerExpandedNodes, (nodeCap - metrics_.ExpandedNodes) / 3));
                const auto began = Clock::now();
                const auto beforeNodes = metrics_.ExpandedNodes;
                const auto beforePredictions = metrics_.NativePredictions;
                auto candidate = Pass(allowPotions, slice, timeCap, baseline, baselineHp);
                metrics_.PassDetails.push_back({beamWidth_, target, allowPotions, passCompleted_,
                    metrics_.ExpandedNodes - beforeNodes, metrics_.NativePredictions - beforePredictions,
                    std::chrono::duration<double, std::milli>(Clock::now() - began).count(),
                    candidate ? std::optional(Quality(*candidate)) : std::nullopt});
                if (target < 0) {
                    normalCompleted = passCompleted_;
                    normalFocusFilters = focusFilters_;
                }
                if (candidate && (!best || SolverInterimResultOrdering::IsBetter(Quality(*candidate), Quality(*best))))
                    best = std::move(candidate);
                if (best && config_.Profile.StopPortfolioAtHpTarget && Quality(*best).Won
                    && Quality(*best).StrategicHpDeficit <= 0) break;
            }
            if (config_.StopAfterStableWidth && before && before->Won && best && !PassStopped(nodeCap, timeCap)) {
                auto previous = *before, current = Quality(*best);
                previous.Score = current.Score = 0;
                if (RouteQualityPolicy::Compare(current, previous, RouteQualityProjection::Primary) == 0) {
                    metrics_.StopReason = "quality_stable"; break;
                }
            }
            if (!config_.Profile.ReallocatedRefinementPortfolio || beamWidth_ >= SolverWeights::MaximumEscalatedBeamWidth
                || (best && config_.Profile.StopPortfolioAtHpTarget && Quality(*best).Won
                    && Quality(*best).StrategicHpDeficit <= 0)) break;
            beamWidth_ = std::min(SolverWeights::MaximumEscalatedBeamWidth, beamWidth_ * 2);
        }
        return best;
    }
    bool EligibleForPass(const Node& node, bool allowPotions, std::optional<RouteQuality> baseline, int32_t baselineHp) const {
        return !allowPotions || Eligible(node, baseline, baselineHp);
    }
    void MergeWorkerMetrics(const CombatSearchMetrics& other) {
        for (auto field : {&CombatSearchMetrics::ExpandedNodes, &CombatSearchMetrics::Transitions,
            &CombatSearchMetrics::ForecastTransitions, &CombatSearchMetrics::ChoiceAnswers,
            &CombatSearchMetrics::TranspositionPruned, &CombatSearchMetrics::EarlyTranspositionPruned,
            &CombatSearchMetrics::DuplicateActionsPruned, &CombatSearchMetrics::ActionTruncated,
            &CombatSearchMetrics::ChoiceTruncated, &CombatSearchMetrics::BoardCaptures,
            &CombatSearchMetrics::BoardCacheHits, &CombatSearchMetrics::ForecastCacheHits,
            &CombatSearchMetrics::TransitionCacheHits, &CombatSearchMetrics::NativePredictions,
            &CombatSearchMetrics::EndTurnReused}) metrics_.*field += other.*field;
        metrics_.Passes += other.Passes;
        metrics_.SearchedTurns = std::max(metrics_.SearchedTurns, other.SearchedTurns);
        for (auto field : {&CombatSearchMetrics::BoardMilliseconds, &CombatSearchMetrics::TransitionMilliseconds,
            &CombatSearchMetrics::ForecastMilliseconds, &CombatSearchMetrics::LegalActionsMilliseconds,
            &CombatSearchMetrics::RetainMilliseconds}) metrics_.*field += other.*field;
        if (other.StopReason == "cancelled" || other.StopReason == "time_budget") metrics_.StopReason = other.StopReason;
    }

    const CombatSearchConfig& config_;
    Clock::time_point start_;
    CombatSearchState root_;
    CombatFacts initial_;
    mutable CombatSearchMetrics metrics_;
    SearchRun* donor_ = nullptr;
    std::mutex cancelMutex_;
    bool cancelled_ = false;
    std::unordered_map<uint64_t, TranspositionFrontier> table_;
    std::unordered_set<int32_t> focusFilters_;
    bool passCompleted_ = false;
    mutable std::unordered_map<uint64_t, Board> boards_;
    std::unordered_map<uint64_t, Forecast> forecasts_;
    std::unordered_map<EdgeKey, CombatSearchState, EdgeHash> successors_;
    PostCombatRelicHealProfile postCombatHeal_;
    BossHpRelief relief_ = BossHpRelief::None;
    int32_t initialBuff_ = 0;
    int32_t initialFairies_ = 0;
    bool initialTailReady_ = false;
    bool hasPotions_ = false;
    int32_t beamWidth_ = 0;
    int32_t focusTarget_ = -1;
    std::vector<int32_t> focusTargets_;
};
}  // namespace

CombatSearchResult CombatSolver::Solve(const Env& root, const CombatSearchConfig& config) {
    return SearchRun(root, config).Solve();
}
}  // namespace sts2::combatsolver
