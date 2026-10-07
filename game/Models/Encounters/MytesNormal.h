// Models/Encounters/MytesNormal.cs
#pragma once
#include "game/Models/EncounterModel.h"
namespace sts2 {
struct MytesNormal : EncounterModel {
    SIM_MODEL(MytesNormal, EncounterModel)
    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::MYTE), SlotIndex("first")},
                                {MonsterModel::ToMutable(sim_, MonsterId::MYTE), SlotIndex("second")}};
    }
};
}  // namespace sts2
