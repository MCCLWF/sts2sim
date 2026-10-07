// Models/Relics/MusicBox.cs (v0.111.0).
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct MusicBox : RelicModel {
    SIM_MODEL(MusicBox, RelicModel)

    bool WasUsedThisTurn() const { return extra(0) != 0; }
    CardModel CardBeingPlayed() const { return extra(1) > 0 ? CardModel(&sim(), static_cast<CardRef>(extra(1))) : CardModel{}; }

    void BeforeCardPlayed(const CardPlay* play) const {
        if (!play || CardBeingPlayed() || WasUsedThisTurn() || play->Card.Owner() != Owner() ||
            play->Card.Type() != CardType::Attack) return;
        extra(1) = static_cast<int32_t>(play->Card.Index());
    }
    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* play) const {
        CardModel source = CardBeingPlayed();
        if (!play || !source || play->Card != source) return;
        CardModel copy = source.CreateClone();
        CardCmd::ApplyKeyword(copy, {CardKeyword::Ethereal});
        CardPileCmd::AddGeneratedCardToCombat(copy, PileType::Hand, Owner());
        extra(0) = 1;
        extra(1) = 0;
        Flash();
    }
    void BeforeSideTurnStart(PlayerChoiceContext&, CombatSide, const CreatureList& participants, CombatState) const {
        if (!participants.contains(Owner().Creature())) return;
        extra(0) = 0;
        extra(1) = 0;
    }
    void AfterCombatEnd(CombatRoom) const { extra(0) = extra(1) = 0; }
};

}  // namespace sts2
