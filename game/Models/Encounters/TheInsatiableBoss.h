// Models/Encounters/TheInsatiableBoss.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct TheInsatiableBoss : EncounterModel {
    SIM_MODEL(TheInsatiableBoss, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::THE_INSATIABLE), -1}};
    }
};

}  // namespace sts2
