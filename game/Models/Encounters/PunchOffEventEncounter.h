// Models/Encounters/PunchOffEventEncounter.cs
#pragma once

#include "game/Models/EncounterModel.h"
#include "game/Models/Monsters/PunchConstruct.h"

namespace sts2 {

struct PunchOffEventEncounter : EncounterModel {
    SIM_MODEL(PunchOffEventEncounter, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        PunchConstruct left(MonsterModel::ToMutable(sim_, MonsterId::PUNCH_CONSTRUCT));
        left.SetStartsWithFastPunch(true);
        left.SetStartingHpReduction(Rng().NextInt(2, 10));
        PunchConstruct right(MonsterModel::ToMutable(sim_, MonsterId::PUNCH_CONSTRUCT));
        right.SetStartingHpReduction(Rng().NextInt(2, 10));
        return MonsterSpawnList{{left, -1}, {right, -1}};
    }
};

}  // namespace sts2
