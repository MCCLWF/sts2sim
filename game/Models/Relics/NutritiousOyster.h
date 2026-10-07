// Models/Relics/NutritiousOyster.cs
#pragma once
#include "game/Models/RelicModel.h"
#include "game/Commands/CreatureCmd.h"
namespace sts2 {
struct NutritiousOyster : RelicModel {
    SIM_MODEL(NutritiousOyster, RelicModel)
    bool HasUponPickupEffect() const { return true; }
    void AfterObtained() const { CreatureCmd::GainMaxHp(Owner().Creature(), DynamicVars().MaxHp().BaseValue()); }
};
}
