// Models/Encounters/SlimesWeak.cs
#pragma once

#include "game/Models/EncounterModel.h"
#include "game/Models/MonsterModel.h"

namespace sts2 {

struct SlimesWeak : EncounterModel {
    SIM_MODEL(SlimesWeak, EncounterModel)

    static constexpr MonsterId _smallSlimes[2] = {MonsterId::LEAF_SLIME_S, MonsterId::TWIG_SLIME_S};
    static constexpr MonsterId _mediumSlimes[2] = {MonsterId::LEAF_SLIME_M, MonsterId::TWIG_SLIME_M};

    MonsterSpawnList GenerateMonsters() const {
        MonsterSpawnList list;
        StaticVec<MonsterId, 2> list2{_smallSlimes[0], _smallSlimes[1]};
        MonsterId monsterModel = Rng().NextItem(list2);
        list2.remove(monsterModel);
        MonsterId monsterModel2 = Rng().NextItem(list2);
        list.push_back({MonsterModel::ToMutable(sim_, monsterModel), -1});
        StaticVec<MonsterId, 2> medium{_mediumSlimes[0], _mediumSlimes[1]};
        list.push_back({MonsterModel::ToMutable(sim_, Rng().NextItem(medium)), -1});
        list.push_back({MonsterModel::ToMutable(sim_, monsterModel2), -1});
        return list;
    }
};

}  // namespace sts2
