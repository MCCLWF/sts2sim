// Models/Encounters/MawlerNormal.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct MawlerNormal : EncounterModel {
    SIM_MODEL(MawlerNormal, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::MAWLER), -1}};
    }
};

}  // namespace sts2
