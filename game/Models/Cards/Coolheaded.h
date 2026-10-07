// Models/Cards/Coolheaded.cs
#pragma once
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/OrbCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Coolheaded : CardModel { SIM_MODEL(Coolheaded, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        OrbCmd::Channel(OrbId::FROST_ORB, ctx, Owner());
        CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), Owner());
    }
};
}
