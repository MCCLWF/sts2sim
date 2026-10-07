// Models/Encounters/NibbitsWeak.cs
#pragma once

#include "game/Models/EncounterModel.h"
#include "game/Models/Monsters/Nibbit.h"

namespace sts2 {

struct NibbitsWeak : EncounterModel {
    SIM_MODEL(NibbitsWeak, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        Nibbit nibbit(MonsterModel::ToMutable(sim_, MonsterId::NIBBIT));
        nibbit.SetIsAlone(true);
        return MonsterSpawnList{{nibbit, -1}};
    }
};

}  // namespace sts2
