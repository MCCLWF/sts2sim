// Models/Encounters/TheObscuraNormal.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct TheObscuraNormal : EncounterModel {
    SIM_MODEL(TheObscuraNormal, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::THE_OBSCURA), SlotIndex("obscura")}};
    }
};

}  // namespace sts2
