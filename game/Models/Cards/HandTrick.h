#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct HandTrick : CardModel {
    SIM_MODEL(HandTrick, CardModel)

    bool GainsBlock() const { return true; }

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
        CardList selected = CardSelectCmd::FromHand(ctx, Owner(), CardSelectorPrefs(1),
            [](CardModel card) { return card.Type() == CardType::Skill && !card.IsSlyThisTurn(); }, *this);
        if (!selected.empty()) CardCmd::ApplySingleTurnSly(selected[0]);
    }
};

}  // namespace sts2
