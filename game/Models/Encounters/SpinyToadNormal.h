// Models/Encounters/SpinyToadNormal.cs
#pragma once
#include "game/Models/EncounterModel.h"
namespace sts2 {
struct SpinyToadNormal : EncounterModel {
    SIM_MODEL(SpinyToadNormal, EncounterModel)
    MonsterSpawnList GenerateMonsters() const { return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::SPINY_TOAD), -1}}; }
};
}  // namespace sts2
