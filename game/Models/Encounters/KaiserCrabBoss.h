// Models/Encounters/KaiserCrabBoss.cs
#pragma once
#include "game/Models/EncounterModel.h"
namespace sts2 {
struct KaiserCrabBoss : EncounterModel {
    SIM_MODEL(KaiserCrabBoss, EncounterModel)
    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::CRUSHER), SlotIndex("crusher")},
                                {MonsterModel::ToMutable(sim_, MonsterId::ROCKET), SlotIndex("rocket")}};
    }
};
}  // namespace sts2
