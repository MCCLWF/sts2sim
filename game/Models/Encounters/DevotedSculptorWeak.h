// Models/Encounters/DevotedSculptorWeak.cs
#pragma once
#include "game/Models/EncounterModel.h"
namespace sts2 {
struct DevotedSculptorWeak : EncounterModel {
    SIM_MODEL(DevotedSculptorWeak, EncounterModel)
    MonsterSpawnList GenerateMonsters() const { return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::DEVOTED_SCULPTOR), -1}}; }
};
}  // namespace sts2
