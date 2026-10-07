// See env.h.
#include "env/env.h"

#include <algorithm>
#include <cstring>

#include "core/xxhash64.h"
#include "engine/json.h"
#include "engine/sim/gc.h"
#include "engine/sim/state_hash.h"
#include "game/All.h"
#include "game/Runs/RunFlow.h"
#include "game/Runs/RunManager.h"

namespace sts2 {

Env::Env() : g_(std::make_unique<GameState>()), sim_(std::make_unique<Sim>(*g_)) {}
Env::Env(const Env& source)
    : g_(std::make_unique<GameState>(*source.g_)),
      // Completed operations have no continuation: their old stationary
      // snapshot is absent from Hash/exports and the next action replaces it.
      // A choice, execution boundary or in-flight operation must retain it.
      base_((source.pending_ || source.pending_operation_ || source.choice_ || source.boundary_)
          ? source.base_ : nullptr),
      sim_(std::make_unique<Sim>(*g_)),
      pending_(source.pending_), pending_operation_(source.pending_operation_),
      last_operation_result_(source.last_operation_result_),
      boundary_(source.boundary_), boundary_skipped_(source.boundary_skipped_),
      answers_(source.answers_), choice_(source.choice_) {}
Env::~Env() = default;

void Env::Reset(const NewRunOptions& options) {
    last_operation_result_ = {};
    pending_.reset(); pending_operation_.reset(); choice_.reset(); boundary_.reset();
    answers_.clear(); boundary_skipped_ = 0;
    new_run(*g_, options);
    Step(Action{.kind = Action::StartRun});
}

void Env::ResetCombat(const NewRunOptions& options, EncounterId encounter, int32_t act_index) {
    if (encounter == EncounterId::NONE || static_cast<size_t>(encounter) >= kEncounterIdNames.size())
        throw InvalidRequest("Env::ResetCombat: encounter ID out of range");
    if (act_index < 0 || act_index >= 3) throw InvalidRequest("Env::ResetCombat: act index out of range");
    new_run(*g_, options);
    last_operation_result_ = {};
    if (act_index < 0 || act_index >= static_cast<int32_t>(g_->run.acts.size()))
        throw GameError("Env::ResetCombat: act index out of range");
    pending_.reset();
    pending_operation_.reset();
    boundary_.reset(); boundary_skipped_ = 0;
    choice_.reset();
    answers_.clear();
    RunManager::SetActInternal(sim_.get(), act_index);
    RunManager::EnterMapCoordDebug(sim_.get(), MapCoord{0, 0}, encounter);
    g_->run.phase = RunPhase::Combat;
    base_ = std::make_shared<GameState>(*g_);
}

Decision Env::decision() const {
    Decision d;
    if (boundary_) { d.kind = DecisionKind::Boundary; d.boundary = boundary_; return d; }
    if (choice_) {
        d.kind = DecisionKind::Choice;
        d.request = *choice_;
        return d;
    }
    switch (g_->run.phase) {
    case RunPhase::Map: d.kind = DecisionKind::Map; break;
    case RunPhase::Combat: d.kind = DecisionKind::Combat; break;
    case RunPhase::Rewards: d.kind = DecisionKind::Rewards; break;
    case RunPhase::Event: d.kind = DecisionKind::Event; break;
    case RunPhase::RestSite: d.kind = DecisionKind::RestSite; break;
    case RunPhase::Shop: d.kind = DecisionKind::Shop; break;
    case RunPhase::Treasure: d.kind = DecisionKind::Treasure; break;
    case RunPhase::Ended: d.kind = DecisionKind::Ended; break;
    default: throw GameError("the run waits in a phase the environment does not handle yet");
    }
    return d;
}

void Env::ValidateCombatRoot() const {
    if (!g_->combat.is_in_progress || !g_->combat.exists || !g_->player_combat.exists
        || (decision().kind != DecisionKind::Combat && decision().kind != DecisionKind::Choice
            && decision().kind != DecisionKind::Boundary))
        throw GameError("combat root coverage requires an active combat decision or choice");
    Sim* sim = sim_.get();
    auto check = [](ModelKind kind, uint16_t id, std::string_view name) {
        if (!IsUsable(kind, id))
            throw Unported("combat root contains unported model " + std::string(name));
    };
    check(ModelKind::Encounter, static_cast<uint16_t>(g_->combat.encounter), name(g_->combat.encounter));
    Player player(sim, 1);
    auto check_card = [&](CardModel card) {
        check(ModelKind::Card, static_cast<uint16_t>(card.CardId_()), name(card.CardId_()));
        if (EnchantmentModel enchantment = card.Enchantment())
            check(ModelKind::Enchantment, static_cast<uint16_t>(enchantment.EnchantmentId_()),
                  name(enchantment.EnchantmentId_()));
        if (AfflictionModel affliction = card.Affliction())
            check(ModelKind::Affliction, static_cast<uint16_t>(affliction.AfflictionId_()),
                  name(affliction.AfflictionId_()));
    };
    for (CardModel card : player.Deck().Cards()) check_card(card);
    PlayerCombatState combat = player.PlayerCombatState();
    for (CardPile pile : combat.AllPiles())
        for (CardModel card : pile.Cards()) check_card(card);
    for (RelicModel relic : player.Relics())
        check(ModelKind::Relic, static_cast<uint16_t>(relic.RelicId_()), name(relic.RelicId_()));
    for (PotionModel potion : player.Potions())
        check(ModelKind::Potion, static_cast<uint16_t>(potion.PotionId_()), name(potion.PotionId_()));
    for (Creature creature : CombatState(sim).Creatures()) {
        if (MonsterModel monster = creature.Monster())
            check(ModelKind::Monster, static_cast<uint16_t>(monster.MonsterId_()), name(monster.MonsterId_()));
        for (PowerModel power : creature.Powers())
            check(ModelKind::Power, static_cast<uint16_t>(power.PowerId_()), name(power.PowerId_()));
    }
    for (OrbRef orb : combat.st().orbs) {
        OrbId id = g_->orbs[orb].id;
        check(ModelKind::Orb, static_cast<uint16_t>(id), name(id));
    }
}

void Env::Step(const Action& action) {
    if (!g_->run.profile.full_veteran_compatible || !g_->run.player.profile.full_veteran_compatible)
        throw Unported("live profile actions require epoch, encounter and first-run rules");
    bool replay = action.kind == Action::Answer || action.kind == Action::Resume;
    if (action.kind != Action::StartRun) {
        const auto legal = LegalActions();
        if (std::find(legal.begin(), legal.end(), action) == legal.end())
            throw InvalidRequest("Env::Step: action is not legal at this decision");
    } else if (g_->run.phase != RunPhase::None) {
        throw InvalidRequest("Env::Step: run already started");
    }
    if (replay) {
        if (action.kind == Action::Answer) {
            if (!choice_) throw GameError("Env::Step: no choice to answer");
            answers_.push_back(action.index);
        } else {
            if (!boundary_) throw GameError("Env::Step: no boundary to resume");
            boundary_skipped_ = boundary_->ordinal;
        }
    } else {
        if (choice_) throw GameError("Env::Step: a choice is pending");
        base_ = std::make_shared<GameState>(*g_);
        pending_ = action;
        pending_operation_.reset();
        boundary_.reset(); boundary_skipped_ = 0;
        answers_.clear();
    }
    Run(replay);
}

void Env::Execute(const Operation& operation) {
    ValidateOperation(*this, operation);
    // Answer/Resume extend the suspended root rather than replacing its source
    // identity or stop policy. New typed actions retain their request even when
    // no execution boundary was requested, so nested choices can expose it.
    if (operation.kind == Operation::Kind::Action
        && (operation.action.kind == Action::Answer || operation.action.kind == Action::Resume)) {
        Step(operation.action);
        return;
    }
    if (!g_->run.profile.full_veteran_compatible || !g_->run.player.profile.full_veteran_compatible)
        throw Unported("live profile operations require epoch, encounter and first-run rules");
    if (operation.kind == Operation::Kind::GenerateRewards) {
        if (!pending_operation_ || !boundary_) throw GameError("Env::Execute: missing outcome continuation");
        pending_operation_->stop_mask = 0;
        boundary_.reset(); boundary_skipped_ = 0;
        Run(true);
        return;
    }
    base_ = std::make_shared<GameState>(*g_);
    pending_.reset();
    pending_operation_ = operation;
    boundary_.reset(); boundary_skipped_ = 0;
    answers_.clear();
    Run(false);
}

std::unique_ptr<Env> Env::PredictOperation(const Operation& operation) const {
    if (g_->combat.is_in_progress) ValidateCombatRoot();
    auto branch = Clone();
    branch->Execute(operation);
    if (branch->g_->combat.is_in_progress) branch->ValidateCombatRoot();
    return branch;
}

void Env::Run(bool replay) {
    if (!g_->run.profile.full_veteran_compatible || !g_->run.player.profile.full_veteran_compatible)
        throw Unported("live profile actions require epoch, encounter and first-run rules");
    if (!base_) throw GameError("Env::Run: no top-level decision snapshot");
    if (replay) *g_ = *base_;
    Sim& sim = *sim_;
    sim.set_prefix(answers_);
    sim.ConfigureExecutionBoundaries(pending_operation_ ? pending_operation_->stop_mask : 0, boundary_skipped_);
    struct BoundaryScope { Sim& sim; ~BoundaryScope() { sim.ConfigureExecutionBoundaries(0, 0); } } boundary_scope{sim};
    choice_.reset(); boundary_.reset();
    last_operation_result_ = {};
    try {
        if (pending_operation_ && pending_operation_->kind != Operation::Kind::Action)
            last_operation_result_ = ExecuteOperation(sim, *pending_operation_);
        else {
        if (!pending_ && !pending_operation_) throw GameError("Env::Run: no pending operation");
        const Action& a = pending_operation_ ? pending_operation_->action : *pending_;
        switch (a.kind) {
        case Action::StartRun: RunFlow::StartRun(&sim); break;
        case Action::MapMove: RunFlow::ChooseMapPoint(&sim, a.coord); break;
        case Action::PlayCard: RunFlow::PlayCard(&sim, a.index, a.target); break;
        case Action::EndTurn: RunFlow::EndTurn(&sim); break;
        case Action::UsePotion: RunFlow::UsePotion(&sim, a.index, a.target); break;
        case Action::DiscardPotion: RunFlow::DiscardPotion(&sim, a.index); break;
        case Action::TakeReward: RunFlow::TakeReward(&sim, a.index); break;
        case Action::Proceed:
            switch (sim.g->run.phase) {
            case RunPhase::Rewards: RunFlow::ProceedFromRewards(&sim); break;
            case RunPhase::RestSite: RunFlow::ProceedFromRestSite(&sim); break;
            case RunPhase::Shop: RunFlow::LeaveShop(&sim); break;
            case RunPhase::Treasure: RunFlow::PickTreasureRelic(&sim, -1); break;
            default: throw GameError("Env: nothing to proceed from");
            }
            break;
        case Action::EventOption: RunFlow::ChooseEventOption(&sim, a.index); break;
        case Action::RestOption: RunFlow::ChooseRestSiteOption(&sim, a.index); break;
        case Action::ShopBuy: RunFlow::BuyShopOption(&sim, a.index); break;
        case Action::TreasurePick: RunFlow::PickTreasureRelic(&sim, a.index); break;
        default: throw GameError("Env: bad action");
        }
        }
        RunFlow::SettleGameOver(&sim);
        if (!sim.prefix_consumed()) throw GameError("Env: the action did not ask for every given answer");
        CollectDetachedCreatures(*g_);
        pending_.reset();
        pending_operation_.reset();
        choice_.reset();
        answers_.clear();
        boundary_skipped_ = 0;
    } catch (const NeedChoice& need) {
        choice_ = need.request;
    } catch (const NeedBoundary& need) {
        if (!sim.prefix_consumed()) throw GameError("Env: boundary precedes previously supplied answers");
        boundary_ = need;
    }
}

namespace {

void add_hash(uint64_t& h, uint64_t value) { h = xxhash64(&value, sizeof(value), h); }

void add_hash(uint64_t& h, std::span<const int32_t> values) {
    if (!values.empty()) h = xxhash64(values.data(), values.size_bytes(), h);
    add_hash(h, static_cast<uint64_t>(values.size()));
}

void add_hash(uint64_t& h, std::string_view value) {
    if (!value.empty()) h = xxhash64(value.data(), value.size(), h);
    add_hash(h, static_cast<uint64_t>(value.size()));
}

std::vector<Action> choice_actions(const ChoiceRequest& request) {
    std::vector<Action> out;
    if (request.subset && request.prompt == "count") {
        int32_t lo = std::max(0, request.min_select);
        int32_t hi = std::min(request.max_select, request.option_count);
        for (int32_t i = lo; i <= hi; ++i) out.push_back(Action{.kind = Action::Answer, .index = i});
        return out;
    }
    for (int32_t i = 0; i < request.option_count; ++i) {
        if (request.subset && std::find(request.selected.begin(), request.selected.end(), i) != request.selected.end()) continue;
        if (static_cast<size_t>(i) < request.options.size() && !request.options[static_cast<size_t>(i)].enabled) continue;
        out.push_back(Action{.kind = Action::Answer, .index = i});
    }
    return out;
}

bool potion_target_allowed(PotionModel potion, Creature target) {
    Player owner = potion.Owner();
    switch (potion.TargetType()) {
    case TargetType::TargetedNoCreature:
    case TargetType::AllEnemies:
    case TargetType::AllAllies:
    case TargetType::RandomEnemy:
        return !target;
    case TargetType::Self:
        return target && target.IsAlive() && target == owner.Creature();
    case TargetType::AnyPlayer:
        return target && target.IsAlive() && target.IsPlayer();
    case TargetType::AnyEnemy:
        return target && target.IsAlive() && target.IsEnemy();
    case TargetType::AnyAlly:
        return target && target.IsAlive() && target.Side() == owner.Creature().Side() && target != owner.Creature();
    case TargetType::Osty:
    default:
        return false;
    }
}

void append_potion_actions(std::vector<Action>& out, Sim* sim, bool in_combat) {
    Player player(sim, 1);
    if (!player.CanUseOrRemovePotions()) return;
    CombatState combat(sim);
    for (int32_t slot = 0; slot < player.MaxPotionCount(); ++slot) {
        PotionModel potion = player.GetPotionAtSlotIndex(slot);
        if (!potion || !IsUsable(ModelKind::Potion, potion.DenseId())) continue;
        // NPotionPopup disables Use for automatic potions, while Discard
        // remains enabled whenever CanUseOrRemovePotions allows it.
        bool can_use = potion.Usage() != PotionUsage::Automatic &&
                       (potion.Usage() != PotionUsage::CombatOnly || in_combat) &&
                       potion.PassesCustomUsabilityCheck();
        TargetType type = potion.TargetType();
        if (can_use && !in_combat) {
            Creature self = player.Creature();
            if (type == TargetType::Self || type == TargetType::AnyPlayer) {
                if (potion_target_allowed(potion, self)) out.push_back(Action{.kind = Action::UsePotion, .index = slot, .target = -1});
            } else if (potion_target_allowed(potion, Creature{})) {
                out.push_back(Action{.kind = Action::UsePotion, .index = slot, .target = -1});
            }
        } else if (can_use && (type == TargetType::AnyEnemy || type == TargetType::AnyAlly)) {
            for (Creature target : combat.Creatures()) {
                if (potion_target_allowed(potion, target)) {
                    out.push_back(Action{.kind = Action::UsePotion, .index = slot, .target = target.CombatId()});
                }
            }
        } else if (can_use && (type == TargetType::Self || type == TargetType::AnyPlayer)) {
            Creature self = player.Creature();
            if (potion_target_allowed(potion, self)) {
                out.push_back(Action{.kind = Action::UsePotion, .index = slot, .target = self.CombatId()});
            }
        } else if (can_use && potion_target_allowed(potion, Creature{})) {
            out.push_back(Action{.kind = Action::UsePotion, .index = slot, .target = -1});
        }
        out.push_back(Action{.kind = Action::DiscardPotion, .index = slot});
    }
}

}  // namespace

std::vector<Action> Env::LegalActions() const {
    if (!g_->run.profile.full_veteran_compatible || !g_->run.player.profile.full_veteran_compatible)
        throw Unported("live profile actions require epoch, encounter and first-run rules");
    if (boundary_) return {Action{.kind = Action::Resume}};
    if (choice_) {
        return choice_actions(*choice_);
    }
    std::vector<Action> out;
    // Native legality getters can materialize lazy costs. Keep this query and
    // Predict's validation pure while letting the executed action reproduce
    // the game's exact getter side effects on its owned successor.
    auto query_state = std::make_unique<GameState>(*g_);
    Sim query(*query_state);
    Sim* sim = &query;
    switch (decision().kind) {
    case DecisionKind::Map:
        append_potion_actions(out, sim, false);
        for (MapCoord c : RunFlow::MapOptions(sim)) out.push_back(Action{.kind = Action::MapMove, .coord = c});
        break;
    case DecisionKind::Combat: {
        append_potion_actions(out, sim, true);
        CombatState combat(sim);
        CardPile hand = Player(sim, 1).PlayerCombatState().Hand();
        for (size_t i = 0; i < hand.refs().size(); ++i) {
            CardModel card(sim, hand.refs()[i]);
            if (!card.CanPlay()) continue;
            TargetType type = card.TargetType();
            if (type == TargetType::AnyEnemy || type == TargetType::AnyAlly) {
                for (Creature target : combat.Creatures()) {
                    if (card.IsValidTarget(target)) {
                        out.push_back(Action{.kind = Action::PlayCard, .index = static_cast<int32_t>(i), .target = target.CombatId()});
                    }
                }
            } else if (card.IsValidTarget(Creature{})) {
                out.push_back(Action{.kind = Action::PlayCard, .index = static_cast<int32_t>(i), .target = -1});
            }
        }
        out.push_back(Action{.kind = Action::EndTurn});
        break;
    }
    case DecisionKind::Rewards:
        append_potion_actions(out, sim, false);
        for (size_t i = 0; i < RunFlow::RewardOptions(sim).size(); ++i)
            if (RunFlow::RewardTakable(sim, static_cast<int32_t>(i))) out.push_back(Action{.kind = Action::TakeReward, .index = static_cast<int32_t>(i)});
        out.push_back(Action{.kind = Action::Proceed});
        break;
    case DecisionKind::Event: {
        append_potion_actions(out, sim, false);
        const EventOptionList& options = RunFlow::EventOptions(sim);
        for (size_t i = 0; i < options.size(); ++i)
            if (!options[i].is_locked) out.push_back(Action{.kind = Action::EventOption, .index = static_cast<int32_t>(i)});
        break;
    }
    case DecisionKind::RestSite: {
        append_potion_actions(out, sim, false);
        for (size_t i = 0; i < RunFlow::RestSiteOptions(sim).size(); ++i)
            if (RunFlow::RestSiteOptions(sim)[i].enabled) out.push_back(Action{.kind = Action::RestOption, .index = static_cast<int32_t>(i)});
        if (RunFlow::CanProceedFromRestSite(sim)) out.push_back(Action{.kind = Action::Proceed});
        break;
    }
    case DecisionKind::Shop: {
        append_potion_actions(out, sim, false);
        for (size_t i = 0; i < RunFlow::ShopOptions(sim).size(); ++i)
            if (RunFlow::ShopOptionBuyable(sim, static_cast<int32_t>(i))) out.push_back(Action{.kind = Action::ShopBuy, .index = static_cast<int32_t>(i)});
        out.push_back(Action{.kind = Action::Proceed});
        break;
    }
    case DecisionKind::Treasure:
        append_potion_actions(out, sim, false);
        for (size_t i = 0; i < RunFlow::TreasureRelics(sim).size(); ++i) out.push_back(Action{.kind = Action::TreasurePick, .index = static_cast<int32_t>(i)});
        out.push_back(Action{.kind = Action::Proceed});
        break;
    case DecisionKind::Ended: break;
    case DecisionKind::Choice: break;
    case DecisionKind::Boundary: break;
    }
    return out;
}

std::unique_ptr<Env> Env::Predict(const Action& action) const {
    if (g_->combat.is_in_progress) ValidateCombatRoot();
    const std::vector<Action> legal = LegalActions();
    if (std::find(legal.begin(), legal.end(), action) == legal.end())
        throw InvalidRequest("Env::Predict: action is not legal at this decision");
    auto branch = Clone();
    branch->Step(action);
    if (branch->g_->combat.is_in_progress) branch->ValidateCombatRoot();
    return branch;
}

std::vector<uint8_t> Env::ActionMask() const {
    std::vector<Action> actions = LegalActions();
    std::vector<uint8_t> mask(std::max(kActionMaskSize, actions.size()), 0);
    std::fill(mask.begin(), mask.begin() + static_cast<std::ptrdiff_t>(actions.size()), 1);
    return mask;
}

std::unique_ptr<Env> Env::Clone() const {
    return std::unique_ptr<Env>(new Env(*this));
}

uint64_t Env::Hash() const {
    uint64_t h = hash_state(*g_);
    add_hash(h, pending_.has_value() ? 1 : 0);
    if (pending_) {
        add_hash(h, static_cast<uint64_t>(pending_->kind));
        add_hash(h, static_cast<uint64_t>(pending_->index));
        add_hash(h, static_cast<uint64_t>(pending_->target));
        add_hash(h, static_cast<uint64_t>(static_cast<uint8_t>(pending_->coord.col)) << 8 |
                       static_cast<uint8_t>(pending_->coord.row));
    }
    add_hash(h, std::span<const int32_t>(answers_.data(), answers_.size()));
    if (pending_operation_) {
        JsonWriter writer;
        WriteOperation(writer, *pending_operation_);
        add_hash(h, std::string_view(writer.str()));
        add_hash(h, boundary_skipped_);
        add_hash(h, boundary_.has_value() ? 1 : 0);
        if (boundary_) {
            add_hash(h, static_cast<uint64_t>(boundary_->kind)); add_hash(h, boundary_->ordinal);
            if (!base_) throw GameError("Env::Hash: boundary has no replay snapshot");
            add_hash(h, hash_state(*base_));
        }
    }
    add_hash(h, choice_.has_value() ? 1 : 0);
    if (choice_) {
        // Answer replays from the stationary snapshot, not the partially
        // executed current state. That snapshot is part of branch identity.
        if (!base_) throw GameError("Env::Hash: choice has no replay snapshot");
        add_hash(h, hash_state(*base_));
        add_hash(h, static_cast<uint64_t>(choice_->kind));
        add_hash(h, static_cast<uint64_t>(choice_->option_count));
        add_hash(h, static_cast<uint64_t>(choice_->min_select));
        add_hash(h, static_cast<uint64_t>(choice_->max_select));
        add_hash(h, std::span<const int32_t>(choice_->selected.data(), choice_->selected.size()));
        add_hash(h, static_cast<uint64_t>(choice_->subset));
        add_hash(h, static_cast<uint64_t>(choice_->can_skip));
        add_hash(h, static_cast<uint64_t>(choice_->cards.size()));
        for (uint32_t card : choice_->cards) add_hash(h, static_cast<uint64_t>(card));
        add_hash(h, std::string_view(choice_->prompt));
        add_hash(h, std::string_view(choice_->source));
        add_hash(h, static_cast<uint64_t>(choice_->options.size()));
        for (const ChoiceOption& option : choice_->options) {
            add_hash(h, static_cast<uint64_t>(option.kind));
            add_hash(h, std::string_view(option.key));
            add_hash(h, static_cast<uint64_t>(option.cards.size()));
            for (uint32_t card : option.cards) add_hash(h, static_cast<uint64_t>(card));
            add_hash(h, static_cast<uint64_t>(option.reward));
            add_hash(h, static_cast<uint64_t>(option.enabled));
        }
    }
    return h;
}

void Env::Determinize(uint64_t seed) {
    if (choice_ || boundary_) throw GameError("Env::Determinize: cannot determinize during a pending operation");
    Player player(sim_.get(), 1);
    if (player.PlayerCombatState()) {
        Rng rng(seed, "determinize");
        rng.Shuffle(CardPile::Get(PileType::Draw, player).refs());
        rng.Shuffle(CardPile::Get(PileType::Discard, player).refs());
    }

    // Future room sets and relic bags are hidden at the current decision point.
    // Keep consumed prefixes intact so determinization cannot rewrite history.
    Rng rooms_rng(seed, "determinize_rooms");
    auto shuffle_suffix = [&rooms_rng](auto& list, size_t prefix) {
        if (prefix >= list.size()) return;
        rooms_rng.shuffle(std::span(list.data() + prefix, list.size() - prefix));
    };
    for (ActState& act : g_->run.acts) {
        shuffle_suffix(act.rooms.events, std::min<size_t>(act.rooms.events_visited, act.rooms.events.size()));
        shuffle_suffix(act.rooms.normal_encounters,
                       std::min<size_t>(act.rooms.normal_encounters_visited, act.rooms.normal_encounters.size()));
        shuffle_suffix(act.rooms.elite_encounters,
                       std::min<size_t>(act.rooms.elite_encounters_visited, act.rooms.elite_encounters.size()));
        rooms_rng.shuffle(act.shared_ancient_subset.span());
    }
    auto shuffle_relic_bag = [&rooms_rng](RelicGrabBag& bag) {
        for (RelicGrabBag::Deque& deque : bag.deques) rooms_rng.shuffle(deque.relics.span());
        rooms_rng.shuffle(bag.mp_fallback.span());
        rooms_rng.shuffle(bag.original.span());
    };
    shuffle_relic_bag(g_->run.shared_relic_grab_bag);
    shuffle_relic_bag(player.st().relic_grab_bag);
    base_ = std::make_shared<GameState>(*g_);
}

std::vector<int32_t> Env::Observe() const {
    std::vector<int32_t> out(static_cast<size_t>(kObservationSize), 0);
    size_t cursor = 0;
    auto put = [&](int32_t value) {
        if (cursor >= INT32_MAX) throw CapacityError("Env::Observe: original int32 buffer count exceeded");
        if (cursor == out.size()) out.push_back(0);
        out[cursor++] = value;
    };
    DecisionKind kind = decision().kind;
    const RunData& run = g_->run;
    Player player(sim_.get(), 1);
    put(2);  // variable-length observation; pad small decisions to 2048
    put(static_cast<int32_t>(kind));
    put(run.current_act_index);
    put(run.act_floor);
    put(run.total_floor);
    put(static_cast<int32_t>(run.phase));
    put(player.Creature().CurrentHp());
    put(player.Creature().MaxHp());
    put(player.Creature().Block());
    put(player.PlayerCombatState() ? player.PlayerCombatState().Energy() : 0);
    put(player.PlayerCombatState() ? player.PlayerCombatState().Stars() : 0);
    put(player.Gold());
    if (player.PlayerCombatState()) {
        CardPile hand = player.PlayerCombatState().Hand();
        put(hand.Count());
        for (CardModel card : hand.Cards()) put(static_cast<int32_t>(card.CardId_()) * 16 + card.CurrentUpgradeLevel());
        for (int32_t i = hand.Count(); i < CardPile::MaxCardsInHand; ++i) put(0);
        put(player.PlayerCombatState().DrawPile().Count());
        put(player.PlayerCombatState().DiscardPile().Count());
        put(player.PlayerCombatState().ExhaustPile().Count());
    } else {
        put(0);
        for (int32_t i = 0; i < CardPile::MaxCardsInHand; ++i) put(0);
        put(0); put(0); put(0);
    }
    if (kind == DecisionKind::Combat) {
        CreatureList enemies = CombatState(sim_.get()).Enemies();
        put(static_cast<int32_t>(enemies.size()));
        for (Creature enemy : enemies) {
            put(static_cast<int32_t>(enemy.Monster().MonsterId_())); put(enemy.CombatId());
            put(enemy.CurrentHp()); put(enemy.MaxHp()); put(enemy.Block());
        }
        for (size_t i = enemies.size(); i < 12; ++i) { put(0); put(-1); put(0); put(0); put(0); }
    } else {
        put(0);
        for (int i = 0; i < 12; ++i) { put(0); put(-1); put(0); put(0); put(0); }
    }
    std::vector<Action> actions = LegalActions();
    put(static_cast<int32_t>(actions.size()));
    for (const Action& action : actions) {
        put(static_cast<int32_t>(action.kind)); put(action.index); put(action.target);
        put(action.coord.col); put(action.coord.row);
    }
    while (cursor < out.size()) out[cursor++] = 0;
    return out;
}

VectorEnv::VectorEnv(size_t count) {
    envs_.reserve(count);
    for (size_t i = 0; i < count; ++i) envs_.push_back(std::make_unique<Env>());
}

Env& VectorEnv::at(size_t index) {
    if (index >= envs_.size()) throw GameError("VectorEnv::at: index out of range");
    return *envs_[index];
}

const Env& VectorEnv::at(size_t index) const {
    if (index >= envs_.size()) throw GameError("VectorEnv::at: index out of range");
    return *envs_[index];
}

void VectorEnv::Reset(std::span<const NewRunOptions> options) {
    if (options.size() != envs_.size()) throw GameError("VectorEnv::Reset: option count mismatch");
    for (size_t i = 0; i < envs_.size(); ++i) envs_[i]->Reset(options[i]);
}

void VectorEnv::Step(size_t index, const Action& action) { at(index).Step(action); }

std::vector<uint64_t> VectorEnv::Hashes() const {
    std::vector<uint64_t> out;
    out.reserve(envs_.size());
    for (const auto& env : envs_) out.push_back(env->Hash());
    return out;
}

std::vector<std::vector<int32_t>> VectorEnv::Observe() const {
    std::vector<std::vector<int32_t>> out;
    out.reserve(envs_.size());
    for (const auto& env : envs_) out.push_back(env->Observe());
    return out;
}

std::vector<std::vector<Action>> VectorEnv::LegalActions() const {
    std::vector<std::vector<Action>> out;
    out.reserve(envs_.size());
    for (const auto& env : envs_) out.push_back(env->LegalActions());
    return out;
}

}  // namespace sts2
