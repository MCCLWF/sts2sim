// Models/Events/Darv.cs (v0.111.0).
#pragma once

#include "engine/sim/alloc.h"
#include "game/Models/EventModel.h"
#include "game/Models/Relics/DustyTome.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct Darv : EventModel {
    SIM_MODEL(Darv, EventModel)

    // Darv.ValidRelicSet contains one relic in every set in v0.111.0.  Keep
    // the source order: the event materializes all valid options, shuffles
    // them with the event RNG, then takes two or three.
    EventOptionList GenerateInitialOptions() const {
        EventOptionList source;
        // Every ValidRelicSet currently has one relic, but the source still
        // calls NextItem on each set; preserve those one-element RNG draws.
        auto add_set = [this, &source](RelicId id) {
            StaticVec<RelicId, 1> relics;
            relics.push_back(id);
            source.push_back(AncientRelicOption(Rng().NextItem(relics)));
        };
        add_set(RelicId::ASTROLABE);
        add_set(RelicId::BLACK_STAR);
        add_set(RelicId::CALLING_BELL);
        add_set(RelicId::EMPTY_CAGE);
        // The simulator's standard profile has no run modifiers.  A future
        // modifier state must suppress this candidate when it clears the deck.
        add_set(RelicId::PANDORAS_BOX);
        add_set(RelicId::RUNIC_PYRAMID);
        add_set(RelicId::SNECKO_EYE);
        if (Owner().RunState().CurrentActIndex() == 1) {
            add_set(RelicId::ECTOPLASM);
            add_set(RelicId::SOZU);
        }
        if (Owner().RunState().CurrentActIndex() >= 1) {
            add_set(RelicId::PHILOSOPHERS_STONE);
            add_set(RelicId::VELVET_CHOKER);
        }

        Rng().Shuffle(source);
        const int32_t count = Rng().NextBool() ? 2 : 3;
        EventOptionList result;
        for (int32_t i = 0; i < count && i < static_cast<int32_t>(source.size()); ++i)
            result.push_back(source[static_cast<size_t>(i)]);

        if (count == 2) {
            RelicModel relic(&sim(), alloc_relic(*sim_->g, RelicId::DUSTY_TOME, 0));
            DustyTome dusty(sim_, relic.Index());
            (void)dusty.SetupForPlayer(Owner());
            result.push_back(AncientRelicOption(relic));
        }
        return result;
    }
};

}  // namespace sts2
