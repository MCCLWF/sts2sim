// Models/Encounters/CorpseSlugsWeak.cs
#pragma once

#include "game/Models/EncounterModel.h"
#include "game/Models/Monsters/CorpseSlug.h"

namespace sts2 {

struct CorpseSlugsWeak : EncounterModel {
    SIM_MODEL(CorpseSlugsWeak, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        MonsterSpawnList list2{{MonsterModel::ToMutable(sim_, MonsterId::CORPSE_SLUG), -1},
                               {MonsterModel::ToMutable(sim_, MonsterId::CORPSE_SLUG), -1}};
        StaticVec<MonsterModel, 8> monsters;
        for (const MonsterSpawn& kvp : list2) monsters.push_back(kvp.monster);
        CorpseSlug::EnsureCorpseSlugsStartWithDifferentMoves(monsters, Rng());
        return list2;
    }
};

}  // namespace sts2
