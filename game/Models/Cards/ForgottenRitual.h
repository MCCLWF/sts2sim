// Models/Cards/ForgottenRitual.cs
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct ForgottenRitual : CardModel { SIM_MODEL(ForgottenRitual, CardModel)
void OnPlay(PlayerChoiceContext&, const CardPlay*) const { PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(),Owner()); }
};
}
