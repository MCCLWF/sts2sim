#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Models/EventModel.h"

namespace sts2 {

struct LostWisp : EventModel {
    SIM_MODEL(LostWisp, EventModel)

    // LostWisp.CanonicalVars begins with GoldVar(60). Its event table currently
    // omits CanonicalVars, so keep that numeric value in the matching state slot.
    void CalculateVars() const {
        st().vars[0] = Dec(60) + Dec(Rng().NextInt(-15, 16));
    }

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(LostWisp, Claim), InitialOptionKey("CLAIM")));
        list.push_back(EventOption(SIM_OPTION(LostWisp, Search), InitialOptionKey("SEARCH")));
        return list;
    }

    void Claim(const EventOptionState&) const {
        StaticVec<CardId, 8> curses;
        curses.push_back(CardId::DECAY);
        CardPileCmd::AddCursesToDeck(curses, Owner());
        RelicCmd::Obtain(RelicId::LOST_WISP, Owner());
        SetEventFinished(L10NLookup("LOST_WISP.pages.CLAIM.description"));
    }

    void Search(const EventOptionState&) const {
        PlayerCmd::GainGold(Dec(st().vars[0].to_int()), Owner());
        SetEventFinished(L10NLookup("LOST_WISP.pages.SEARCH.description"));
    }
};

}  // namespace sts2
