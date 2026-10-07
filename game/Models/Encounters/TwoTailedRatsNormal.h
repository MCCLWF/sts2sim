// Models/Encounters/TwoTailedRatsNormal.cs
#pragma once

#include "game/Models/EncounterModel.h"
#include "game/Models/Monsters/TwoTailedRat.h"

namespace sts2 {

struct TwoTailedRatsNormal : EncounterModel {
    SIM_MODEL(TwoTailedRatsNormal, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        TwoTailedRat first(MonsterModel::ToMutable(sim_, MonsterId::TWO_TAILED_RAT));
        TwoTailedRat second(MonsterModel::ToMutable(sim_, MonsterId::TWO_TAILED_RAT));
        TwoTailedRat third(MonsterModel::ToMutable(sim_, MonsterId::TWO_TAILED_RAT));
        int32_t index = Rng().NextInt(3);
        first.SetStarterMoveIndex(index);
        second.SetStarterMoveIndex((index + 1) % 3);
        third.SetStarterMoveIndex((index + 2) % 3);
        return MonsterSpawnList{{first, SlotIndex("third")},
                                {second, SlotIndex("fourth")},
                                {third, SlotIndex("fifth")}};
    }
};

}  // namespace sts2
