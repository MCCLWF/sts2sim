// Models/Relics/DragonFruit.cs
#pragma once
#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"
#include "game/Runs/RunState.h"
namespace sts2 {
struct DragonFruit : RelicModel {
    SIM_MODEL(DragonFruit, RelicModel)
    void AfterGoldGained(Player player) const {
        if (player == Owner()) CreatureCmd::GainMaxHp(Owner().Creature(), DynamicVars().MaxHp().BaseValue());
    }
    bool IsAllowed(RunState runState) const { return IsBeforeAct3TreasureChest(runState); }
};
}
