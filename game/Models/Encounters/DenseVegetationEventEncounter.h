// Models/Encounters/DenseVegetationEventEncounter.cs
#pragma once

#include "game/Models/EncounterModel.h"
#include "game/Models/Monsters/Wriggler.h"

namespace sts2 {

struct DenseVegetationEventEncounter : EncounterModel {
    SIM_MODEL(DenseVegetationEventEncounter, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        MonsterSpawnList monsters;
        for (std::string_view slot : Slots()) {
            Wriggler monster(MonsterModel::ToMutable(sim_, MonsterId::WRIGGLER));
            monster.SetStartStunned(false);
            monsters.push_back({monster, SlotIndex(slot)});
        }
        return monsters;
    }
};

}  // namespace sts2
