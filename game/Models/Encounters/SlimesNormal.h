// Models/Encounters/SlimesNormal.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct SlimesNormal : EncounterModel {
    SIM_MODEL(SlimesNormal, EncounterModel)

    static constexpr MonsterId _smallSlimes[2] = {MonsterId::LEAF_SLIME_S, MonsterId::TWIG_SLIME_S};

    MonsterSpawnList GenerateMonsters() const {
        bool flag = Rng().NextBool();
        MonsterId first = flag ? MonsterId::LEAF_SLIME_S : MonsterId::TWIG_SLIME_S;
        MonsterId second = flag ? MonsterId::TWIG_SLIME_S : MonsterId::LEAF_SLIME_S;
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::TWIG_SLIME_M), -1},
                                {MonsterModel::ToMutable(sim_, MonsterId::LEAF_SLIME_M), -1},
                                {MonsterModel::ToMutable(sim_, first), -1},
                                {MonsterModel::ToMutable(sim_, second), -1}};
    }
};

}  // namespace sts2
