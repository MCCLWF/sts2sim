#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Entities/Creatures/Creature.h"
#include "game/Entities/Players/PlayerCombatState.h"
#include "game/Models/Afflictions/Hexed.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct HexPower : PowerModel {
    SIM_MODEL(HexPower, PowerModel)

    bool TryModifyKeywordsInCombat(CardModel card, CardKeywordSet& keywords) const {
        AfflictionModel affliction = card.Affliction();
        if (card.Owner() != Owner().Player() || !affliction || affliction.AfflictionId_() != AfflictionId::HEXED) return false;
        if (keywords.Contains(CardKeyword::Ethereal)) return false;
        keywords.Add(CardKeyword::Ethereal);
        return true;
    }

    void AfterApplied(Creature, CardModel) const {
        for (CardModel card : Owner().Player().PlayerCombatState().AllCards()) Afflict(card);
    }

    void AfterCardEnteredCombat(CardModel card) const {
        if (card.Owner() == Owner().Player()) Afflict(card);
    }

    void AfterDeath(PlayerChoiceContext&, Creature creature, bool wasRemovalPrevented, float) const {
        if (!wasRemovalPrevented && creature == Applier()) PowerCmd::Remove(*this);
    }

    void AfterRemoved(Creature oldOwner) const {
        sts2::CombatState combat = oldOwner.CombatState();
        if (!combat) return;
        for (Player player : combat.Players()) {
            for (CardModel card : player.PlayerCombatState().AllCards()) {
                if (card.Affliction() && card.Affliction().AfflictionId_() == AfflictionId::HEXED) CardCmd::ClearAffliction(card);
            }
        }
    }

private:
    void Afflict(CardModel card) const {
        if (!card.Affliction()) CardCmd::Afflict(AfflictionId::HEXED, card, Dec(Amount()));
    }
};

}  // namespace sts2
