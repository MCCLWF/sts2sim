// Models/Encounters/OvergrowthCrawlers.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct OvergrowthCrawlers : EncounterModel {
    SIM_MODEL(OvergrowthCrawlers, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::SHRINKER_BEETLE), -1},
                                {MonsterModel::ToMutable(sim_, MonsterId::FUZZY_WURM_CRAWLER), -1}};
    }
};

}  // namespace sts2
