// Models/Encounters/FlyconidNormal.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct FlyconidNormal : EncounterModel {
    SIM_MODEL(FlyconidNormal, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        StaticVec<MonsterId, 2> medium{MonsterId::LEAF_SLIME_M, MonsterId::TWIG_SLIME_M};
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, Rng().NextItem(medium)), -1},
                                {MonsterModel::ToMutable(sim_, MonsterId::FLYCONID), -1}};
    }
};

}  // namespace sts2
