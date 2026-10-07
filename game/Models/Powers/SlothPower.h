// Models/Powers/SlothPower.cs
#pragma once
#include "game/Models/PowerModel.h"
namespace sts2 {
struct SlothPower : PowerModel {
    SIM_MODEL(SlothPower, PowerModel)
    bool ShouldPlay(CardModel card, AutoPlayType) const {
        return card.Owner().Creature() != Owner() || extra(0) < Amount();
    }
    void BeforeCardPlayed(const CardPlay* play) const {
        if (play && play->Card.Owner() == Owner().Player()) ++extra(0);
    }
    void BeforeSideTurnStart(PlayerChoiceContext&, CombatSide, const CreatureList& participants, sts2::CombatState) const {
        if (participants.contains(Owner())) extra(0) = 0;
    }
};
}  // namespace sts2
