#pragma once
#include "game/Models/EncounterModel.h"
#include "game/Models/Monsters/HauntedShip.h"
namespace sts2 {
struct HauntedShipNormal : EncounterModel { SIM_MODEL(HauntedShipNormal, EncounterModel) MonsterSpawnList GenerateMonsters() const { return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::HAUNTED_SHIP), -1}}; } };
}
