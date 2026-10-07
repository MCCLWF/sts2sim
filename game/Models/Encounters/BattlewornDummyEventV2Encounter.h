// Models/Encounters/BattlewornDummyEventV2Encounter.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct BattlewornDummyEventV2Encounter : EncounterModel {
    SIM_MODEL(BattlewornDummyEventV2Encounter, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::BATTLE_FRIEND_V2), -1}};
    }
};

}  // namespace sts2
