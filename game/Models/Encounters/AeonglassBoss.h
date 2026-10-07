#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct AeonglassBoss : EncounterModel {
    SIM_MODEL(AeonglassBoss, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::AEONGLASS), -1}};
    }
};

}  // namespace sts2
