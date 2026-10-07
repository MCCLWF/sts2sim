// Models/Events/SunkenStatue.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Models/EventModel.h"

namespace sts2 {

struct SunkenStatue : EventModel {
    SIM_MODEL(SunkenStatue, EventModel)

    int32_t& Gold() const { return extra(0); }

    void CalculateVars() const {
        Gold() = 111 + Rng().NextInt(-10, 11);
    }

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(SunkenStatue, GrabSword),
                                   "SUNKEN_STATUE.pages.INITIAL.options.GRAB_SWORD"));
        list.push_back(EventOption(SIM_OPTION(SunkenStatue, DiveIntoWater),
                                   "SUNKEN_STATUE.pages.INITIAL.options.DIVE_INTO_WATER")
                           .ThatDoesDamage(Dec(7)));
        return list;
    }

    void GrabSword(const EventOptionState&) const {
        RelicCmd::Obtain(RelicId::SWORD_OF_STONE, Owner());
        SetEventFinished(L10NLookup("SUNKEN_STATUE.pages.GRAB_SWORD.description"));
    }

    void DiveIntoWater(const EventOptionState&) const {
        PlayerCmd::GainGold(Dec(Gold()), Owner());
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        CreatureCmd::Damage(ctx, Owner().Creature(), Dec(7), ValueProp::Unblockable | ValueProp::Unpowered,
                             Creature{});
        SetEventFinished(L10NLookup("SUNKEN_STATUE.pages.DIVE_INTO_WATER.description"));
    }
};

}  // namespace sts2
