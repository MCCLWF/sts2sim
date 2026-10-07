// Models/EncounterModel.cs
#include "game/Models/EncounterModel.h"

#include "core/xxhash64.h"
#include "game/All.h"

namespace sts2 {

int8_t EncounterModel::SlotIndex(std::string_view slot) const {
    std::span<const std::string_view> slots = Slots();
    for (size_t i = 0; i < slots.size(); ++i) {
        if (slots[i] == slot) return static_cast<int8_t>(i);
    }
    return -1;
}

// Slots.FirstOrDefault(s => Enemies.All(c => c.SlotName != s), "")
std::string_view EncounterModel::GetNextSlot(sts2::CombatState combatState) const {
    for (std::string_view s : Slots()) {
        bool free = true;
        for (Creature c : combatState.Enemies()) free &= c.SlotName() != s;
        if (free) return s;
    }
    return {};
}

MonsterSpawnList EncounterModel::GenerateMonstersWithSlots(RunState runState) {
    // _rng is null for a fresh encounter (monsters are generated once per combat).
    uint64_t seed = static_cast<uint64_t>(static_cast<int64_t>(runState.Rng().seed) + runState.TotalFloor()) +
                    deterministic_hash(data().entry);
    sim_->g->combat.encounter_rng = sts2::Rng(seed);
    return GenerateMonsters();
}

void EncounterModel::OnCreatureSpawned(Creature creature) {
    if (creature.Side() != CombatSide::Enemy) return;
    MonsterId monsterModel = creature.Monster().MonsterId_();  // CanonicalInstance
    auto& spawned = sim_->g->combat.spawned_enemies;
    if (!spawned.contains(monsterModel)) spawned.push_back(monsterModel);
}

float EncounterModel::CalculateGoldProportion_Default(sts2::CombatState combatState) const {
    return 1.0f - static_cast<float>(combatState.EscapedCreatures().size()) / static_cast<float>(SpawnedEnemies().size());
}

}  // namespace sts2
