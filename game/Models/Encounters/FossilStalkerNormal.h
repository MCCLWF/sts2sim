#pragma once
#include "game/Models/EncounterModel.h"
#include "game/Models/Monsters/FossilStalker.h"
namespace sts2 {
struct FossilStalkerNormal : EncounterModel { SIM_MODEL(FossilStalkerNormal, EncounterModel) MonsterSpawnList GenerateMonsters() const { return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::FOSSIL_STALKER), -1}}; } };
}
