// Models/Encounters/ToadpolesWeak.cs
#pragma once

#include "game/Models/EncounterModel.h"
#include "game/Models/Monsters/Toadpole.h"

namespace sts2 {

struct ToadpolesWeak : EncounterModel {
    SIM_MODEL(ToadpolesWeak, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        Toadpole toadpole(MonsterModel::ToMutable(sim_, MonsterId::TOADPOLE));
        toadpole.SetIsFront(true);
        Toadpole toadpole2(MonsterModel::ToMutable(sim_, MonsterId::TOADPOLE));
        toadpole2.SetIsFront(false);
        return MonsterSpawnList{{toadpole, -1}, {toadpole2, -1}};
    }
};

}  // namespace sts2
