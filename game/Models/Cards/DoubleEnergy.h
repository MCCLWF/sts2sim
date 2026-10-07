#pragma once
#include "game/Commands/PlayerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct DoubleEnergy : CardModel { SIM_MODEL(DoubleEnergy, CardModel)
    void OnPlay(PlayerChoiceContext&, const CardPlay*) const {
        Player player = Owner();
        PlayerCmd::GainEnergy(Dec(player.PlayerCombatState().Energy()), player);
    }
};
}
