#pragma once
#include "game/Commands/CardPileCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Reboot : CardModel { SIM_MODEL(Reboot, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CardList hand = Owner().PlayerCombatState().Hand().Cards();
        for (CardModel card : hand) CardPileCmd::Add(card, PileType::Draw);
        CardPileCmd::Shuffle(ctx, Owner());
        CardPileCmd::Draw(ctx, DynamicVars().Cards().BaseValue(), Owner());
    }
};
}
