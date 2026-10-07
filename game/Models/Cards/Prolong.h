// Models/Cards/Prolong.cs
#pragma once
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Prolong : CardModel {
    SIM_MODEL(Prolong, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        Creature creature = Owner().Creature();
        PowerCmd::Apply(PowerId::BLOCK_NEXT_TURN_POWER, ctx, creature, Dec(creature.Block()), creature, *this);
    }
};
}
