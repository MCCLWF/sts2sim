#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct MysteriousKnightEventEncounter : EncounterModel {
    SIM_MODEL(MysteriousKnightEventEncounter, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::MYSTERIOUS_KNIGHT), -1}};
    }
};

}  // namespace sts2
