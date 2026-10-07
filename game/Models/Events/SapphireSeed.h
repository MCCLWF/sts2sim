#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Models/EventModel.h"

namespace sts2 {

struct SapphireSeed : EventModel {
    SIM_MODEL(SapphireSeed, EventModel)
    static bool Sownable(CardModel c) {
        return c.Type() != CardType::Status && c.Type() != CardType::Curse && c.Type() != CardType::Quest &&
               !c.Enchantment() && !(c.Pile() && c.Pile().Type() == PileType::Deck && c.Keywords().Contains(CardKeyword::Unplayable));
    }
    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(SapphireSeed, Eat), InitialOptionKey("EAT")));
        list.push_back(EventOption(SIM_OPTION(SapphireSeed, Plant), InitialOptionKey("PLANT")));
        return list;
    }
    void Eat(const EventOptionState&) const {
        CreatureCmd::Heal(Owner().Creature(), Dec(9));
        CardList cards = CardSelectCmd::FromDeckForUpgrade(Owner(), CardSelectorPrefs(1));
        if (!cards.empty()) CardCmd::Upgrade(cards[0]);
        SetEventFinished(L10NLookup("SAPPHIRE_SEED.pages.EAT.description"));
    }
    void Plant(const EventOptionState&) const {
        CardList cards = CardSelectCmd::FromDeckForEnchantment(Owner(), CardSelectorPrefs(1), Sownable);
        if (!cards.empty()) CardCmd::Enchant(EnchantmentId::SOWN, cards[0], Dec(1));
        SetEventFinished(L10NLookup("SAPPHIRE_SEED.pages.PLANT.description"));
    }
};

}  // namespace sts2
