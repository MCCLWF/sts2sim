#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct FanOfKnives : CardModel {
    SIM_MODEL(FanOfKnives, CardModel)

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        PowerCmd::Apply(PowerId::FAN_OF_KNIVES_POWER, ctx, Owner().Creature(), Dec(1), Owner().Creature(), *this);
        for (int32_t i = 0; i < DynamicVars()["Shivs"].IntValue(); ++i) {
            CardModel shiv = CombatState().CreateCard(CardId::SHIV, Owner());
            CardPileCmd::AddGeneratedCardToCombat(shiv, PileType::Hand, Owner());
        }
    }
};

}  // namespace sts2
