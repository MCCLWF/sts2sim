#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct GlobeHeadNormal : EncounterModel {
    SIM_MODEL(GlobeHeadNormal, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::GLOBE_HEAD), -1}};
    }
};

}  // namespace sts2
