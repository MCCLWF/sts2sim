// Models/CardModel.cs and Entities/Cards/CardEnergyCost.cs
#include "game/Models/CardModel.h"

#include <algorithm>
#include "core/csharp_int.h"

#include "game/All.h"
#include "game/Combat/CombatManager.h"
#include "game/Combat/History/CombatHistory.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Hooks/Hook.h"

namespace sts2 {

// ============================================================ CardEnergyCost

CardEnergyCost::CardEnergyCost(const CardModel& card) : sim_(&card.sim()), card_(card.Index()) {}

int32_t CardModel::ResolveEnergyXValue() const {
    if (!EnergyCost().CostsX()) throw GameError("This card does not have an X-cost.");
    return Hook::ModifyXValue(CombatState(), *this, EnergyCost().CapturedXValue());
}

int32_t CardModel::ResolveStarXValue() const {
    if (!HasStarCostX()) throw GameError("This card does not have an X star cost.");
    return Hook::ModifyXValue(CombatState(), *this, LastStarsSpent());
}

namespace {
CardState& cs(Sim* sim, CardRef c) { return sim->g->cards[c]; }
}  // namespace

// Canonical: the table cost of the card as created (0 for X-cost cards).
int32_t CardEnergyCost::Canonical() const {
    const CardData& d = data(cs(sim_, card_).id);
    return d.costs_x ? 0 : d.levels[0].energy_cost;
}
bool CardEnergyCost::CostsX() const { return data(cs(sim_, card_).id).costs_x; }
bool CardEnergyCost::WasJustUpgraded() const { return cs(sim_, card_).cost_was_just_upgraded; }
bool CardEnergyCost::HasLocalModifiers() const { return !cs(sim_, card_).cost_modifiers.empty(); }

int32_t CardEnergyCost::CapturedXValue() const {
    if (!CostsX()) throw GameError("Only X-cost cards have a captured value.");
    return cs(sim_, card_).captured_x;
}

void CardEnergyCost::SetCapturedXValue(int32_t v) const {
    if (!CostsX()) throw GameError("Only X-cost cards have a captured value.");
    cs(sim_, card_).captured_x = static_cast<int32_t>(v);
}

int32_t CardEnergyCost::GetWithModifiers(CostModifiers modifiers) const {
    const CardState& c = cs(sim_, card_);
    int32_t num = c.cost_base;
    if (num < 0) return num;
    if (CostsX()) return num;
    if (has_flag(modifiers, CostModifiers::Local)) {
        for (const LocalCostModifier& m : c.cost_modifiers) {
            switch (m.type) {  // LocalCostModifier.Modify
            case LocalCostType::Absolute: num = m.reduce_only ? std::min(num, static_cast<int32_t>(m.amount)) : m.amount; break;
            case LocalCostType::Relative: num = m.reduce_only ? std::min(num, int_add(num, m.amount)) : int_add(num, m.amount); break;
            default: throw GameError("LocalCostModifier: invalid Type");
            }
        }
    }
    CardModel card(sim_, card_);
    if (has_flag(modifiers, CostModifiers::Global) && card.CombatState()) {
        num = Hook::ModifyEnergyCostInCombat(card.CombatState(), card, Dec(num)).to_int();
    }
    return std::max(0, num);
}

int32_t CardEnergyCost::GetAmountToSpend() const {
    if (CostsX()) {
        PlayerCombatState pcs = CardModel(sim_, card_).Owner().PlayerCombatState();
        return pcs ? pcs.Energy() : 0;
    }
    return std::max(0, GetWithModifiers(CostModifiers::All));
}

int32_t CardEnergyCost::GetResolved() const {
    if (CostsX()) return CapturedXValue();
    return std::max(0, GetWithModifiers(CostModifiers::All));
}

namespace {
void add_modifier(Sim* sim, CardRef card, int32_t amount, LocalCostType type, LocalCostModifierExpiration exp, bool reduceOnly) {
    auto& modifiers = cs(sim, card).cost_modifiers;
    modifiers.push_back(LocalCostModifier{amount, type, reduceOnly, exp});
}
constexpr auto kWhenPlayed = LocalCostModifierExpiration::WhenPlayed;
constexpr auto kEndOfTurn = LocalCostModifierExpiration::EndOfTurn;
constexpr auto kEndOfCombat = LocalCostModifierExpiration::EndOfCombat;
}  // namespace

void CardEnergyCost::SetUntilPlayed(int32_t cost, bool reduceOnly) const {
    if (cost != 0 || Canonical() >= 0) add_modifier(sim_, card_, cost, LocalCostType::Absolute, kWhenPlayed, reduceOnly);
}
void CardEnergyCost::SetThisTurnOrUntilPlayed(int32_t cost, bool reduceOnly) const {
    if (cost != 0 || Canonical() >= 0) add_modifier(sim_, card_, cost, LocalCostType::Absolute, kEndOfTurn | kWhenPlayed, reduceOnly);
}
void CardEnergyCost::SetThisTurn(int32_t cost, bool reduceOnly) const {
    if (cost != 0 || Canonical() >= 0) add_modifier(sim_, card_, cost, LocalCostType::Absolute, kEndOfTurn, reduceOnly);
}
void CardEnergyCost::SetThisCombat(int32_t cost, bool reduceOnly) const {
    if (cost != 0 || Canonical() >= 0) add_modifier(sim_, card_, cost, LocalCostType::Absolute, kEndOfCombat, reduceOnly);
}
void CardEnergyCost::AddUntilPlayed(int32_t amount, bool reduceOnly) const {
    if (amount != 0) add_modifier(sim_, card_, amount, LocalCostType::Relative, kWhenPlayed, reduceOnly);
}
void CardEnergyCost::AddThisTurnOrUntilPlayed(int32_t amount, bool reduceOnly) const {
    if (amount != 0) add_modifier(sim_, card_, amount, LocalCostType::Relative, kEndOfTurn | kWhenPlayed, reduceOnly);
}
void CardEnergyCost::AddThisTurn(int32_t amount, bool reduceOnly) const {
    if (amount != 0) add_modifier(sim_, card_, amount, LocalCostType::Relative, kEndOfTurn, reduceOnly);
}
void CardEnergyCost::AddThisCombat(int32_t amount, bool reduceOnly) const {
    if (amount != 0) add_modifier(sim_, card_, amount, LocalCostType::Relative, kEndOfCombat, reduceOnly);
}

namespace {
bool remove_modifiers(Sim* sim, CardRef card, LocalCostModifierExpiration exp) {
    auto& mods = cs(sim, card).cost_modifiers;
    bool removed = false;
    for (size_t i = mods.size(); i-- > 0;) {
        if (has_flag(mods[i].expiration, exp)) {
            mods.erase_at(i);
            removed = true;
        }
    }
    return removed;
}
}  // namespace

bool CardEnergyCost::EndOfTurnCleanup() const { return remove_modifiers(sim_, card_, kEndOfTurn); }
bool CardEnergyCost::AfterCardPlayedCleanup() const { return remove_modifiers(sim_, card_, kWhenPlayed); }

void CardEnergyCost::UpgradeBy(int32_t addend) const {
    if (CostsX() || addend == 0) return;
    CardState& c = cs(sim_, card_);
    int32_t num = c.cost_base;
    int32_t num2 = std::max(int_add(c.cost_base, addend), 0);
    c.cost_was_just_upgraded = true;
    if (num2 < num) {
        for (LocalCostModifier& m : c.cost_modifiers) {
            if (m.type == LocalCostType::Absolute && m.amount > num2) m.amount = static_cast<int32_t>(num2);
        }
    }
    SetCustomBaseCost(num2);
}

void CardEnergyCost::SetCustomBaseCost(int32_t cost) const { cs(sim_, card_).cost_base = static_cast<int32_t>(cost); }

// ================================================================ CardModel

Player CardModel::Owner() const { return st().owner ? Player(sim_, st().owner) : Player{}; }

void CardModel::SetOwner(Player owner) const {
    if (st().owner != 0 && owner) throw GameError("Card already has an owner.");
    st().owner = owner ? owner.Index() : 0;
}

CardPile CardModel::Pile() const {
    if (st().pile == PileType::None || st().owner == 0) return {};
    return CardPile(sim_, st().owner, st().pile);
}

sts2::CombatState CardModel::CombatState() const {
    if (st().pile != PileType::None && IsCombatPile(st().pile) && Owner()) return Owner().Creature().CombatState();
    return {};
}

bool CardModel::IsInCombat() const { return st().pile != PileType::None && IsCombatPile(st().pile); }
CardModel CardModel::DeckVersion() const { return CardModel(sim_, st().deck_version); }
CardModel CardModel::CloneOf() const { return CardModel(sim_, st().clone_of); }
CardModel CardModel::DupeOf() const { return IsDupe() ? CloneOf() : CardModel{}; }

void CardModel::RemoveFromCurrentPile(bool silent) const {
    CardPile pile = Pile();
    if (pile) pile.RemoveInternal(*this, silent);
}

void CardModel::RemoveFromState() const {
    RemoveFromCurrentPile();
    SetHasBeenRemovedFromState(true);
}

int32_t CardModel::CompareTo(CardModel other) const {
    if (*this == other) return 0;
    if (!other) return 1;
    int32_t num = data().entry.compare(other.data().entry);  // same category: Entry, ordinal
    if (num != 0) return num < 0 ? -1 : 1;
    if (CurrentUpgradeLevel() != other.CurrentUpgradeLevel()) return CurrentUpgradeLevel() < other.CurrentUpgradeLevel() ? -1 : 1;
    return 0;
}

int32_t CardModel::BaseStarCost() const {
    if (!st().star_cost_set) {
        st().base_star_cost = static_cast<int32_t>(CanonicalStarCost());
        st().star_cost_set = true;
    }
    return st().base_star_cost;
}

int32_t CardModel::CurrentStarCost_Default() const {
    const auto& tmp = st().temporary_star_costs;
    if (!tmp.empty()) {
        int32_t num = tmp.back().cost;
        if (num == 0 && BaseStarCost() < 0) return BaseStarCost();
        return num;
    }
    return BaseStarCost();
}

int32_t CardModel::GetStarCostWithModifiers() const {
    if (HasStarCostX()) {
        PlayerCombatState pcs = Owner().PlayerCombatState();
        return pcs ? pcs.Stars() : 0;
    }
    CardPile pile = Pile();
    if (pile && pile.IsCombatPile() && CombatState()) {
        return Hook::ModifyStarCost(CombatState(), *this, Dec(CurrentStarCost())).to_int();
    }
    return CurrentStarCost();
}

void CardModel::SetToFreeThisCombat() const {
    EnergyCost().SetThisCombat(0);
    AddTemporaryStarCost(TemporaryCardCost{0, false, false});
}

void CardModel::AddTemporaryStarCost(TemporaryCardCost cost) const {
    cost.cost = std::max(0, cost.cost); // TemporaryCardCost factories clamp before storing.
    st().temporary_star_costs.push_back(cost);
}

CardKeywordSet CardModel::Keywords() const { return GetKeywordsWithSources(KeywordSources::All); }

CardKeywordSet CardModel::GetKeywordsWithSources(KeywordSources sources) const {
    bool flag = has_flag(sources, KeywordSources::Local);
    CardKeywordSet local{st().keywords};
    if (!has_flag(sources, KeywordSources::Global) || !CombatState()) {
        return flag ? local : CardKeywordSet{};
    }
    CardKeywordSet set = flag ? local : CardKeywordSet{};
    Hook::ModifyKeywordsInCombat(CombatState(), *this, set);
    return set;
}

void CardModel::AddKeyword(CardKeyword keyword) const { st().keywords |= uint64_t{1} << static_cast<unsigned>(keyword); }
void CardModel::RemoveKeyword(CardKeyword keyword) const { st().keywords &= ~(uint64_t{1} << static_cast<unsigned>(keyword)); }

bool CardModel::ShouldRetainThisTurn() const {
    if (!Keywords().Contains(CardKeyword::Retain)) return st().single_turn_retain;
    return true;
}

bool CardModel::IsSlyThisTurn() const {
    if (!Keywords().Contains(CardKeyword::Sly)) return st().single_turn_sly;
    return true;
}

void CardModel::GiveSingleTurnRetain() const { st().single_turn_retain = true; }
void CardModel::GiveSingleTurnSly() const { st().single_turn_sly = true; }

EnchantmentModel CardModel::Enchantment() const {
    return st().enchantment != EnchantmentId::NONE ? EnchantmentModel(sim_, index_) : EnchantmentModel{};
}
AfflictionModel CardModel::Affliction() const {
    const AfflictionRef ref = st().affliction_instance;
    return ref != 0 && sim_->g->afflictions.live(ref)
        ? AfflictionModel(sim_, ref, true) : AfflictionModel{};
}

AfflictionModel CardModel::AfflictInternal(AfflictionModel affliction, Dec amount) const {
    if (Affliction()) throw GameError("Attempted to afflict a card that was already afflicted! This is not allowed");
    if (!affliction.IsPending()) throw GameError("Afflictions cannot be moved from one card to another.");
    if (!affliction.HasInstance() || !affliction.IsPending()) throw GameError("AfflictionModel is not a pending instance");
    st().affliction = affliction.AfflictionId_();
    st().affliction_instance = static_cast<AfflictionRef>(affliction.Index() & ~AfflictionModel::kHandle);
    affliction.st().card = index_;
    affliction.st().pending = false;
    st().affliction_amount = 0;
    AfflictionModel onCard(sim_, index_);
    onCard.SetAmount(amount.to_int());  // Affliction.Amount = (int)amount
    // AfflictionChanged: the card nodes' event.
    return onCard;
}

void CardModel::ClearAfflictionInternal() const {
    AfflictionModel a = Affliction();
    if (!a) return;
    a.ClearInternal();
    st().affliction = AfflictionId::NONE;
    st().affliction_amount = 0;
    st().affliction_instance = 0;
    Owner().PlayerCombatState().RecalculateCardValues();
}

bool CardModel::IsUpgradable() const { return CurrentUpgradeLevel() < MaxUpgradeLevel(); }

// UpgradeInternal: CurrentUpgradeLevel++; OnUpgrade(). Table-only OnUpgrade
// overrides are suppressed by gen_dispatch.py; applying the level delta here
// already reproduces their UpgradeValueBy, EnergyCost.UpgradeBy, and keyword
// changes. The remaining OnUpgrade overrides carry behavior beyond those
// table deltas and are dispatched below.
void CardModel::UpgradeInternal() const {
    const CardData& d = data();
    int from = st().upgrade;
    int to = from + 1;
    st().upgrade = static_cast<uint8_t>(to);
    if (static_cast<size_t>(to) < d.levels.size()) {
        const CardLevel& a = d.levels[static_cast<size_t>(from)];
        const CardLevel& b = d.levels[static_cast<size_t>(to)];
        // A var without an override reads the table at the new level already;
        // an overridden one gets the same UpgradeValueBy(delta).
        for (VarOverride& o : st().var_overrides) {
            if (b.vars[o.index] != a.vars[o.index]) o.value = o.value + (b.vars[o.index] - a.vars[o.index]);
        }
        if (b.energy_cost != a.energy_cost) EnergyCost().UpgradeBy(b.energy_cost - a.energy_cost);
        uint64_t added = b.keywords & ~a.keywords;
        uint64_t removed = a.keywords & ~b.keywords;
        st().keywords = (st().keywords | added) & ~removed;
    }
    OnUpgrade();
}

// CardModel.DowngradeInternal: reset mutable values to the canonical base
// level, while retaining enchantments, afflictions, and subclass state.
void CardModel::DowngradeInternal() const {
    st().upgrade = 0;
    st().var_overrides.clear();
    st().cost_base = static_cast<int32_t>(EnergyCost().Canonical());
    st().base_star_cost = static_cast<int32_t>(CanonicalStarCost());
    st().keywords = data().levels[0].keywords;
    AfterDowngraded();
    if (EnchantmentModel enchantment = Enchantment()) enchantment.ModifyCard();
    if (Affliction()) Affliction().AfterApplied();
}


Creature CardModel::CurrentTarget() const { return Creature(sim_, st().current_target); }

bool CardModel::CanPlayTargeting(Creature target) const {
    if (!IsValidTarget(target)) return false;
    return CanPlay();
}

bool CardModel::CanPlay() const {
    UnplayableReason reason;
    AbstractModel preventer;
    return CanPlay(reason, preventer);
}

bool CardModel::CanPlay(UnplayableReason& reason, AbstractModel& preventer) const {
    reason = UnplayableReason::None;
    sts2::CombatState combatState = CombatState() ? CombatState() : (Owner() ? Owner().Creature().CombatState() : sts2::CombatState{});
    if (!combatState || !Owner().PlayerCombatState()) {
        preventer = {};
        return false;
    }
    if (Keywords().Contains(CardKeyword::Unplayable)) reason = reason | UnplayableReason::HasUnplayableKeyword;
    UnplayableReason reason2;
    if (!Owner().PlayerCombatState().HasEnoughResourcesFor(*this, reason2)) reason = reason | reason2;
    if (TargetType() == sts2::TargetType::AnyAlly) {
        int alive = 0;
        for (Creature c : combatState.PlayerCreatures()) alive += c.IsAlive() ? 1 : 0;
        if (alive <= 1) reason = reason | UnplayableReason::NoLivingAllies;
    }
    if (!Hook::ShouldPlay(combatState, *this, preventer, AutoPlayType::None)) reason = reason | UnplayableReason::BlockedByHook;
    if (!IsPlayable()) reason = reason | UnplayableReason::BlockedByCardLogic;
    return reason == UnplayableReason::None;
}

bool CardModel::IsValidTarget(Creature target) const {
    if (!target) {
        if (TargetType() != sts2::TargetType::AnyEnemy) return TargetType() != sts2::TargetType::AnyAlly;
        return false;
    }
    if (!target.IsAlive()) return false;
    if (TargetType() == sts2::TargetType::AnyEnemy) return target.Side() != Owner().Creature().Side();
    if (TargetType() == sts2::TargetType::AnyAlly) return target.Side() == Owner().Creature().Side();
    return false;
}

CardModel::SpentResources CardModel::SpendResources() const {
    int32_t energy = Owner().PlayerCombatState().Energy();
    int32_t energyToSpend = EnergyCost().GetAmountToSpend();
    int32_t starsToSpend = std::max(0, GetStarCostWithModifiers());
    if (energyToSpend > energy && Hook::ShouldPayExcessEnergyCostWithStars(CombatState(), Owner())) {
        starsToSpend += (energyToSpend - energy) * 2;
        energyToSpend = energy;
    }
    // SpendEnergy
    if (EnergyCost().CostsX()) EnergyCost().SetCapturedXValue(energyToSpend);
    if (energyToSpend > 0) {
        CombatManager(sim_).History().EnergySpent(CombatState(), energyToSpend, Owner());
        Owner().PlayerCombatState().LoseEnergy(Dec(std::max(0, energyToSpend)));
    }
    Hook::AfterEnergySpent(CombatState(), *this, energyToSpend);
    // SpendStars
    st().last_stars_spent = static_cast<int32_t>(starsToSpend);
    if (starsToSpend > 0) {
        Owner().PlayerCombatState().LoseStars(Dec(starsToSpend));
        Hook::AfterStarsSpent(Owner().Creature().CombatState(), starsToSpend, Owner());
    }
    return SpentResources{energyToSpend, starsToSpend};
}

CardLocation CardModel::GetResultLocationForCardPlay_Default() const {
    if (IsDupe() || Type() == CardType::Power) return CardLocation{Owner(), PileType::None, CardPilePosition::Bottom};
    if (ExhaustOnNextPlay() || Keywords().Contains(CardKeyword::Exhaust)) {
        SetExhaustOnNextPlay(false);
        return CardLocation{Owner(), PileType::Exhaust, CardPilePosition::Bottom};
    }
    return CardLocation{Owner(), PileType::Discard, CardPilePosition::Bottom};
}

int32_t CardModel::GetEnchantedReplayCount() const {
    EnchantmentModel e = Enchantment();
    return e ? e.EnchantPlayCount(BaseReplayCount()) : BaseReplayCount();
}

int32_t CardModel::GeneratePlayCount(sts2::CombatState combatState, Creature target) const {
    int32_t playCount = GetEnchantedReplayCount() + 1;
    ModelList modifyingModels;
    playCount = Hook::ModifyCardPlayCount(combatState, *this, playCount, target, modifyingModels);
    Hook::AfterModifyingCardPlayCount(combatState, *this, modifyingModels);
    return playCount;
}

// OnPlayWrapper. The choice context's model stack, VFX and waits are dropped.
void CardModel::OnPlayWrapper(PlayerChoiceContext& choiceContext, Creature target, bool isAutoPlay,
                              const ResourceInfo& resources, bool skipCardPileVisuals) const {
    ExecutionSourceScope damageSource(*sim_, ExecutionDamageSourceKind::Card, data().entry);
    choiceContext.PushModel(*this);
    st().current_target = target.Index();
    st().current_play_index = 0;
    if (!isAutoPlay) {
        CardPileCmd::AddDuringManualCardPlay(*this);
    } else {
        CardPileCmd::Add(*this, PileType::Play, CardPilePosition::Bottom, AbstractModel{}, skipCardPileVisuals);
    }
    sts2::CombatState combatState = CombatState();
    if (!combatState) return;
    CardLocation resultLocation = GetResultLocationForCardPlay();
    ModelList modifiers;
    resultLocation = Hook::ModifyCardPlayResultLocation(combatState, *this, isAutoPlay, resources, resultLocation, modifiers);
    for (AbstractModel item : modifiers) item.AfterModifyingCardPlayResultLocation(*this, resultLocation);
    int32_t playCount = GeneratePlayCount(combatState, target);
    if (Owner().Creature().IsDead()) return;
    CombatManager cm(sim_);
    cm.BeginCardOrPotionEffect(Owner());
    struct EndEffect {  // finally { await CombatManager.Instance.EndCardOrPotionEffect(...) }
        CombatManager& cm;
        Player owner;
        ~EndEffect() noexcept(false) {
            if (std::uncaught_exceptions() == 0) cm.EndCardOrPotionEffect(owner);
        }
    };
    {
        EndEffect end{cm, Owner()};
        for (int32_t i = 0; i < playCount; ++i) {
            if (cm.IsOverOrEnding()) break;
            st().current_play_index = static_cast<int32_t>(i);
            CardPlay cardPlay{*this, Owner(), target, resultLocation.pileType, resources, isAutoPlay, i, playCount};
            Hook::BeforeCardPlayed(combatState, &cardPlay);
            if (cm.IsInProgress()) cm.History().CardPlayStarted(combatState, cardPlay);
            PlayerChoiceContext branchingPlayerChoiceContext;  // BranchingPlayerChoiceContext
            branchingPlayerChoiceContext.PushModel(*this);
            cm.History().CardEffectStarted(cardPlay);
            OnPlay(branchingPlayerChoiceContext, &cardPlay);
            if (Owner().Creature().IsDead()) return;
            if (EnchantmentModel e = Enchantment()) {
                e.OnPlay(choiceContext, &cardPlay);
                if (Owner().Creature().IsDead()) return;
            }
            if (AfflictionModel a = Affliction()) {
                a.OnPlay(choiceContext, target);
                if (Owner().Creature().IsDead()) return;
            }
            if (cm.IsInProgress()) cm.History().CardPlayFinished(combatState, cardPlay);
            if (cm.IsInProgress()) {
                Hook::AfterCardPlayed(combatState, choiceContext, &cardPlay);
                if (Owner().Creature().IsDead()) return;
            }
        }
    }
    Player originalOwner = Owner();
    // CombatManager.Instance.CurrentCombatId == effectCombatId: the same combat.
    if (resultLocation.player != originalOwner && resultLocation.pileType != PileType::None) {
        throw GameError("GiveToAnotherPlayer: multiplayer only");
    }
    CardPile pile = Pile();
    if (pile && pile.Type() == PileType::Play) {
        switch (resultLocation.pileType) {
        case PileType::None: CardPileCmd::RemoveFromCombat(*this, skipCardPileVisuals); break;
        case PileType::Exhaust: CardCmd::Exhaust(choiceContext, *this, /*causedByEthereal=*/false, skipCardPileVisuals); break;
        default:
            CardPileCmd::Add(*this, resultLocation.pileType, resultLocation.position, AbstractModel{}, skipCardPileVisuals);
            break;
        }
    }
    cm.CheckForEmptyHand(choiceContext, originalOwner);
    EnergyCost().AfterCardPlayedCleanup();
    auto& tmp = st().temporary_star_costs;  // _temporaryStarCosts.RemoveAll(ClearsWhenCardIsPlayed)
    for (size_t i = tmp.size(); i-- > 0;) {
        if (tmp[i].clears_when_card_is_played) tmp.erase_at(i);
    }
    st().current_target = 0;
    st().current_play_index = 0;
    choiceContext.PopModel(*this);
}

void CardModel::OnTurnEndInHandWrapper(PlayerChoiceContext& choiceContext) const { OnTurnEndInHand(choiceContext); }

void CardModel::MoveToResultPileWithoutPlaying(PlayerChoiceContext& choiceContext) const {
    CardPile pile = Pile();
    if (pile && pile.Type() == PileType::Play) {
        if (IsDupe()) {
            CardPileCmd::RemoveFromCombat(*this);
        } else if (ExhaustOnNextPlay() || Keywords().Contains(CardKeyword::Exhaust)) {
            CardCmd::Exhaust(choiceContext, *this);
        } else {
            CardPileCmd::Add(*this, PileType::Discard);
        }
    }
}

void CardModel::EndOfTurnCleanup() const {
    SetExhaustOnNextPlay(false);
    st().single_turn_retain = false;
    st().single_turn_sly = false;
    EnergyCost().EndOfTurnCleanup();
    auto& tmp = st().temporary_star_costs;
    for (size_t i = tmp.size(); i-- > 0;) {
        if (tmp[i].clears_when_turn_ends) tmp.erase_at(i);
    }
}

// CreateClone: CardScope.CloneCard(this) (the combat), _cloneOf = this.
CardModel CardModel::CreateClone() const {
    CardPile pile = Pile();
    if (pile && !IsCombatPile(pile.Type())) throw GameError("Cannot create a clone of a card that is not in a combat pile.");
    CardModel clone = CombatState().CloneCard(*this);
    clone.st().clone_of = index_;
    clone.SetExhaustOnNextPlay(false);
    return clone;
}

CardModel CardModel::CreateCloneForPlayer(Player player) const {
    CardModel clone = CreateClone();
    // C#: CreateCloneForPlayer changes the mutable clone's Owner after cloning.
    clone.st().owner = player.Index();
    return clone;
}

CardModel CardModel::CreateDupe() const {
    if (IsDupe()) return DupeOf().CreateDupe();
    CardModel clone = CreateClone();  // CreateCloneForPlayer(owner): same owner in single player
    clone.st().is_dupe = true;
    clone.RemoveKeyword(CardKeyword::Exhaust);
    return clone;
}

// Constructor arguments (CardModel(cost, type, rarity, target)): table values.
CardType CardModel::Type_Default() const { return data().type; }
CardRarity CardModel::Rarity_Default() const { return data().rarity; }
int32_t CardModel::CanonicalEnergyCost_Default() const { return data().levels[0].energy_cost; }
sts2::TargetType CardModel::TargetType_Default() const { return data().target; }
sts2::OrbEvokeType CardModel::OrbEvokeType_Default() const { return OrbEvokeType::None; }

bool CardModel::IsBasicStrikeOrDefend_Default() const {
    return Rarity() == CardRarity::Basic && (HasTag(CardTag::Strike) || HasTag(CardTag::Defend));
}


}  // namespace sts2
