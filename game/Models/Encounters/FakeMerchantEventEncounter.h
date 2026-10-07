// Models/Encounters/FakeMerchantEventEncounter.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct FakeMerchantEventEncounter : EncounterModel {
    SIM_MODEL(FakeMerchantEventEncounter, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::FAKE_MERCHANT_MONSTER), SlotIndex("merchant")}};
    }
};

}  // namespace sts2
