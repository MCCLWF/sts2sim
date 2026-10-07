#pragma once

#include "game/Models/EncounterModel.h"

namespace sts2 {

struct KnightsElite : EncounterModel {
    SIM_MODEL(KnightsElite, EncounterModel)

    MonsterSpawnList GenerateMonsters() const {
        return MonsterSpawnList{
            {MonsterModel::ToMutable(sim_, MonsterId::FLAIL_KNIGHT), SlotIndex("first")},
            {MonsterModel::ToMutable(sim_, MonsterId::SPECTRAL_KNIGHT), SlotIndex("second")},
            {MonsterModel::ToMutable(sim_, MonsterId::MAGI_KNIGHT), SlotIndex("third")},
        };
    }
};

}  // namespace sts2
