// Models/Relics/TouchOfOrobas.cs.
#pragma once

#include "game/Commands/RelicCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

struct TouchOfOrobas : RelicModel {
    SIM_MODEL(TouchOfOrobas, RelicModel)

    static RelicId RefinedStarter(RelicId id) {
        switch (id) {
        case RelicId::BURNING_BLOOD: return RelicId::BLACK_BLOOD;
        case RelicId::RING_OF_THE_SNAKE: return RelicId::RING_OF_THE_DRAKE;
        case RelicId::DIVINE_RIGHT: return RelicId::DIVINE_DESTINY;
        case RelicId::BOUND_PHYLACTERY: return RelicId::PHYLACTERY_UNBOUND;
        case RelicId::CRACKED_CORE: return RelicId::INFUSED_CORE;
        default: return RelicId::CIRCLET;
        }
    }

    void AfterObtained() const {
        Player owner = Owner();
        RelicModel starter;
        RelicId upgraded = RelicId::NONE;
        // Orobas saves both ids on the offered mutable relic. Other legitimate
        // acquisition paths derive the same values from the first starter relic.
        if (extra(0) != 0) {
            starter = owner.GetRelic(static_cast<RelicId>(extra(0)));
            upgraded = static_cast<RelicId>(extra(1));
        } else {
            for (RelicModel relic : owner.Relics()) {
                if (relic.Rarity() == RelicRarity::Starter) {
                    starter = relic;
                    upgraded = RefinedStarter(relic.RelicId_());
                    break;
                }
            }
        }
        if (!starter || upgraded == RelicId::NONE)
            throw GameError("Touch of Orobas requires an owned starter relic");
        RelicCmd::Replace(starter, upgraded);
    }
};

}  // namespace sts2
