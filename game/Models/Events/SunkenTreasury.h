// Models/Events/SunkenTreasury.cs
#pragma once

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct SunkenTreasury : EventModel {
    SIM_MODEL(SunkenTreasury, EventModel)

    void CalculateVars() const {
        DynamicVars()["SmallChestGold"].SetBaseValue(
            DynamicVars()["SmallChestGold"].BaseValue() + Dec(Rng().NextInt(16) - 8));
        DynamicVars()["LargeChestGold"].SetBaseValue(
            DynamicVars()["LargeChestGold"].BaseValue() + Dec(Rng().NextInt(61) - 30));
    }

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(SunkenTreasury, FirstChest),
                                   "SUNKEN_TREASURY.pages.INITIAL.options.FIRST_CHEST"));
        list.push_back(EventOption(SIM_OPTION(SunkenTreasury, SecondChest),
                                   "SUNKEN_TREASURY.pages.INITIAL.options.SECOND_CHEST"));
        return list;
    }

    void FirstChest(const EventOptionState&) const {
        PlayerCmd::GainGold(DynamicVars()["SmallChestGold"].BaseValue(), Owner());
        SetEventFinished(L10NLookup("SUNKEN_TREASURY.pages.FIRST_CHEST.description"));
    }

    void SecondChest(const EventOptionState&) const {
        PlayerCmd::GainGold(DynamicVars()["LargeChestGold"].BaseValue(), Owner());
        CardPileCmd::Add(Owner().RunState().CreateCard(CardId::GREED, Owner()), PileType::Deck);
        SetEventFinished(L10NLookup("SUNKEN_TREASURY.pages.SECOND_CHEST.description"));
    }
};

}  // namespace sts2
