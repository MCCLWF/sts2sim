// Models/Encounters/SoulFyshBoss.cs
#pragma once

#include "game/Models/EncounterModel.h"
#include "game/Models/Monsters/SoulFysh.h"

namespace sts2 {

struct SoulFyshBoss : EncounterModel {
    SIM_MODEL(SoulFyshBoss, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::SOUL_FYSH), -1}};
    }
};

}  // namespace sts2
