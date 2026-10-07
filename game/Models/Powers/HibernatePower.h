#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct HibernatePower : PowerModel { SIM_MODEL(HibernatePower, PowerModel)
    void AfterPlayerTurnStart(PlayerChoiceContext&, Player player) const {
        if (player.Creature() == Owner()) PowerCmd::Decrement(*this);
    }
};
}
