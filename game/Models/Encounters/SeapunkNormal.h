// Models/Encounters/SeapunkNormal.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct SeapunkNormal : EncounterModel {
    SIM_MODEL(SeapunkNormal, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::CALCIFIED_CULTIST), -1},
                                {MonsterModel::ToMutable(sim_, MonsterId::SEAPUNK), -1}};
    }
};

}  // namespace sts2
