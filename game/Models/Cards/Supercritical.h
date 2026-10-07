#pragma once
#include "game/Commands/PlayerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Supercritical : CardModel { SIM_MODEL(Supercritical, CardModel)
    void OnPlay(PlayerChoiceContext&, const CardPlay*) const { PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(), Owner()); }
};
}
