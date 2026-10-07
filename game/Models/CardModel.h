// Models/CardModel.cs (+ Entities/Cards/CardEnergyCost.cs as CardEnergyCost)
#pragma once

#include <cstdint>

#include "content/generated/tables.h"
#include "game/Localization/DynamicVars/DynamicVarSet.h"
#include "game/Models/AbstractModel.h"

namespace sts2 {

// Entities/Cards/CardEnergyCost.cs, over the card's CardState.
class CardEnergyCost {
public:
    CardEnergyCost() = default;
    explicit CardEnergyCost(const CardModel& card);

    int32_t Canonical() const;
    bool CostsX() const;
    bool WasJustUpgraded() const;
    bool HasLocalModifiers() const;
    int32_t CapturedXValue() const;
    void SetCapturedXValue(int32_t v) const;
    int32_t GetWithModifiers(CostModifiers modifiers) const;
    int32_t GetAmountToSpend() const;
    int32_t GetResolved() const;
    void SetUntilPlayed(int32_t cost, bool reduceOnly = false) const;
    void SetThisTurnOrUntilPlayed(int32_t cost, bool reduceOnly = false) const;
    void SetThisTurn(int32_t cost, bool reduceOnly = false) const;
    void SetThisCombat(int32_t cost, bool reduceOnly = false) const;
    void AddUntilPlayed(int32_t amount, bool reduceOnly = false) const;
    void AddThisTurnOrUntilPlayed(int32_t amount, bool reduceOnly = false) const;
    void AddThisTurn(int32_t amount, bool reduceOnly = false) const;
    void AddThisCombat(int32_t amount, bool reduceOnly = false) const;
    bool EndOfTurnCleanup() const;
    bool AfterCardPlayedCleanup() const;
    void UpgradeBy(int32_t addend) const;
    void SetCustomBaseCost(int32_t cost) const;

private:
    Sim* sim_ = nullptr;
    CardRef card_ = 0;
};

class CardModel : public AbstractModel {
public:
    CardModel() = default;
    CardModel(Sim* sim, CardRef index) : AbstractModel(sim, ModelKind::Card, index) {}

    CardState& st() const { return sim_->g->cards[index_]; }
    CardId CardId_() const { return st().id; }  // the id enum (Id() is the ModelId)
    const CardData& data() const { return sts2::data(st().id); }
    // CardModel.MultiplayerConstraint is serialized into CardData by the
    // v0.111.0 source export; the Silent class overrides are constant values.
    CardMultiplayerConstraint MultiplayerConstraint() const { return data().multiplayer; }

    // --- ownership and location
    Player Owner() const;
    void SetOwner(Player owner) const;
    CardPile Pile() const;                  // null when in no pile
    sts2::CombatState CombatState() const;  // null unless in a combat pile
    bool IsInCombat() const;
    bool HasBeenRemovedFromState() const { return st().removed; }
    void SetHasBeenRemovedFromState(bool v) const { st().removed = v; }
    CardModel DeckVersion() const;
    CardModel CloneOf() const;
    CardModel DupeOf() const;
    bool IsDupe() const { return st().is_dupe; }
    int32_t FloorAddedToDeck() const { return st().floor_added; }  // -1 = null
    void SetFloorAddedToDeck(int32_t floor) const { st().floor_added = static_cast<int32_t>(floor); }
    void RemoveFromCurrentPile(bool silent = false) const;  // Pile?.RemoveInternal(this, silent)
    void RemoveFromState() const;
    // CompareTo(AbstractModel): Id (category, then entry, ordinal), then CurrentUpgradeLevel.
    int32_t CompareTo(CardModel other) const;
    void InvokeDrawn() const {}  // Drawn: only CombatStateTracker (UI refresh) subscribes

    // --- static properties (from the tables unless overridden; see CardModel.def)
    CardPoolId Pool() const { return data().pool; }
    // CardModel.VisualCardPool: some event-pool cards are displayed as cards
    // from their original character pool. Keep the complete v0.111.0 override
    // set here; most cards use their actual pool.
    CardPoolId VisualCardPool() const {
        switch (CardId_()) {
        case CardId::CALTROPS:
        case CardId::DISTRACTION:
        case CardId::OUTMANEUVER: return CardPoolId::SILENT_CARD_POOL;
        case CardId::CLASH:
        case CardId::DUAL_WIELD:
        case CardId::ENTRENCH: return CardPoolId::IRONCLAD_CARD_POOL;
        case CardId::HELLO_WORLD:
        case CardId::REBOUND:
        case CardId::RIP_AND_TEAR:
        case CardId::STACK: return CardPoolId::DEFECT_CARD_POOL;
        default: return Pool();
        }
    }
    // CardPoolModel.IsColorless is true for these source pools, including
    // token cards such as Sovereign Blade (which are not in ColorlessCardPool).
    bool VisualCardPoolIsColorless() const {
        switch (VisualCardPool()) {
        case CardPoolId::COLORLESS_CARD_POOL:
        case CardPoolId::DEPRECATED_CARD_POOL:
        case CardPoolId::EVENT_CARD_POOL:
        case CardPoolId::TOKEN_CARD_POOL: return true;
        default: return false;
        }
    }
    bool HasTag(CardTag tag) const { return (data().tags >> static_cast<unsigned>(tag)) & 1u; }

