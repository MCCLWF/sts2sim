#pragma once
#include "game/Models/PowerModel.h"
namespace sts2 {
struct FeralPower : PowerModel { SIM_MODEL(FeralPower, PowerModel)
    int32_t Used() const { return extra(0); }
    void AfterApplied(Creature, CardModel) const {
        extra(0) = Owner().IsPlayer() ? Owner().CombatState().st().player_zero_cost_attacks_started_this_turn : 0;
    }
    CardLocation ModifyCardPlayResultLocation(CardModel card, bool, const ResourceInfo& resources, CardLocation location) const {
        if (card.Owner().Creature() != Owner() || card.Type() != CardType::Attack || resources.EnergyValue > 0 ||
            card.IsDupe() || Used() >= Amount()) return location;
        location.player = Owner().Player();
        location.pileType = PileType::Hand;
        location.position = CardPilePosition::Top;
        return location;
    }
    void AfterModifyingCardPlayResultLocation(CardModel, CardLocation) const { ++extra(0); }
    void AfterSideTurnStart(CombatSide, const CreatureList& participants, sts2::CombatState) const {
        if (participants.contains(Owner())) extra(0) = 0;
    }
};
}
