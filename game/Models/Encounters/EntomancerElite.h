// Models/Encounters/EntomancerElite.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct EntomancerElite : EncounterModel {
    SIM_MODEL(EntomancerElite, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::ENTOMANCER), -1}};
    }
};

}  // namespace sts2
