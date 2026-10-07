#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct CeremonialBeastBoss : EncounterModel {
    SIM_MODEL(CeremonialBeastBoss, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::CEREMONIAL_BEAST), -1}};
    }
};

}  // namespace sts2
