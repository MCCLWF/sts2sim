// Models/Encounters/NibbitsNormal.cs
#pragma once

#include "game/Models/EncounterModel.h"
#include "game/Models/Monsters/Nibbit.h"

namespace sts2 {

struct NibbitsNormal : EncounterModel {
    SIM_MODEL(NibbitsNormal, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        Nibbit front(MonsterModel::ToMutable(sim_, MonsterId::NIBBIT));
        front.SetIsFront(true);
        return MonsterSpawnList{{front, 0},
                                {MonsterModel::ToMutable(sim_, MonsterId::NIBBIT), 1}};
    }
};

}  // namespace sts2
