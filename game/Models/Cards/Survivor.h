// Models/Cards/Survivor.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct Survivor : CardModel {
    SIM_MODEL(Survivor, CardModel)

    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
        CardList selected = CardSelectCmd::FromHandForDiscard(ctx, Owner(), CardSelectorPrefs(1), nullptr, *this);
        if (!selected.empty()) CardCmd::Discard(ctx, selected[0]);
    }
};

}  // namespace sts2
