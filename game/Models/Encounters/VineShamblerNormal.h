// Models/Encounters/VineShamblerNormal.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct VineShamblerNormal : EncounterModel {
    SIM_MODEL(VineShamblerNormal, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::VINE_SHAMBLER), -1}};
    }
};

}  // namespace sts2
