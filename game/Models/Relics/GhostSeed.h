// Models/Relics/GhostSeed.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Rooms/AbstractRoom.h"

namespace sts2 {

struct GhostSeed : RelicModel {
    SIM_MODEL(GhostSeed, RelicModel)
    static bool CanAffect(CardModel card) {
        return card.Rarity() == CardRarity::Basic &&
               (card.HasTag(CardTag::Strike) || card.HasTag(CardTag::Defend)) &&
               !card.GetKeywordsWithSources(KeywordSources::Local).Contains(CardKeyword::Ethereal);
    }
    void AfterCardEnteredCombat(CardModel card) const {
        if (card.Owner() == Owner() && CanAffect(card)) CardCmd::ApplyKeyword(card, {CardKeyword::Ethereal});
    }
    void AfterRoomEntered(AbstractRoom room) const {
        if (!room.IsCombatRoom()) return;
        for (CardModel card : Owner().PlayerCombatState().AllCards())
            if (CanAffect(card)) CardCmd::ApplyKeyword(card, {CardKeyword::Ethereal});
    }
};

}  // namespace sts2
