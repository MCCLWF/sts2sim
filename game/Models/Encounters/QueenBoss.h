// Models/Encounters/QueenBoss.cs
#pragma once
#include "game/Models/EncounterModel.h"
namespace sts2 {
struct QueenBoss : EncounterModel {
    SIM_MODEL(QueenBoss, EncounterModel)
    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::TORCH_HEAD_AMALGAM), SlotIndex("amalgam")},
                                {MonsterModel::ToMutable(sim_, MonsterId::QUEEN), SlotIndex("queen")}};
    }
};
}  // namespace sts2
