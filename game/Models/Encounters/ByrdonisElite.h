// Models/Encounters/ByrdonisElite.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct ByrdonisElite : EncounterModel {
    SIM_MODEL(ByrdonisElite, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::BYRDONIS), -1}};
    }
};

}  // namespace sts2
