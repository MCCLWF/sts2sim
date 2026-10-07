#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct OwlMagistrateNormal : EncounterModel {
    SIM_MODEL(OwlMagistrateNormal, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::OWL_MAGISTRATE), -1}};
    }
};

}  // namespace sts2
