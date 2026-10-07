// Models/Powers/DrawCardsNextTurnPower.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct DrawCardsNextTurnPower : PowerModel { SIM_MODEL(DrawCardsNextTurnPower, PowerModel)
    Dec ModifyHandDraw(Player player, Dec count) const {
        if (player != Owner().Player() || AmountOnTurnStart() == 0) return count;
        return count + Dec(Amount());
    }
    void AfterSideTurnStart(CombatSide, const CreatureList& participants, sts2::CombatState) const {
        if (participants.contains(Owner()) && AmountOnTurnStart() != 0) PowerCmd::Remove(*this);
    }
};
}
