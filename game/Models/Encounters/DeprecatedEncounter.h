// Models/Encounters/DeprecatedEncounter.cs
#pragma once
#include "game/Models/EncounterModel.h"
namespace sts2 {
struct DeprecatedEncounter : EncounterModel {
    SIM_MODEL(DeprecatedEncounter, EncounterModel)
    MonsterSpawnList GenerateMonsters() const { return {}; }
};
}  // namespace sts2
