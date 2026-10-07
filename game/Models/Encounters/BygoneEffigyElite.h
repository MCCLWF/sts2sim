// Models/Encounters/BygoneEffigyElite.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct BygoneEffigyElite : EncounterModel {
    SIM_MODEL(BygoneEffigyElite, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::BYGONE_EFFIGY), -1}};
    }
};

}  // namespace sts2
