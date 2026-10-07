// Models/Encounters/HunterKillerNormal.cs
#pragma once
#include "game/Models/EncounterModel.h"
namespace sts2 {
struct HunterKillerNormal : EncounterModel {
    SIM_MODEL(HunterKillerNormal, EncounterModel)
    MonsterSpawnList GenerateMonsters() const { return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::HUNTER_KILLER), -1}}; }
};
}  // namespace sts2
