#pragma once
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/OrbCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Modded : CardModel { SIM_MODEL(Modded, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        OrbCmd::AddSlots(Owner(), DynamicVars().Repeat().IntValue());
        CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), Owner());
        EnergyCost().AddThisCombat(1);
    }
};
}
