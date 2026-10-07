// Models/Events/SpiritGrafter.cs (v0.111.0).
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct SpiritGrafter : EventModel {
    SIM_MODEL(SpiritGrafter, EventModel)

    EventOptionList GenerateInitialOptions() const {
        EventOptionList result;
        result.push_back(EventOption(SIM_OPTION(SpiritGrafter, LetItIn), InitialOptionKey("LET_IT_IN")));
        result.push_back(EventOption(SIM_OPTION(SpiritGrafter, Rejection), InitialOptionKey("REJECTION"))
                             .ThatDoesDamage(DynamicVars()["RejectionHpLoss"].BaseValue()));
        return result;
    }

    void LetItIn(const EventOptionState&) const {
        CreatureCmd::Heal(Owner().Creature(), DynamicVars()["LetItInHealAmount"].BaseValue());
        CardPileCmd::Add(Owner().RunState().CreateCard(CardId::METAMORPHOSIS, Owner()), PileType::Deck);
        SetEventFinished(L10NLookup("SPIRIT_GRAFTER.pages.LET_IT_IN.description"));
    }

    void Rejection(const EventOptionState&) const {
        CardList selected = CardSelectCmd::FromDeckForUpgrade(Owner(), CardSelectorPrefs(1));
        if (!selected.empty()) CardCmd::Upgrade(selected[0]);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        CreatureCmd::Damage(ctx, Owner().Creature(), DynamicVars()["RejectionHpLoss"].BaseValue(),
                            ValueProp::Unblockable | ValueProp::Unpowered, Creature{});
        SetEventFinished(L10NLookup("SPIRIT_GRAFTER.pages.REJECTION.description"));
    }
};

}  // namespace sts2
