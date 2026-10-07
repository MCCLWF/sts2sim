// Models/Encounters/ExoskeletonsNormal.cs
#pragma once
#include "game/Models/EncounterModel.h"
namespace sts2 {
struct ExoskeletonsNormal : EncounterModel {
    SIM_MODEL(ExoskeletonsNormal, EncounterModel)
    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::EXOSKELETON), SlotIndex("first")},
                                {MonsterModel::ToMutable(sim_, MonsterId::EXOSKELETON), SlotIndex("second")},
                                {MonsterModel::ToMutable(sim_, MonsterId::EXOSKELETON), SlotIndex("third")},
                                {MonsterModel::ToMutable(sim_, MonsterId::EXOSKELETON), SlotIndex("fourth")}};
    }
};
}  // namespace sts2
