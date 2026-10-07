// Models/Encounters/TheKinBoss.cs
#pragma once

#include "game/Models/EncounterModel.h"
#include "game/Models/Monsters/KinFollower.h"
#include "game/Models/Monsters/KinPriest.h"

namespace sts2 {

struct TheKinBoss : EncounterModel {
    SIM_MODEL(TheKinBoss, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        KinFollower dancer(MonsterModel::ToMutable(sim_, MonsterId::KIN_FOLLOWER));
        dancer.SetStartsWithDance(true);
        return MonsterSpawnList{{dancer, SlotIndex("slot1")},
                                {MonsterModel::ToMutable(sim_, MonsterId::KIN_FOLLOWER), SlotIndex("slot2")},
                                {MonsterModel::ToMutable(sim_, MonsterId::KIN_PRIEST), SlotIndex("leaderSlot")}};
    }
};

}  // namespace sts2
