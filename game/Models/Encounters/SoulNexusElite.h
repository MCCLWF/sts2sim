// Models/Encounters/SoulNexusElite.cs
#pragma once
#include "game/Models/EncounterModel.h"
namespace sts2 {
struct SoulNexusElite : EncounterModel {
    SIM_MODEL(SoulNexusElite, EncounterModel)
    MonsterSpawnList GenerateMonsters() const { return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::SOUL_NEXUS), -1}}; }
};
}  // namespace sts2
