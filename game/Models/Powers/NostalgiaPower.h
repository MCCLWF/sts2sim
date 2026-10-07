#pragma once

#include "game/Models/PowerModel.h"

namespace sts2 {

struct NostalgiaPower : PowerModel {
    SIM_MODEL(NostalgiaPower, PowerModel)

    CardLocation ModifyCardPlayResultLocation(CardModel card, bool, const ResourceInfo&, CardLocation location) const {
        if (card.Owner().Creature() != Owner() ||
            (card.Type() != CardType::Attack && card.Type() != CardType::Skill) ||
            location.pileType != PileType::Discard ||
            Owner().CombatState().st().player_attack_skill_cards_started_this_turn >= Amount()) return location;
        location.pileType = PileType::Draw;
        location.position = CardPilePosition::Top;
        return location;
    }

    void AfterModifyingCardPlayResultLocation(CardModel, CardLocation) const { Flash(); }
};

}  // namespace sts2
