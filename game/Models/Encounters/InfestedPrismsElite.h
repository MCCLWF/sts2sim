// Models/Encounters/InfestedPrismsElite.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct InfestedPrismsElite : EncounterModel {
    SIM_MODEL(InfestedPrismsElite, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::INFESTED_PRISM), -1}};
    }
};

}  // namespace sts2
