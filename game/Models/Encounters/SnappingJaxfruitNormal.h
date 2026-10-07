// Models/Encounters/SnappingJaxfruitNormal.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct SnappingJaxfruitNormal : EncounterModel {
    SIM_MODEL(SnappingJaxfruitNormal, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::SNAPPING_JAXFRUIT), -1},
                                {MonsterModel::ToMutable(sim_, MonsterId::FLYCONID), -1}};
    }
};

}  // namespace sts2
