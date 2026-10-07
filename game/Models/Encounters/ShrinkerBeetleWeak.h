// Models/Encounters/ShrinkerBeetleWeak.cs
#pragma once

#include "game/Models/EncounterModel.h"
#include "game/Models/MonsterModel.h"

namespace sts2 {

struct ShrinkerBeetleWeak : EncounterModel {
    SIM_MODEL(ShrinkerBeetleWeak, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::SHRINKER_BEETLE), -1}};
    }
};

}  // namespace sts2
