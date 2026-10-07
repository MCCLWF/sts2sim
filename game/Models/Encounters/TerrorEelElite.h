#pragma once
#include "game/Models/EncounterModel.h"
#include "game/Models/Monsters/TerrorEel.h"
namespace sts2 {
struct TerrorEelElite : EncounterModel { SIM_MODEL(TerrorEelElite, EncounterModel) MonsterSpawnList GenerateMonsters() const { return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::TERROR_EEL), -1}}; } };
}
