#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Entities/Players/PlayerCombatState.h"
#include "game/Models/PowerModel.h"
#include "engine/sim/errors.h"

namespace sts2 {

struct DampenPower : PowerModel {
    SIM_MODEL(DampenPower, PowerModel)

    void AfterApplied(Creature, CardModel) const {
        for (CardModel card : Owner().Player().PlayerCombatState().AllCards()) {
            if (card.CurrentUpgradeLevel() <= 0) continue;
            for (const auto& entry : st().dampen_downgraded_cards)
                if (entry.card == card.Index()) throw GameError("duplicate Dampen downgraded card");
            st().dampen_downgraded_cards.push_back({card.Index(), card.CurrentUpgradeLevel()});
            CardCmd::Downgrade(card);
        }
    }

    void AfterDeath(PlayerChoiceContext&, Creature creature, bool wasRemovalPrevented, float) const {
        if (wasRemovalPrevented) return;
        if (st().dampen_casters.remove(creature.Index()) && st().dampen_casters.empty()) PowerCmd::Remove(*this);
    }

    void AddCaster(Creature creature) const {
        if (!st().dampen_casters.contains(creature.Index())) st().dampen_casters.push_back(creature.Index());
    }

    void AfterRemoved(Creature) const {
        for (const auto& entry : st().dampen_downgraded_cards) {
            CardModel card(sim_, entry.card);
            for (int level = 0; level < entry.level; ++level) CardCmd::Upgrade(card);
        }
    }
};

}  // namespace sts2
