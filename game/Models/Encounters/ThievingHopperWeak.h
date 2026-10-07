// Models/Encounters/ThievingHopperWeak.cs
#pragma once
#include "game/Models/EncounterModel.h"
namespace sts2 {
struct ThievingHopperWeak : EncounterModel {
    SIM_MODEL(ThievingHopperWeak, EncounterModel)
    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::THIEVING_HOPPER), -1}};
    }
};
}  // namespace sts2
