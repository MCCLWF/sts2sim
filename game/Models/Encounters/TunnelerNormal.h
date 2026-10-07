// Models/Encounters/TunnelerNormal.cs
#pragma once
#include "game/Models/EncounterModel.h"
#include "game/Models/Monsters/Chomper.h"
namespace sts2 {
struct TunnelerNormal : EncounterModel {
    SIM_MODEL(TunnelerNormal, EncounterModel)
    MonsterSpawnList GenerateMonsters() const {
        MonsterModel chomper = MonsterModel::ToMutable(sim_, MonsterId::CHOMPER);
        Chomper(chomper).SetScreamFirst(true);
        return MonsterSpawnList{{chomper, -1}, {MonsterModel::ToMutable(sim_, MonsterId::TUNNELER), -1}};
    }
};
}  // namespace sts2
