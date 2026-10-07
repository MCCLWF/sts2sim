// Models/Potions/FoulPotion.cs (combat and ordinary merchant paths)
#pragma once

#include "engine/sim/errors.h"
#include "game/Combat/CombatManager.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Models/PotionModel.h"
#include "game/Models/Events/FakeMerchant.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct FoulPotion : PotionModel {
    SIM_MODEL(FoulPotion, PotionModel)

    bool PassesCustomUsabilityCheck() const {
        if (CombatManager(sim_).IsInProgress()) return true;
        const RunData& run = Owner().RunState().st();
        if (run.rooms.empty()) return false;
        return run.rooms.back() == RoomType::Shop ||
               (run.rooms.back() == RoomType::Event && run.room_event == EventId::FAKE_MERCHANT);
    }

    void OnUse(PlayerChoiceContext& choiceContext, Creature) const {
        if (CombatManager(sim_).IsInProgress()) {
            CreatureList targets;
            for (Creature creature : Owner().Creature().CombatState().Creatures())
                if (!creature.IsPet()) targets.push_back(creature);
            CreatureCmd::Damage(choiceContext, targets, DynamicVars().Damage().BaseValue(),
                                DynamicVars().Damage().Props(), Owner().Creature());
            return;
        }
        const RunData& run = Owner().RunState().st();
        if (run.rooms.empty()) return;
        if (run.rooms.back() == RoomType::Shop) {
            PlayerCmd::GainGold(DynamicVars().Gold().BaseValue(), Owner());
        } else if (run.rooms.back() == RoomType::Event && run.room_event == EventId::FAKE_MERCHANT) {
            FakeMerchant(sim_, 1).FoulPotionThrown();
        }
    }
};

}  // namespace sts2
