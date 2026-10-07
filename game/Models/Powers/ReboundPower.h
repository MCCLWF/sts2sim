#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct ReboundPower : PowerModel {
    SIM_MODEL(ReboundPower, PowerModel)

    CardLocation ModifyCardPlayResultLocation(CardModel card, bool, const ResourceInfo&, CardLocation location) const {
        if (!card || card.Owner().Creature() != Owner() || location.pileType != PileType::Discard) return location;
        location.pileType = PileType::Draw;
        location.position = CardPilePosition::Top;
        return location;
    }

    void AfterModifyingCardPlayResultLocation(CardModel card, CardLocation) const {
        if (card && card.Owner().Creature() == Owner()) PowerCmd::Decrement(*this);
    }

    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (participants.contains(Owner())) PowerCmd::Remove(*this);
    }
};

}  // namespace sts2
