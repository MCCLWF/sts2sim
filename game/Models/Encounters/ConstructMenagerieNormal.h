// Models/Encounters/ConstructMenagerieNormal.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct ConstructMenagerieNormal : EncounterModel {
    SIM_MODEL(ConstructMenagerieNormal, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        MonsterSpawnList list;
        list.push_back({MonsterModel::ToMutable(sim_, MonsterId::PUNCH_CONSTRUCT), -1});
        list.push_back({MonsterModel::ToMutable(sim_, MonsterId::CUBEX_CONSTRUCT), -1});
        list.push_back({MonsterModel::ToMutable(sim_, MonsterId::CUBEX_CONSTRUCT), -1});
        return list;
    }
};

}  // namespace sts2
