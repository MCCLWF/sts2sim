// Models/Encounters/DecimillipedeElite.cs
#pragma once

#include "game/Models/EncounterModel.h"
#include "game/Models/Monsters/DecimillipedeSegmentLogic.h"

namespace sts2 {

struct DecimillipedeElite : EncounterModel {
    SIM_MODEL(DecimillipedeElite, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        MonsterModel front = MonsterModel::ToMutable(sim_, MonsterId::DECIMILLIPEDE_SEGMENT_FRONT);
        MonsterModel middle = MonsterModel::ToMutable(sim_, MonsterId::DECIMILLIPEDE_SEGMENT_MIDDLE);
        MonsterModel back = MonsterModel::ToMutable(sim_, MonsterId::DECIMILLIPEDE_SEGMENT_BACK);
        int32_t start = Rng().NextInt(3);
        DecimillipedeSegmentLogic(front).SetStarterMoveIdx(start);
        DecimillipedeSegmentLogic(middle).SetStarterMoveIdx((start + 1) % 3);
        DecimillipedeSegmentLogic(back).SetStarterMoveIdx((start + 2) % 3);
        return MonsterSpawnList{{front, SlotIndex("segment1")}, {middle, SlotIndex("segment2")}, {back, SlotIndex("segment3")}};
    }
};

}  // namespace sts2
