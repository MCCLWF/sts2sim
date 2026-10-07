#pragma once
#include "game/Models/EncounterModel.h"
#include "game/Models/Monsters/SewerClam.h"
namespace sts2 {
struct SewerClamNormal : EncounterModel { SIM_MODEL(SewerClamNormal, EncounterModel) MonsterSpawnList GenerateMonsters() const { return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::SEWER_CLAM), -1}}; } };
}
