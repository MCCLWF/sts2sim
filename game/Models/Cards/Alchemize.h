#pragma once

#include "game/Commands/PotionCmd.h"
#include "game/Factories/PotionFactory.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Alchemize : CardModel {
    SIM_MODEL(Alchemize, CardModel)

    bool CanBeGeneratedInCombat() const { return false; }

    void OnPlay(PlayerChoiceContext&, const CardPlay*) const {
        PotionId potion = PotionFactory::CreateRandomPotionInCombat(Owner(), Owner().RunState().Rng().CombatPotionGeneration());
        if (potion != PotionId::NONE) PotionCmd::TryToProcure(potion, Owner());
    }

};

}  // namespace sts2
