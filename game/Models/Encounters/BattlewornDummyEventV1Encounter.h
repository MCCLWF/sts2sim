// Models/Encounters/BattlewornDummyEventV1Encounter.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct BattlewornDummyEventV1Encounter : EncounterModel {
    SIM_MODEL(BattlewornDummyEventV1Encounter, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::BATTLE_FRIEND_V1), -1}};
    }
};

}  // namespace sts2
