// Models/Relics/PhilosophersStone.cs (v0.111.0).
#pragma once

#include "game/Commands/PowerCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct PhilosophersStone : RelicModel {
    SIM_MODEL(PhilosophersStone, RelicModel)

    Dec ModifyMaxEnergy(Player player, Dec amount) const {
        return player == Owner() ? amount + Dec(DynamicVars()["Energy"].IntValue()) : amount;
    }
    void AfterCreatureAddedToCombat(Creature creature) const {
        if (creature.Side() == Owner().Creature().Side()) return;
        PlayerChoiceContext context = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, context, creature,
                        DynamicVars()["StrengthPower"].BaseValue(), Creature{}, CardModel{});
    }
    void AfterRoomEntered(AbstractRoom room) const {
        if (!room.IsCombatRoom()) return;
        CombatState combat(&sim());
        CreatureList targets;
        for (Creature creature : combat.GetOpponentsOf(Owner().Creature()))
            if (creature.IsAlive()) targets.push_back(creature);
        PlayerChoiceContext context = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::STRENGTH_POWER, context, targets,
                        DynamicVars()["StrengthPower"].BaseValue(), Creature{}, CardModel{});
    }
};

}  // namespace sts2
