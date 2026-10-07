// Models/Powers/TangledPower.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"

namespace sts2 {

struct TangledPower : PowerModel {
    SIM_MODEL(TangledPower, PowerModel)

    void AfterApplied(Creature, CardModel) const {
        Player player = Owner().Player();
        if (!player || !player.PlayerCombatState()) return;
        for (CardModel card : player.PlayerCombatState().AllCards()) {
            if (card.Type() == CardType::Attack) CardCmd::Afflict(AfflictionId::ENTANGLED, card, Dec(1));
        }
    }

    void AfterCardEnteredCombat(CardModel card) const {
        if (card.Owner() == Owner().Player() && !card.Affliction() && card.Type() == CardType::Attack) {
            CardCmd::Afflict(AfflictionId::ENTANGLED, card, Dec(1));
        }
    }

    void AfterSideTurnEnd(PlayerChoiceContext&, CombatSide, const CreatureList& participants) const {
        if (participants.contains(Owner())) {
            Flash();
            PowerCmd::Remove(*this);
        }
    }

    void AfterRemoved(Creature oldOwner) const {
        Player player = oldOwner.Player();
        if (!player || !player.PlayerCombatState()) return;
        for (CardModel card : player.PlayerCombatState().AllCards()) {
            if (card.Affliction() && card.Affliction().AfflictionId_() == AfflictionId::ENTANGLED) {
                CardCmd::ClearAffliction(card);
            }
        }
    }

    bool TryModifyEnergyCostInCombat(CardModel card, Dec originalCost, Dec& modifiedCost) const {
        if (!card.Affliction() || card.Affliction().AfflictionId_() != AfflictionId::ENTANGLED ||
            card.Owner() != Owner().Player()) {
            modifiedCost = originalCost;
            return false;
        }
        modifiedCost = originalCost + Dec(Amount());
        return true;
    }
};

}  // namespace sts2
