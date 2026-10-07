// Models/Encounters/CultistsNormal.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct CultistsNormal : EncounterModel {
    SIM_MODEL(CultistsNormal, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::CALCIFIED_CULTIST), -1},
                                {MonsterModel::ToMutable(sim_, MonsterId::DAMP_CULTIST), -1}};
    }
};

}  // namespace sts2
