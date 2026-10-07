// Models/Encounters/WaterfallGiantBoss.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct WaterfallGiantBoss : EncounterModel {
    SIM_MODEL(WaterfallGiantBoss, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::WATERFALL_GIANT), -1}};
    }
};

}  // namespace sts2
