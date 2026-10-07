// Models/Relics/BurningSticks.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct BurningSticks : RelicModel {
    SIM_MODEL(BurningSticks, RelicModel)

    void AfterRoomEntered(AbstractRoom room) const {
        if (!room.IsCombatRoom()) return;
        extra(0) = 0;
        SetStatus(RelicStatus::Active);
    }

    void AfterCardExhausted(PlayerChoiceContext& choiceContext, CardModel card, bool) const {
        if (card.Owner() != Owner() || card.Type() != CardType::Skill || extra(0)) return;
        CardModel copy = card.CreateClone();
        CardPileCmd::AddGeneratedCardToCombat(copy, PileType::Hand, Owner());
        extra(0) = 1;
        SetStatus(RelicStatus::Normal);
    }

    void AfterCombatEnd(CombatRoom) const {
        extra(0) = 0;
        SetStatus(RelicStatus::Normal);
    }
};

}  // namespace sts2
