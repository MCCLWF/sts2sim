// Models/Encounters/SlimedBerserkerNormal.cs
#pragma once
#include "game/Models/EncounterModel.h"
namespace sts2 {
struct SlimedBerserkerNormal : EncounterModel {
    SIM_MODEL(SlimedBerserkerNormal, EncounterModel)
    MonsterSpawnList GenerateMonsters() const { return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::SLIMED_BERSERKER), -1}}; }
};
}  // namespace sts2
