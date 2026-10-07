#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct BowlbugsWeak : EncounterModel {
    SIM_MODEL(BowlbugsWeak, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        StaticVec<MonsterId, 2> workers{MonsterId::BOWLBUG_EGG, MonsterId::BOWLBUG_NECTAR};
        MonsterId worker = Rng().NextItem(workers);
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::BOWLBUG_ROCK), SlotIndex("odd")},
                                {MonsterModel::ToMutable(sim_, worker), SlotIndex("even")}};
    }
};

}  // namespace sts2
