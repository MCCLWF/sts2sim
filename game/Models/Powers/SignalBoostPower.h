#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct SignalBoostPower : PowerModel { SIM_MODEL(SignalBoostPower, PowerModel)
    int32_t ModifyCardPlayCount(CardModel card, Creature, int32_t count) const {
        return card.Owner().Creature() == Owner() && card.Type() == CardType::Power ? count + 1 : count;
    }
    void AfterModifyingCardPlayCount(CardModel) const { PowerCmd::Decrement(*this); }
};
}
