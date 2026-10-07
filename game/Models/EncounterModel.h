// Models/EncounterModel.cs. At most one encounter exists at a time (the
// combat's); its instance data lives in CombatData, so the handle carries no
// index. Concrete encounters (game/Models/Encounters/) define GenerateMonsters.
#pragma once

#include <cstdint>
#include <string_view>

#include "content/generated/tables.h"
#include "game/Models/AbstractModel.h"
#include "game/Models/MonsterModel.h"

namespace sts2 {

// One (MonsterModel, string?) of GenerateMonsters: a mutable monster
// (MonsterModel::ToMutable, fields already set by the encounter) and its slot.
struct MonsterSpawn {
    MonsterModel monster;
    int8_t slot = -1;  // index into Slots(); -1 = null
};
using MonsterSpawnList = StaticVec<MonsterSpawn, 8>;

class EncounterModel : public AbstractModel {
public:
    EncounterModel() = default;
    explicit EncounterModel(Sim* sim) : AbstractModel(sim, ModelKind::Encounter, 1) {}
    EncounterModel(Sim* sim, uint16_t) : EncounterModel(sim) {}  // the dispatch's (sim, index) form

    EncounterId EncounterId_() const { return sim_->g->combat.encounter; }
    const EncounterData& data() const { return sts2::data(EncounterId_()); }

    sts2::Rng& Rng() const { return sim_->g->combat.encounter_rng; }
    RoomType RoomType_() const { return data().room_type; }
    bool IsWeak() const { return data().is_weak; }
    bool ShouldGiveRewards() const { return data().gives_rewards; }
    std::span<const std::string_view> Slots() const { return data().slots; }
    int8_t SlotIndex(std::string_view slot) const;  // Slots.IndexOf; -1 when absent

    // GenerateMonstersWithSlots: seeds the encounter Rng with
    // runState.Rng.Seed + TotalFloor + hash(Id.Entry), then GenerateMonsters.
    MonsterSpawnList GenerateMonstersWithSlots(RunState runState);
    void OnCreatureSpawned(Creature creature);
    StaticVec<MonsterId, 16> SpawnedEnemies() const { return sim_->g->combat.spawned_enemies; }
    std::string_view GetNextSlot(sts2::CombatState combatState) const;

    // --- overridable members (EncounterModel.def)
#define SIM_MEMBER(Base, Ret, Name, Params, Args, Default) Ret Name Params const;
#define SIM_MEMBER_CUSTOM(Base, Ret, Name, Params, Args, Default) \
    Ret Name Params const;                                          \
    Ret Name##_Default Params const;
#include "game/Models/EncounterModel.def"
#undef SIM_MEMBER
#undef SIM_MEMBER_CUSTOM
};

}  // namespace sts2
