// Rooms/AbstractRoom.cs and Rooms/CombatRoom.cs (the parts hooks read).
// The current room is recorded in RunData; a room handle reads it there.
#pragma once

#include <cstdint>

#include "engine/sim/sim.h"
#include "game/fwd.h"

namespace sts2 {

class AbstractRoom {
public:
    AbstractRoom() = default;
    AbstractRoom(Sim* sim, sts2::RoomType type) : sim_(sim), type_(type) {
        if (sim && IsCombatRoom()) {
            // A room passed to EventModel.Resume is already off the stack and
            // CombatManager.Reset has cleared the current combat. Keep the
            // exited room's facts on the value handle, as the game does with
            // its retained CombatRoom instance.
            encounter_ = sim->g->combat.encounter;
            encounter_ran_out_of_time_ = sim->g->combat.battleworn_dummy_ran_out_of_time;
            extra_rewards_ = sim->g->run.combat_extra_rewards;
        }
    }

    explicit operator bool() const { return sim_ != nullptr; }
    Sim& sim() const { return *sim_; }
    sts2::RoomType RoomType() const { return type_; }
    bool IsCombatRoom() const {
        return type_ == sts2::RoomType::Monster || type_ == sts2::RoomType::Elite || type_ == sts2::RoomType::Boss;
    }
    EncounterId ExitedEncounter() const { return encounter_; }
    bool EncounterRanOutOfTime() const { return encounter_ran_out_of_time_; }
    const StaticVec<RewardRef, 8>& ExtraRewards() const { return extra_rewards_; }

protected:
    Sim* sim_ = nullptr;
    sts2::RoomType type_{};
    EncounterId encounter_ = EncounterId::NONE;
    bool encounter_ran_out_of_time_ = false;
    StaticVec<RewardRef, 8> extra_rewards_;
};

class CombatRoom : public AbstractRoom {
public:
    CombatRoom() = default;
    CombatRoom(Sim* sim, sts2::RoomType type) : AbstractRoom(sim, type) {}
    sts2::CombatState CombatState() const;
    EncounterId Encounter() const;
};

}  // namespace sts2
