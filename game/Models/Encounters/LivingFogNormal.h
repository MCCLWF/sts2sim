// Models/Encounters/LivingFogNormal.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct LivingFogNormal : EncounterModel {
    SIM_MODEL(LivingFogNormal, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::LIVING_FOG), SlotIndex("livingFog")}};
    }
};

}  // namespace sts2
