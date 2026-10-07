// Models/Events/HungryForMushrooms.cs
#pragma once

#include "game/Models/EventModel.h"

namespace sts2 {

struct HungryForMushrooms : EventModel {
    SIM_MODEL(HungryForMushrooms, EventModel)

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(RelicOption(RelicId::BIG_MUSHROOM, SIM_OPTION(HungryForMushrooms, BigMushroom),
                                   "INITIAL").ThatHasDynamicTitle());
        list.push_back(RelicOption(RelicId::FRAGRANT_MUSHROOM, SIM_OPTION(HungryForMushrooms, FragrantMushroom),
                                   "INITIAL")
                           .ThatDoesDamage(Dec(15)));
        return list;
    }

    void BigMushroom(const EventOptionState&) const {
        RelicCmd::Obtain(RelicId::BIG_MUSHROOM, Owner());
        SetEventFinished(L10NLookup("HUNGRY_FOR_MUSHROOMS.pages.BIG_MUSHROOM.description"));
    }

    void FragrantMushroom(const EventOptionState&) const {
        RelicCmd::Obtain(RelicId::FRAGRANT_MUSHROOM, Owner());
        SetEventFinished(L10NLookup("HUNGRY_FOR_MUSHROOMS.pages.FRAGRANT_MUSHROOM.description"));
    }
};

}  // namespace sts2
