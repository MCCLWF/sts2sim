// Models/Encounters/TheArchitectEventEncounter.cs
#pragma once
#include "game/Models/EncounterModel.h"
namespace sts2 {
struct TheArchitectEventEncounter : EncounterModel {
    SIM_MODEL(TheArchitectEventEncounter, EncounterModel)
    MonsterSpawnList GenerateMonsters() const { return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::ARCHITECT), -1}}; }
};
}  // namespace sts2
