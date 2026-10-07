#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct SlumberingBeetleNormal : EncounterModel {
    SIM_MODEL(SlumberingBeetleNormal, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::BOWLBUG_ROCK), SlotIndex("first")},
                                {MonsterModel::ToMutable(sim_, MonsterId::BOWLBUG_SILK), SlotIndex("second")},
                                {MonsterModel::ToMutable(sim_, MonsterId::SLUMBERING_BEETLE), SlotIndex("third")}};
    }
};

}  // namespace sts2
