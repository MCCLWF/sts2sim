#pragma once
#include "game/Commands/CardPileCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Skim : CardModel { SIM_MODEL(Skim, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), Owner()); }
};
}
