// Models/Encounters/ChompersNormal.cs
#pragma once
#include "game/Models/EncounterModel.h"
#include "game/Models/Monsters/Chomper.h"
namespace sts2 {
struct ChompersNormal : EncounterModel {
    SIM_MODEL(ChompersNormal, EncounterModel)
    MonsterSpawnList GenerateMonsters() const {
        MonsterModel first = MonsterModel::ToMutable(sim_, MonsterId::CHOMPER);
        MonsterModel second = MonsterModel::ToMutable(sim_, MonsterId::CHOMPER);
        Chomper(second).SetScreamFirst(true);
        return MonsterSpawnList{{first, -1}, {second, -1}};
    }
};
}  // namespace sts2
