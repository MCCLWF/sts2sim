#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct BowlbugsNormal : EncounterModel {
    SIM_MODEL(BowlbugsNormal, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        StaticVec<MonsterId, 3> workers{MonsterId::BOWLBUG_EGG, MonsterId::BOWLBUG_SILK, MonsterId::BOWLBUG_NECTAR};
        MonsterSpawnList result{{MonsterModel::ToMutable(sim_, MonsterId::BOWLBUG_ROCK), SlotIndex("first")}};
        for (int i = 0; i < 2; ++i) {
            MonsterId chosen = Rng().NextItem(workers);
            workers.remove(chosen);
            result.push_back({MonsterModel::ToMutable(sim_, chosen), SlotIndex(i == 0 ? "middle" : "last")});
        }
        return result;
    }
};

}  // namespace sts2
