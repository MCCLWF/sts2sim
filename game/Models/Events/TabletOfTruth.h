// Models/Events/TabletOfTruth.cs
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/EventModel.h"

namespace sts2 {

struct TabletOfTruth : EventModel {
    SIM_MODEL(TabletOfTruth, EventModel)

    int32_t& DecipherCount() const { return extra(0); }

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(TabletOfTruth, Decipher),
                                   "TABLET_OF_TRUTH.pages.INITIAL.options.DECIPHER_1")
                           .ThatDecreasesMaxHp(DynamicVars()["DecipherMaxHpLoss"].BaseValue()));
        list.push_back(EventOption(SIM_OPTION(TabletOfTruth, Smash),
                                   "TABLET_OF_TRUTH.pages.INITIAL.options.SMASH"));
        return list;
    }

    void Smash(const EventOptionState&) const {
        CreatureCmd::Heal(Owner().Creature(), DynamicVars()["SmashHPGain"].BaseValue());
        SetEventFinished(L10NLookup("TABLET_OF_TRUTH.pages.SMASH.description"));
    }

    void Decipher(const EventOptionState&) const {
        LoseMaxHpAndUpgrade(DynamicVars()["DecipherMaxHpLoss"].BaseValue());
        ++DecipherCount();
        if (DecipherCount() == 5) {
            SetEventFinished(L10NLookup("TABLET_OF_TRUTH.pages.DECIPHER_5.description"));
            return;
        }
        int32_t cost = DecipherCount() == 1 ? 6 : DecipherCount() == 2 ? 12 : DecipherCount() == 3 ? 24
                                                                                                      : Owner().Creature().MaxHp() - 1;
        DynamicVars()["DecipherMaxHpLoss"].SetBaseValue(Dec(cost));
        EventOptionList list;
        std::string key = "TABLET_OF_TRUTH.pages.DECIPHER_" + std::to_string(DecipherCount()) + ".options.DECIPHER";
        list.push_back(EventOption(SIM_OPTION(TabletOfTruth, Decipher), key)
                           .ThatDecreasesMaxHp(DynamicVars()["DecipherMaxHpLoss"].BaseValue()));
        list.push_back(EventOption(SIM_OPTION(TabletOfTruth, GiveUp),
                                   "TABLET_OF_TRUTH.pages.DECIPHER.options.GIVE_UP"));
        std::string page = "TABLET_OF_TRUTH.pages.DECIPHER_" + std::to_string(DecipherCount()) + ".description";
        SetEventState(L10NLookup(page), list);
    }

    void GiveUp(const EventOptionState&) const {
        SetEventFinished(L10NLookup("TABLET_OF_TRUTH.pages.GIVE_UP.description"));
    }

    void LoseMaxHpAndUpgrade(Dec hp) const {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        if (!(hp < Dec(Owner().Creature().MaxHp()))) {
            CreatureCmd::LoseMaxHp(ctx, Owner().Creature(), Dec(Owner().Creature().MaxHp() - 1), false);
            CreatureCmd::Kill(Owner().Creature());
            return;
        }
        CreatureCmd::LoseMaxHp(ctx, Owner().Creature(), hp, false);
        CardList cards;
        for (CardModel c : Owner().Deck().Cards()) if (c.IsUpgradable()) cards.push_back(c);
        if (DecipherCount() == 4) {
            for (CardModel c : cards) CardCmd::Upgrade(c);
        } else if (!cards.empty()) {
            CardCmd::Upgrade(Rng().NextItem(cards));
        }
    }
};

}  // namespace sts2
