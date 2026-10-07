// Models/Encounters/ScrollsOfBitingWeak.cs
#pragma once
#include "game/Models/EncounterModel.h"
#include "game/Models/Monsters/ScrollOfBiting.h"
namespace sts2 {
struct ScrollsOfBitingWeak : EncounterModel {
    SIM_MODEL(ScrollsOfBitingWeak, EncounterModel)
    MonsterSpawnList GenerateMonsters() const {
        ScrollOfBiting first(MonsterModel::ToMutable(sim_, MonsterId::SCROLL_OF_BITING));
        ScrollOfBiting second(MonsterModel::ToMutable(sim_, MonsterId::SCROLL_OF_BITING));
        ScrollOfBiting third(MonsterModel::ToMutable(sim_, MonsterId::SCROLL_OF_BITING));
        int32_t index = Rng().NextInt(3);
        first.SetStarterMoveIdx(index);
        second.SetStarterMoveIdx((index + 1) % 3);
        third.SetStarterMoveIdx((index + 2) % 3);
        return MonsterSpawnList{{first, -1}, {second, -1}, {third, -1}};
    }
};
}  // namespace sts2
