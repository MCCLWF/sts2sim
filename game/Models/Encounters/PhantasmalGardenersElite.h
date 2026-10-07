#pragma once
#include "game/Models/EncounterModel.h"
#include "game/Models/Monsters/PhantasmalGardener.h"
namespace sts2 {
struct PhantasmalGardenersElite : EncounterModel {
    SIM_MODEL(PhantasmalGardenersElite, EncounterModel)
    MonsterSpawnList GenerateMonsters() const { return MonsterSpawnList{{MonsterModel::ToMutable(sim_, MonsterId::PHANTASMAL_GARDENER), SlotIndex("first")}, {MonsterModel::ToMutable(sim_, MonsterId::PHANTASMAL_GARDENER), SlotIndex("second")}, {MonsterModel::ToMutable(sim_, MonsterId::PHANTASMAL_GARDENER), SlotIndex("third")}, {MonsterModel::ToMutable(sim_, MonsterId::PHANTASMAL_GARDENER), SlotIndex("fourth")}}; }
};
}
