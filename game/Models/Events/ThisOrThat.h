// Models/Events/ThisOrThat.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Factories/RelicFactory.h"
#include "game/Models/EventModel.h"

namespace sts2 {

struct ThisOrThat : EventModel {
    SIM_MODEL(ThisOrThat, EventModel)

    int32_t& Gold() const { return extra(0); }

    void CalculateVars() const { Gold() = Rng().NextInt(41, 69); }

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(ThisOrThat, Plain),
                                   "THIS_OR_THAT.pages.INITIAL.options.PLAIN")
                           .ThatDoesDamage(Dec(6)));
        list.push_back(EventOption(SIM_OPTION(ThisOrThat, Ornate),
                                   "THIS_OR_THAT.pages.INITIAL.options.ORNATE"));
        return list;
    }

    void Plain(const EventOptionState&) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        CreatureCmd::Damage(ctx, Owner().Creature(), Dec(6), ValueProp::Unblockable | ValueProp::Unpowered,
                             Creature{});
        PlayerCmd::GainGold(Dec(Gold()), Owner());
        SetEventFinished(L10NLookup("THIS_OR_THAT.pages.PLAIN.description"));
    }

    void Ornate(const EventOptionState&) const {
        RelicCmd::Obtain(RelicFactory::PullNextRelicFromFront(Owner()), Owner());
        CardPileCmd::Add(Owner().RunState().CreateCard(CardId::CLUMSY, Owner()), PileType::Deck);
        SetEventFinished(L10NLookup("THIS_OR_THAT.pages.ORNATE.description"));
    }
};

}  // namespace sts2
