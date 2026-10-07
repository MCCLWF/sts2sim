// Models/Relics/StoneHumidifier.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Commands/CreatureCmd.h"
namespace sts2 {
struct StoneHumidifier : RelicModel {
    SIM_MODEL(StoneHumidifier, RelicModel)
    void AfterRestSiteHeal(Player player, bool isMimicked) const { (void)isMimicked; if (player == Owner()) CreatureCmd::GainMaxHp(Owner().Creature(), DynamicVars().MaxHp().BaseValue()); }
};
}
