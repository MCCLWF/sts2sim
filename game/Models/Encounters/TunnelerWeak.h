// Models/Encounters/TunnelerWeak.cs
#pragma once
#include "game/Models/EncounterModel.h"
namespace sts2 {
struct TunnelerWeak : EncounterModel {
    SIM_MODEL(TunnelerWeak, EncounterModel)
    MonsterSpawnList GenerateMonsters() const { return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::TUNNELER), -1}}; }
};
}  // namespace sts2
