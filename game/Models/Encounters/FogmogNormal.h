// Models/Encounters/FogmogNormal.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct FogmogNormal : EncounterModel {
    SIM_MODEL(FogmogNormal, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::FOGMOG), SlotIndex("fogmog")}};
    }
};

}  // namespace sts2
