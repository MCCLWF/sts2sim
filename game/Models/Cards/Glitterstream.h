#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
#include "game/Hooks/Hook.h"

namespace sts2 {
struct Glitterstream : CardModel {
    SIM_MODEL(Glitterstream, CardModel)
    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        DynamicVar nextTurn = DynamicVars()["BlockNextTurn"];
        ModelList modifiers;
        Dec nextTurnAmount = Hook::ModifyBlock(CombatState(), Owner().Creature(), nextTurn.BaseValue(),
            nextTurn.Props(), *this, play, modifiers);
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), play);
        PowerCmd::Apply(PowerId::BLOCK_NEXT_TURN_POWER, ctx, Owner().Creature(), nextTurnAmount, Owner().Creature(), *this);
    }
};
}  // namespace sts2
