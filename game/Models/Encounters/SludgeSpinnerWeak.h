// Models/Encounters/SludgeSpinnerWeak.cs
#pragma once

#include "game/Models/EncounterModel.h"
#include "game/Models/MonsterModel.h"

namespace sts2 {

struct SludgeSpinnerWeak : EncounterModel {
    SIM_MODEL(SludgeSpinnerWeak, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::SLUDGE_SPINNER), -1}};
    }
};

}  // namespace sts2
