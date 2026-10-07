// Models/Encounters/TheLostAndForgottenNormal.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct TheLostAndForgottenNormal : EncounterModel {
    SIM_MODEL(TheLostAndForgottenNormal, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{
            {MonsterModel::ToMutable(sim_, MonsterId::THE_LOST), -1},
            {MonsterModel::ToMutable(sim_, MonsterId::THE_FORGOTTEN), -1},
        };
    }
};

}  // namespace sts2
