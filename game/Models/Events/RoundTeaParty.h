// Models/Events/RoundTeaParty.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Factories/RelicFactory.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct RoundTeaParty : EventModel {
    SIM_MODEL(RoundTeaParty, EventModel)

    static constexpr Dec kDamage = Dec(11);

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(RoundTeaParty, EnjoyTea), InitialOptionKey("ENJOY_TEA")));
        list.push_back(EventOption(SIM_OPTION(RoundTeaParty, PickFight), InitialOptionKey("PICK_FIGHT"))
                           .ThatDoesDamage(kDamage));
        return list;
    }

    void EnjoyTea(const EventOptionState&) const {
        Creature creature = Owner().Creature();
        RelicCmd::Obtain(RelicId::ROYAL_POISON, Owner());
        CreatureCmd::Heal(creature, Dec(creature.MaxHp() - creature.CurrentHp()));
        SetEventFinished(L10NLookup("ROUND_TEA_PARTY.pages.ENJOY_TEA.description"));
    }

    void PickFight(const EventOptionState&) const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(RoundTeaParty, ContinueFight),
                                   OptionKey("PICK_FIGHT", "CONTINUE_FIGHT")));
        SetEventState(L10NLookup("ROUND_TEA_PARTY.pages.PICK_FIGHT.description"), list);
    }

    void ContinueFight(const EventOptionState&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        CreatureCmd::Damage(ctx, Owner().Creature(), kDamage,
                            ValueProp::Unblockable | ValueProp::Unpowered, Creature{});
        RelicCmd::Obtain(RelicFactory::PullNextRelicFromFront(Owner()), Owner());
        SetEventFinished(L10NLookup("ROUND_TEA_PARTY.pages.CONTINUE_FIGHT.description"));
    }

    bool IsAllowed(RunState runState) const {
        for (Player p : runState.Players()) if (p.Creature().CurrentHp() < 12) return false;
        return true;
    }
};

}  // namespace sts2
