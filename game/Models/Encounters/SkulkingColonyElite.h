#pragma once
#include "game/Models/EncounterModel.h"
#include "game/Models/Monsters/SkulkingColony.h"
namespace sts2 {
struct SkulkingColonyElite : EncounterModel { SIM_MODEL(SkulkingColonyElite, EncounterModel) MonsterSpawnList GenerateMonsters() const { return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::SKULKING_COLONY), -1}}; } };
}
