// Models/Encounters/CubexConstructNormal.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct CubexConstructNormal : EncounterModel {
    SIM_MODEL(CubexConstructNormal, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::CUBEX_CONSTRUCT), -1}};
    }
};

}  // namespace sts2
