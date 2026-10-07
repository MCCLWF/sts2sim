// Models/Encounters/ExoskeletonsWeak.cs
#pragma once
#include "game/Models/EncounterModel.h"
namespace sts2 {
struct ExoskeletonsWeak : EncounterModel {
    SIM_MODEL(ExoskeletonsWeak, EncounterModel)
    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::EXOSKELETON), SlotIndex("first")},
                                {MonsterModel::ToMutable(sim_, MonsterId::EXOSKELETON), SlotIndex("second")},
                                {MonsterModel::ToMutable(sim_, MonsterId::EXOSKELETON), SlotIndex("third")}};
    }
};
}  // namespace sts2
