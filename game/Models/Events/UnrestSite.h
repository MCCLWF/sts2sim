// Models/Events/UnrestSite.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Factories/RelicFactory.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct UnrestSite : EventModel {
    SIM_MODEL(UnrestSite, EventModel)

    void CalculateVars() const {
        DynamicVars().Heal().SetBaseValue(Dec(Owner().Creature().MaxHp() - Owner().Creature().CurrentHp()));
    }

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(UnrestSite, Rest), InitialOptionKey("REST")));
        list.push_back(EventOption(SIM_OPTION(UnrestSite, Kill), InitialOptionKey("KILL"))
                           .ThatDecreasesMaxHp(DynamicVars()["MaxHpLoss"].BaseValue()));
        return list;
    }

    void Rest(const EventOptionState&) const {
        CreatureCmd::Heal(Owner().Creature(), DynamicVars().Heal().BaseValue());
        CardModel curse = Owner().RunState().CreateCard(CardId::POOR_SLEEP, Owner());
        CardPileCmd::Add(curse, PileType::Deck);
        SetEventFinished(L10NLookup("UNREST_SITE.pages.REST.description"));
    }

    void Kill(const EventOptionState&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        CreatureCmd::LoseMaxHp(ctx, Owner().Creature(), DynamicVars()["MaxHpLoss"].BaseValue(), false);
        RelicCmd::Obtain(RelicFactory::PullNextRelicFromFront(Owner()), Owner());
        SetEventFinished(L10NLookup("UNREST_SITE.pages.KILL.description"));
    }

    bool IsAllowed(RunState runState) const {
        for (Player p : runState.Players()) {
            if (Dec(p.Creature().CurrentHp()) > Dec(p.Creature().MaxHp()) * Dec::lit(70, 2)) return false;
        }
        return true;
    }
};

}  // namespace sts2
