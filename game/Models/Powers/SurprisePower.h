// Models/Powers/SurprisePower.cs
#pragma once

#include "engine/sim/alloc.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/EncounterModel.h"
#include "game/Models/Encounters/GremlinMercNormal.h"
#include "game/Models/MonsterModel.h"
#include "game/Models/Powers/HeistPower.h"
#include "game/Models/PowerModel.h"
#include "game/Models/Powers/ThieveryPower.h"

namespace sts2 {

struct SurprisePower : PowerModel {
    SIM_MODEL(SurprisePower, PowerModel)

    void AfterDeath(PlayerChoiceContext& choiceContext, Creature target, bool wasRemovalPrevented, float) const {
        if (wasRemovalPrevented || Owner() != target) return;
        sts2::CombatState combat = PowerModel::CombatState();
        MonsterModel fatModel = MonsterModel::ToMutable(sim_, MonsterId::FAT_GREMLIN);
        Creature fat = combat.CreateCreature(fatModel, CombatSide::Enemy, EncounterModel(sim_).SlotIndex("fat"));
        int32_t totalStolen = 0;
        for (PowerModel stolen : Owner().GetPowerInstances(PowerId::THIEVERY_POWER)) {
            int32_t intValue = stolen.DynamicVars().Gold().IntValue();
            totalStolen += intValue;
            HeistPower heist(PowerModel(sim_, alloc_power(*sim_->g, PowerId::HEIST_POWER)));
            heist.SetTarget(stolen.Target());
            PowerCmd::Apply(choiceContext, heist, fat, Dec(intValue), Owner(), CardModel{});
        }
        CreatureCmd::Add(MonsterModel::ToMutable(sim_, MonsterId::SNEAKY_GREMLIN), combat, CombatSide::Enemy, "sneaky");
        CreatureCmd::Add(fat);
        if (totalStolen > 0 && combat.Encounter() == EncounterId::GREMLIN_MERC_NORMAL) GremlinMercNormal(sim_, 0).MarkGoldStolen();
    }

    bool ShouldStopCombatFromEnding() const { return true; }
};

}  // namespace sts2
