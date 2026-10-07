// Models/Events/TrashHeap.cs
#pragma once

#include <array>

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct TrashHeap : EventModel {
    SIM_MODEL(TrashHeap, EventModel)

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(TrashHeap, DiveIn), InitialOptionKey("DIVE_IN"))
                           .ThatDoesDamage(DynamicVars().HpLoss().BaseValue()));
        list.push_back(EventOption(SIM_OPTION(TrashHeap, Grab), InitialOptionKey("GRAB")));
        return list;
    }

    void DiveIn(const EventOptionState&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        CreatureCmd::Damage(ctx, Owner().Creature(), DynamicVars().HpLoss().BaseValue(),
                            ValueProp::Unblockable | ValueProp::Unpowered, Creature{});
        static constexpr std::array<RelicId, 5> kRelics = {
            RelicId::DARKSTONE_PERIAPT, RelicId::DREAM_CATCHER, RelicId::HAND_DRILL,
            RelicId::MAW_BANK, RelicId::THE_BOOT};
        RelicCmd::Obtain(Rng().NextItem(kRelics), Owner());
        SetEventFinished(L10NLookup("TRASH_HEAP.pages.DIVE_IN.description"));
    }

    void Grab(const EventOptionState&) const {
        PlayerCmd::GainGold(DynamicVars().Gold().BaseValue(), Owner());
        static constexpr std::array<CardId, 10> kCards = {
            CardId::CALTROPS, CardId::CLASH, CardId::DISTRACTION, CardId::DUAL_WIELD, CardId::ENTRENCH,
            CardId::HELLO_WORLD, CardId::OUTMANEUVER, CardId::REBOUND, CardId::RIP_AND_TEAR, CardId::STACK};
        CardModel card = Owner().RunState().CreateCard(Rng().NextItem(kCards), Owner());
        CardPileCmd::Add(card, PileType::Deck);
        SetEventFinished(L10NLookup("TRASH_HEAP.pages.GRAB.description"));
    }

    bool IsAllowed(RunState runState) const {
        for (Player p : runState.Players()) if (p.Creature().CurrentHp() <= 5) return false;
        return true;
    }
};

}  // namespace sts2
