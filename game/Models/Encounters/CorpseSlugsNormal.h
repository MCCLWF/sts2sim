// Models/Encounters/CorpseSlugsNormal.cs
#pragma once
#include "game/Models/EncounterModel.h"
#include "game/Models/Monsters/CorpseSlug.h"
namespace sts2 {
struct CorpseSlugsNormal : EncounterModel {
    SIM_MODEL(CorpseSlugsNormal, EncounterModel)
    MonsterSpawnList GenerateMonsters() const {
        MonsterSpawnList list{{MonsterModel::ToMutable(sim_, MonsterId::CORPSE_SLUG), -1}, {MonsterModel::ToMutable(sim_, MonsterId::CORPSE_SLUG), -1}, {MonsterModel::ToMutable(sim_, MonsterId::CORPSE_SLUG), -1}};
        StaticVec<MonsterModel, 8> monsters; for (const MonsterSpawn& s : list) monsters.push_back(s.monster);
        CorpseSlug::EnsureCorpseSlugsStartWithDifferentMoves(monsters, Rng()); return list;
    }
};
}
