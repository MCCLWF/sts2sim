// Models/Encounters/SeapunkWeak.cs
#pragma once

#include "game/Models/EncounterModel.h"
#include "game/Models/MonsterModel.h"

namespace sts2 {

struct SeapunkWeak : EncounterModel {
    SIM_MODEL(SeapunkWeak, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::SEAPUNK), -1}};
    }
};

}  // namespace sts2
