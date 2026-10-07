// Models/Encounters/FuzzyWurmCrawlerWeak.cs
#pragma once

#include "game/Models/EncounterModel.h"
#include "game/Models/MonsterModel.h"

namespace sts2 {

struct FuzzyWurmCrawlerWeak : EncounterModel {
    SIM_MODEL(FuzzyWurmCrawlerWeak, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::FUZZY_WURM_CRAWLER), -1}};
    }
};

}  // namespace sts2
