#pragma once
#include "game/Commands/PlayerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Fuel : CardModel { SIM_MODEL(Fuel, CardModel)
    void OnPlay(PlayerChoiceContext&, const CardPlay*) const { PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(), Owner()); }
};
}
