// Models/Events/ColossalFlower.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct ColossalFlower : EventModel {
    SIM_MODEL(ColossalFlower, EventModel)

    int32_t& NumberOfDigs() const { return extra(0); }

    static constexpr int kPrizeCosts[3] = {35, 75, 135};
    static constexpr int kPrizeDamage[3] = {5, 6, 7};

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(ColossalFlower, ExtractCurrentPrize),
                                   NumberOfDigs() == 0 ? "COLOSSAL_FLOWER.pages.INITIAL.options.EXTRACT_CURRENT_PRIZE_1" : "COLOSSAL_FLOWER.pages.INITIAL.options.EXTRACT_CURRENT_PRIZE_2"));
        list.push_back(EventOption(SIM_OPTION(ColossalFlower, ReachDeeper),
                                   NumberOfDigs() == 0 ? "COLOSSAL_FLOWER.pages.INITIAL.options.REACH_DEEPER_1" : "COLOSSAL_FLOWER.pages.INITIAL.options.REACH_DEEPER_2")
                           .ThatDoesDamage(Dec(kPrizeDamage[NumberOfDigs()])));
        return list;
    }

    void ExtractCurrentPrize(const EventOptionState&) const {
        PlayerCmd::GainGold(Dec(kPrizeCosts[NumberOfDigs()]), Owner());
        SetEventFinished(L10NLookup("COLOSSAL_FLOWER.pages.EXTRACT_CURRENT_PRIZE.description"));
    }

    void ReachDeeper(const EventOptionState&) const {
        DealReachDeeperDamage();
        ++NumberOfDigs();
        if (NumberOfDigs() < 2) {
            EventOptionList list;
            list.push_back(EventOption(SIM_OPTION(ColossalFlower, ExtractCurrentPrize),
                                       "COLOSSAL_FLOWER.pages.REACH_DEEPER_1.options.EXTRACT_CURRENT_PRIZE_2"));
            list.push_back(EventOption(SIM_OPTION(ColossalFlower, ReachDeeper),
                                       "COLOSSAL_FLOWER.pages.REACH_DEEPER_1.options.REACH_DEEPER_2")
                               .ThatDoesDamage(Dec(kPrizeDamage[NumberOfDigs()])));
            SetEventState(L10NLookup("COLOSSAL_FLOWER.pages.REACH_DEEPER_1.description"), list);
        } else {
            EventOptionList list;
            list.push_back(EventOption(SIM_OPTION(ColossalFlower, ExtractInstead),
                                       "COLOSSAL_FLOWER.pages.REACH_DEEPER_2.options.EXTRACT_INSTEAD"));
            list.push_back(EventOption(SIM_OPTION(ColossalFlower, ObtainPollinousCore),
                                       "COLOSSAL_FLOWER.pages.REACH_DEEPER_2.options.POLLINOUS_CORE")
                               .ThatDoesDamage(Dec(kPrizeDamage[NumberOfDigs()])));
            SetEventState(L10NLookup("COLOSSAL_FLOWER.pages.REACH_DEEPER_2.description"), list);
        }
    }

    void ExtractInstead(const EventOptionState&) const {
        PlayerCmd::GainGold(Dec(kPrizeCosts[NumberOfDigs()]), Owner());
        SetEventFinished(L10NLookup("COLOSSAL_FLOWER.pages.EXTRACT_INSTEAD.description"));
    }

    void ObtainPollinousCore(const EventOptionState&) const {
        DealReachDeeperDamage();
        RelicCmd::Obtain(RelicId::POLLINOUS_CORE, Owner());
        SetEventFinished(L10NLookup("COLOSSAL_FLOWER.pages.POLLINOUS_CORE.description"));
    }

    void DealReachDeeperDamage() const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        CreatureCmd::Damage(ctx, Owner().Creature(), Dec(kPrizeDamage[NumberOfDigs()]),
                            ValueProp::Unblockable | ValueProp::Unpowered, Creature{});
    }

    bool IsAllowed(RunState runState) const {
        for (Player p : runState.Players()) if (p.Creature().CurrentHp() < 19) return false;
        return true;
    }
};

}  // namespace sts2
