// Models/Encounters/OvicopterNormal.cs
#pragma once

#include "game/Models/EncounterModel.h"
#include "game/Models/Monsters/Ovicopter.h"

namespace sts2 {

struct OvicopterNormal : EncounterModel {
    SIM_MODEL(OvicopterNormal, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::OVICOPTER), SlotIndex("ovicopter")}};
    }
};

}  // namespace sts2
