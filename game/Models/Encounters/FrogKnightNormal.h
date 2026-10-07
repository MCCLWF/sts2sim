// Models/Encounters/FrogKnightNormal.cs
#pragma once
#include "game/Models/EncounterModel.h"
namespace sts2 {
struct FrogKnightNormal : EncounterModel {
    SIM_MODEL(FrogKnightNormal, EncounterModel)
    MonsterSpawnList GenerateMonsters() const { return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::FROG_KNIGHT), -1}}; }
};
}  // namespace sts2
