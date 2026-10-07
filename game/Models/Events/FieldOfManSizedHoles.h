// Models/Events/FieldOfManSizedHoles.cs (v0.111.0).
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/Events/AllowedHelpers.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct FieldOfManSizedHoles : EventModel {
    SIM_MODEL(FieldOfManSizedHoles, EventModel)

    // The exporter omits this event's CanonicalVars; CardsVar(2) is the
    // source's selection count. GoldVar(75) and both StringVars are text only.
    static constexpr int kCardsToRemove = 2;

    EventOptionList GenerateInitialOptions() const {
        EventOptionList options;
        options.push_back(EventOption(SIM_OPTION(FieldOfManSizedHoles, Resist),
                                      "FIELD_OF_MAN_SIZED_HOLES.pages.INITIAL.options.RESIST"));
        options.push_back(EventOption(SIM_OPTION(FieldOfManSizedHoles, EnterYourHole),
                                      "FIELD_OF_MAN_SIZED_HOLES.pages.INITIAL.options.ENTER_YOUR_HOLE"));
        return options;
    }

    void Resist(const EventOptionState&) const {
        CardList cards = CardSelectCmd::FromDeckForRemoval(Owner(), CardSelectorPrefs(kCardsToRemove));
        CardPileCmd::RemoveFromDeck(cards);
        StaticVec<CardId, 8> curses;
        curses.push_back(CardId::NORMALITY);
        CardPileCmd::AddCursesToDeck(curses, Owner());
        SetEventFinished(L10NLookup("FIELD_OF_MAN_SIZED_HOLES.pages.RESIST.description"));
    }

    void EnterYourHole(const EventOptionState&) const {
        CardList cards = CardSelectCmd::FromDeckForEnchantment(Owner(), CardSelectorPrefs(1), CanEnchant);
        if (!cards.empty()) CardCmd::Enchant(EnchantmentId::PERFECT_FIT, cards[0], Dec(1));
        SetEventFinished(L10NLookup("FIELD_OF_MAN_SIZED_HOLES.pages.ENTER_YOUR_HOLE.description"));
    }

    static bool CanEnchant(CardModel card) { return event_allowed::CanEnchant(card, EnchantmentId::PERFECT_FIT); }

    template <class Fn> static bool all_players(RunState runState, Fn&& fn) {
        for (Player p : runState.Players()) if (!fn(p)) return false;
        return true;
    }

    bool IsAllowed(RunState runState) const {
        return all_players(runState, [](Player p) {
            for (CardModel c : p.Deck().Cards()) if (CanEnchant(c)) return true;
            return false;
        });
    }
};

}  // namespace sts2
