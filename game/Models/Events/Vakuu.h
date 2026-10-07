// Models/Events/Vakuu.cs (v0.111.0).
#pragma once

#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct Vakuu : EventModel {
    SIM_MODEL(Vakuu, EventModel)

    void CalculateVars() const {
        const Player owner = Owner();
        if (!owner) return;  // LocalContext.IsMe(owner); single-player Env owner.
        const int32_t visits = owner.RunState().st().progress_ancient_stats.GetVisitsAs(EventId_(), owner.Character());
        DynamicVars()["Visits"].SetBaseValue(Dec(visits + 1));
    }

    EventOptionList GenerateInitialOptions() const {
        // Pool properties are materialized in sequence before any shuffles.
        // RelicOption<T> constructs a distinct mutable relic for every entry.
        EventOptionList pool1;
        pool1.push_back(AncientRelicOption(RelicId::BLOOD_SOAKED_ROSE));
        pool1.push_back(AncientRelicOption(RelicId::WHISPERING_EARRING));
        pool1.push_back(AncientRelicOption(RelicId::FIDDLE));

        EventOptionList pool2;
        pool2.push_back(AncientRelicOption(RelicId::PRESERVED_FOG));
        pool2.push_back(AncientRelicOption(RelicId::SERE_TALON).ThatDecreasesMaxHp(Dec(9)));
        pool2.push_back(AncientRelicOption(RelicId::DISTINGUISHED_CAPE));

        EventOptionList pool3;
        pool3.push_back(AncientRelicOption(RelicId::CHOICES_PARADOX));
        pool3.push_back(AncientRelicOption(RelicId::MUSIC_BOX));
        pool3.push_back(AncientRelicOption(RelicId::LORDS_PARASOL));
        pool3.push_back(AncientRelicOption(RelicId::JEWELED_MASK));

        Rng().Shuffle(pool1);
        Rng().Shuffle(pool2);
        Rng().Shuffle(pool3);

        EventOptionList result;
        result.push_back(pool1[0]);
        result.push_back(pool2[0]);
        result.push_back(pool3[0]);
        return result;
    }
};

}  // namespace sts2
