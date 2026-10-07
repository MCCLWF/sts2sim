// Models/Events/SlipperyBridge.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/Events/AllowedHelpers.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct SlipperyBridge : EventModel {
    SIM_MODEL(SlipperyBridge, EventModel)
    int32_t CurrentHpLoss() const { return 3 + extra(0); }
    CardModel RandomCardToLose() const { return CardModel(sim_, static_cast<CardRef>(extra(1))); }

    void GetNewRandomCard() const {
        CardList candidates;
        CardModel previous = RandomCardToLose();
        if (previous) st().slippery_skipped_removals.push_back(previous.Index());
        for (CardModel card : Owner().Deck().Cards()) {
            if (!card.IsRemovable()) continue;
            if (!previous) {
                if (card.Rarity() != CardRarity::Basic) candidates.push_back(card);
            } else if (card.CardId_() != previous.CardId_() &&
                       !st().slippery_skipped_removals.contains(card.Index())) {
                candidates.push_back(card);
            }
        }
        if (candidates.empty()) {
            for (CardModel card : Owner().Deck().Cards()) if (card.IsRemovable()) candidates.push_back(card);
        }
        CardModel selected = Rng().NextItem(candidates);
        if (!selected) throw GameError("SlipperyBridge: no removable card");
        extra(1) = selected.Index();
    }

    EventOptionList GenerateInitialOptions() const {
        GetNewRandomCard();
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(SlipperyBridge, Overcome), InitialOptionKey("OVERCOME")));
        list.push_back(EventOption(SIM_OPTION(SlipperyBridge, HoldOn), InitialOptionKey("HOLD_ON_0"))
                           .ThatDoesDamage(Dec(CurrentHpLoss())));
        return list;
    }

    void OnRoomEnter() const {}  // RainVfx is presentation only.

    void Overcome(const EventOptionState&) const {
        CardPileCmd::RemoveFromDeck(RandomCardToLose());
        SetEventFinished(L10NLookup("SLIPPERY_BRIDGE.pages.OVERCOME.description"));
    }

    static std::string HoldOnSuffix(int32_t number) {
        return number >= 7 ? "LOOP" : std::to_string(number);
    }

    void HoldOn(const EventOptionState&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        CreatureCmd::Damage(ctx, Owner().Creature(), Dec(CurrentHpLoss()),
                            ValueProp::Unblockable | ValueProp::Unpowered, CardModel{}, nullptr);
        ++extra(0);
        DynamicVars()["HpLoss"].SetBaseValue(Dec(CurrentHpLoss()));
        GetNewRandomCard();
        std::string previous = HoldOnSuffix(extra(0) - 1);
        std::string current = HoldOnSuffix(extra(0));
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(SlipperyBridge, Overcome), InitialOptionKey("OVERCOME")));
        list.push_back(EventOption(SIM_OPTION(SlipperyBridge, HoldOn),
                                   "SLIPPERY_BRIDGE.pages.HOLD_ON_" + previous + ".options.HOLD_ON_" + current)
                           .ThatDoesDamage(Dec(CurrentHpLoss())));
        SetEventState(L10NLookup("SLIPPERY_BRIDGE.pages.HOLD_ON_" + previous + ".description"), list);
    }

    template <class Fn> static bool all_players(RunState runState, Fn&& fn) {
        for (Player p : runState.Players()) if (!fn(p)) return false;
        return true;
    }

    bool IsAllowed(RunState runState) const { if (runState.TotalFloor() <= 6) return false; return all_players(runState, [](Player p) { for (CardModel c : p.Deck().Cards()) if (c.IsRemovable()) return true; return false; }); }
};

}  // namespace sts2
