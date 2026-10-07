#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct AxebotsNormal : EncounterModel {
    SIM_MODEL(AxebotsNormal, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::AXEBOT), SlotIndex("front")}};
    }
};

}  // namespace sts2
