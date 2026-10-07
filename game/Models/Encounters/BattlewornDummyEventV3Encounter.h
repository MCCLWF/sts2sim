// Models/Encounters/BattlewornDummyEventV3Encounter.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct BattlewornDummyEventV3Encounter : EncounterModel {
    SIM_MODEL(BattlewornDummyEventV3Encounter, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::BATTLE_FRIEND_V3), -1}};
    }
};

}  // namespace sts2
