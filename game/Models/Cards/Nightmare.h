#pragma once

#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
#include "game/Models/Powers/NightmarePower.h"

namespace sts2 {

struct Nightmare : CardModel {
    SIM_MODEL(Nightmare, CardModel)

    bool CanBeGeneratedInCombat() const { return false; }

    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CardList selected = CardSelectCmd::FromHand(ctx, Owner(), CardSelectorPrefs(1), nullptr, *this);
        if (selected.empty()) return;
        PowerModel power = PowerCmd::Apply(PowerId::NIGHTMARE_POWER, ctx, Owner().Creature(), Dec(3), Owner().Creature(), *this);
        NightmarePower(power).SetSelectedCard(selected[0]);
    }
};

}  // namespace sts2
