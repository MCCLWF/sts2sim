// Models/Encounters/InkletsNormal.cs
#pragma once

#include "game/Models/EncounterModel.h"
#include "game/Models/Monsters/Inklet.h"

namespace sts2 {

struct InkletsNormal : EncounterModel {
    SIM_MODEL(InkletsNormal, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        Inklet middle(MonsterModel::ToMutable(sim_, MonsterId::INKLET));
        middle.SetMiddleInklet(true);
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::INKLET), -1},
                                {middle, -1},
                                {MonsterModel::ToMutable(sim_, MonsterId::INKLET), -1}};
    }
};

}  // namespace sts2
