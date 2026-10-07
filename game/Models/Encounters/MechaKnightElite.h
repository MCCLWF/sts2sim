// Models/Encounters/MechaKnightElite.cs
#pragma once

#include "game/Models/EncounterModel.h"
#include "game/Models/Monsters/MechaKnight.h"

namespace sts2 {

struct MechaKnightElite : EncounterModel {
    SIM_MODEL(MechaKnightElite, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::MECHA_KNIGHT), -1}};
    }
};

}  // namespace sts2
