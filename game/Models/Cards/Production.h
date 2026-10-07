// Models/Cards/Production.cs
#pragma once
#include "game/Commands/PlayerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Production : CardModel {
    SIM_MODEL(Production, CardModel)
    void OnPlay(PlayerChoiceContext&, const CardPlay*) const {
        PlayerCmd::GainEnergy(DynamicVars().Energy().BaseValue(), Owner());
    }
};
}
