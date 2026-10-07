// Models/Encounters/GremlinMercNormal.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct GremlinMercNormal : EncounterModel {
    SIM_MODEL(GremlinMercNormal, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::GREMLIN_MERC), SlotIndex("merc")}};
    }

    void MarkGoldStolen() const { sim_->g->combat.gremlin_merc_gold_was_stolen = true; }

    float CalculateGoldProportion(CombatState combatState) const {
        bool fatEscaped = false;
        for (Creature creature : combatState.EscapedCreatures()) {
            if (creature.Monster().MonsterId_() == MonsterId::FAT_GREMLIN) fatEscaped = true;
        }
        if (!fatEscaped) return 1.0f;
        return sim_->g->combat.gremlin_merc_gold_was_stolen ? 0.0f : 0.5f;
    }
};

}  // namespace sts2
