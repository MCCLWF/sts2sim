#pragma once
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Scavenge : CardModel { SIM_MODEL(Scavenge, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        CardList selected = CardSelectCmd::FromHand(ctx, Owner(), CardSelectorPrefs(1), nullptr, *this);
        if (!selected.empty()) CardCmd::Exhaust(ctx, selected[0]);
        PowerCmd::Apply(PowerId::ENERGY_NEXT_TURN_POWER, ctx, Owner().Creature(), DynamicVars().Energy().BaseValue(), Owner().Creature(), *this);
    }
};
}
