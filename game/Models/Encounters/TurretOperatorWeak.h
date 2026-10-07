#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct TurretOperatorWeak : EncounterModel {
    SIM_MODEL(TurretOperatorWeak, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::LIVING_SHIELD), -1},
                                {MonsterModel::ToMutable(sim_, MonsterId::TURRET_OPERATOR), -1}};
    }
};

}  // namespace sts2
