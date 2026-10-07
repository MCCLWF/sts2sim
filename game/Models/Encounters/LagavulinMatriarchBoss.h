// Models/Encounters/LagavulinMatriarchBoss.cs
#pragma once

#include "game/Models/EncounterModel.h"
#include "game/Models/Monsters/LagavulinMatriarch.h"

namespace sts2 {

struct LagavulinMatriarchBoss : EncounterModel {
    SIM_MODEL(LagavulinMatriarchBoss, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::LAGAVULIN_MATRIARCH), -1}};
    }
};

}  // namespace sts2
