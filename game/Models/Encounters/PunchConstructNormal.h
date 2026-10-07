#pragma once
#include "game/Models/EncounterModel.h"
#include "game/Models/Monsters/PunchConstruct.h"
namespace sts2 {
struct PunchConstructNormal : EncounterModel { SIM_MODEL(PunchConstructNormal, EncounterModel) MonsterSpawnList GenerateMonsters() const { return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::PUNCH_CONSTRUCT), -1}}; } };
}
