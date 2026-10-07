#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct LouseProgenitorNormal : EncounterModel {
    SIM_MODEL(LouseProgenitorNormal, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::LOUSE_PROGENITOR), -1}};
    }
};

}  // namespace sts2
