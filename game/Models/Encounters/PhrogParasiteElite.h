// Models/Encounters/PhrogParasiteElite.cs
#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct PhrogParasiteElite : EncounterModel {
    SIM_MODEL(PhrogParasiteElite, EncounterModel)

    static std::string_view GetWrigglerSlotName(int32_t index) {
        static constexpr std::string_view slots[4] = {"wriggler1", "wriggler2", "wriggler3", "wriggler4"};
        if (index < 0 || index >= 4) throw GameError("invalid wriggler index");
        return slots[index];
    }

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::PHROG_PARASITE), SlotIndex("phrog")}};
    }
};

}  // namespace sts2