    // --- costs
    sts2::CardEnergyCost EnergyCost() const { return sts2::CardEnergyCost(*this); }
    int32_t ResolveEnergyXValue() const;
    int32_t ResolveStarXValue() const;
    int32_t BaseReplayCount() const { return st().base_replay_count; }
    void SetBaseReplayCount(int32_t v) const { st().base_replay_count = static_cast<int32_t>(v); }
    int32_t GetEnchantedReplayCount() const;
    int32_t BaseStarCost() const;
    int32_t GetStarCostWithModifiers() const;
    bool CostsEnergyOrStars(bool includeGlobalModifiers) const {
        if (includeGlobalModifiers)
            return (!EnergyCost().CostsX() && EnergyCost().GetWithModifiers(CostModifiers::All) > 0) ||
                   (!HasStarCostX() && GetStarCostWithModifiers() > 0);
        return EnergyCost().GetWithModifiers(CostModifiers::Local) > 0 || CurrentStarCost() > 0;
    }
    // CardModel.SetToFreeThisCombat: set energy and star costs for the
    // remainder of combat, including after the card is played.
    void SetToFreeThisCombat() const;
    void SetStarCostUntilPlayed(int32_t cost) const { AddTemporaryStarCost({cost, false, true}); }
    void SetStarCostThisTurn(int32_t cost) const { AddTemporaryStarCost({cost, true, true}); }
    void SetStarCostThisCombat(int32_t cost) const { AddTemporaryStarCost({cost, false, false}); }
    void SetToFreeThisTurn() const {
        EnergyCost().SetThisTurnOrUntilPlayed(0);
        // CardModel.SetStarCostThisTurn(0) uses TemporaryCardCost.ThisTurn:
        // the override ends when the card is played or when the turn ends.
        AddTemporaryStarCost(TemporaryCardCost{0, true, true});
    }
    int32_t LastStarsSpent() const { return st().last_stars_spent; }
    void SetLastStarsSpent(int32_t v) const { st().last_stars_spent = static_cast<int32_t>(v); }

    // --- keywords, vars
    CardKeywordSet Keywords() const;  // GetKeywordsWithSources(KeywordSources.All)
    CardKeywordSet GetKeywordsWithSources(KeywordSources sources) const;
    void AddKeyword(CardKeyword keyword) const;
    void RemoveKeyword(CardKeyword keyword) const;
    DynamicVarSet DynamicVars() const { return DynamicVarSet(*this); }
    bool ExhaustOnNextPlay() const { return st().exhaust_on_next_play; }
    Dec GrowthDamage() const { return st().growth_damage; }
    void SetGrowthDamage(Dec value) const { st().growth_damage = value; }
    void SetExhaustOnNextPlay(bool v) const { st().exhaust_on_next_play = v; }
    bool ShouldRetainThisTurn() const;
    bool IsSlyThisTurn() const;
    void GiveSingleTurnRetain() const;
    void GiveSingleTurnSly() const;

    // --- enchantment / affliction
    EnchantmentModel Enchantment() const;  // null when none
    AfflictionModel Affliction() const;
    // AfflictInternal(affliction, amount): the pending affliction goes on the
    // card (the card's affliction handle is returned). ClearAfflictionInternal.
    AfflictionModel AfflictInternal(AfflictionModel affliction, Dec amount) const;
    void ClearAfflictionInternal() const;

    // --- upgrades
    int32_t CurrentUpgradeLevel() const { return st().upgrade; }
    bool IsUpgradable() const;
    bool IsRemovable() const { return !Keywords().Contains(CardKeyword::Eternal); }
    // IsTransformable: removable, or (Eternal) outside the deck.
    bool IsTransformable() const { return IsRemovable() || st().pile != PileType::Deck; }
    void UpgradeInternal() const;  // CardModel.UpgradeInternal: level + 1, table deltas, OnUpgrade
    void DowngradeInternal() const;  // CardModel.DowngradeInternal: reset to canonical values, then AfterDowngraded
    // FinalizeUpgradeInternal: clears the WasJustUpgraded flags (DynamicVars' are display-only and not kept).
    void FinalizeUpgradeInternal() const {
        st().cost_was_just_upgraded = false;
        st().was_star_cost_just_upgraded = false;
    }

    // --- play
    Creature CurrentTarget() const;
    int32_t CurrentPlayIndex() const { return st().current_play_index; }
    bool CanPlayTargeting(Creature target) const;
    bool CanPlay() const;
    bool CanPlay(UnplayableReason& reason, AbstractModel& preventer) const;
    bool IsValidTarget(Creature target) const;
    struct SpentResources {
        int32_t energy = 0;
        int32_t stars = 0;
    };
    SpentResources SpendResources() const;
    void OnPlayWrapper(PlayerChoiceContext& choiceContext, Creature target, bool isAutoPlay, const ResourceInfo& resources,
                       bool skipCardPileVisuals = false) const;
    void OnTurnEndInHandWrapper(PlayerChoiceContext& choiceContext) const;
    void MoveToResultPileWithoutPlaying(PlayerChoiceContext& choiceContext) const;
    void EndOfTurnCleanup() const;

    // --- creation
    CardModel CreateClone() const;
    CardModel CreateCloneForPlayer(Player player) const;
    CardModel CreateDupe() const;

    // --- overridable members (CardModel.def)
#define SIM_MEMBER(Base, Ret, Name, Params, Args, Default) Ret Name Params const;
#define SIM_MEMBER_CUSTOM(Base, Ret, Name, Params, Args, Default) \
    Ret Name Params const;                                          \
    Ret Name##_Default Params const;
#include "game/Models/CardModel.def"
#undef SIM_MEMBER
#undef SIM_MEMBER_CUSTOM

protected:
    int32_t& extra(int i) const { return st().extra[i]; }
    void AddTemporaryStarCost(TemporaryCardCost cost) const;
    int32_t GeneratePlayCount(sts2::CombatState combatState, Creature target) const;
};

}  // namespace sts2
