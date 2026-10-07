// Models/Encounters/VantomBoss.cs
#pragma once

#include "game/Models/EncounterModel.h"
#include "game/Models/Monsters/Vantom.h"

namespace sts2 {

struct VantomBoss : EncounterModel {
    SIM_MODEL(VantomBoss, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::VANTOM), -1}};
    }
};

}  // namespace sts2
