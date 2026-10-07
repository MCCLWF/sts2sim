// Models/Encounters/FabricatorNormal.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct FabricatorNormal : EncounterModel {
    SIM_MODEL(FabricatorNormal, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::FABRICATOR), SlotIndex("fabricator")}};
    }
};

}  // namespace sts2
