// Models/Relics/VelvetChoker.cs
#pragma once

#include "game/Combat/CombatManager.h"
#include "game/Entities/Cards/CardPlay.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct VelvetChoker : RelicModel {
    SIM_MODEL(VelvetChoker, RelicModel)

    int32_t CardsPlayedThisTurn() const { return extra(0); }
    void Reset() const { extra(0) = 0; }
    bool ShowCounter() const { return CombatManager(&sim()).IsInProgress(); }
    int32_t DisplayAmount() const { return Owner() ? CardsPlayedThisTurn() : 0; }
    Dec ModifyMaxEnergy(Player player, Dec amount) const {
        return player == Owner() ? amount + DynamicVars().Energy().BaseValue() : amount;
    }
    bool ShouldPlay(CardModel card, AutoPlayType) const {
        return card.Owner() != Owner() || CardsPlayedThisTurn() < DynamicVars().Cards().IntValue();
    }
    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* play) const {
        if (play && play->Card.Owner() == Owner()) ++extra(0);
    }
    void AfterRoomEntered(AbstractRoom room) const {
        if (room.IsCombatRoom()) Reset();
    }
    void AfterCombatEnd(CombatRoom) const { Reset(); }
    void BeforeSideTurnStart(PlayerChoiceContext&, CombatSide, const CreatureList& participants, CombatState) const {
        if (participants.contains(Owner().Creature())) Reset();
    }
};

}  // namespace sts2
