// Models/Events/WoodCarvings.cs (IsAllowed partial port)
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct WoodCarvings : EventModel {
    SIM_MODEL(WoodCarvings, EventModel)
    EventOptionList GenerateInitialOptions() const {
        bool canSnake = false;
        for (CardModel c : Owner().Deck().Cards()) canSnake |= slitherable(c);
        EventOptionList list;
        list.push_back(EventOption(SIM_OPTION(WoodCarvings, Bird), InitialOptionKey("BIRD")));
        list.push_back(canSnake ? EventOption(SIM_OPTION(WoodCarvings, Snake), InitialOptionKey("SNAKE")) : EventOption(0, InitialOptionKey("SNAKE_LOCKED")));
        list.push_back(EventOption(SIM_OPTION(WoodCarvings, Torus), InitialOptionKey("TORUS")));
        return list;
    }
    static bool basic_transformable(CardModel c) { return c.IsTransformable() && c.Rarity() == CardRarity::Basic; }
    static bool slitherable(CardModel c) { return c.Pile() && c.Type() != CardType::Status && c.Type() != CardType::Curse && c.Type() != CardType::Quest && !c.Enchantment() && !c.Keywords().Contains(CardKeyword::Unplayable) && !c.EnergyCost().CostsX(); }
    void Bird(const EventOptionState&) const { CardList c = CardSelectCmd::FromDeckGeneric(Owner(), CardSelectorPrefs(1), basic_transformable); if (!c.empty()) CardCmd::Transform(c[0], Owner().RunState().CreateCard(CardId::PECK, Owner())); SetEventFinished(L10NLookup("WOOD_CARVINGS.pages.BIRD.description")); }
    void Snake(const EventOptionState&) const { CardList c = CardSelectCmd::FromDeckForEnchantment(Owner(), CardSelectorPrefs(1), slitherable); if (!c.empty()) CardCmd::Enchant(EnchantmentId::SLITHER, c[0], Dec(1)); SetEventFinished(L10NLookup("WOOD_CARVINGS.pages.SNAKE.description")); }
    void Torus(const EventOptionState&) const { CardList c = CardSelectCmd::FromDeckGeneric(Owner(), CardSelectorPrefs(1), basic_transformable); if (!c.empty()) CardCmd::Transform(c[0], Owner().RunState().CreateCard(CardId::TORIC_TOUGHNESS, Owner())); SetEventFinished(L10NLookup("WOOD_CARVINGS.pages.TORUS.description")); }

    template <class Fn> static bool all_players(RunState runState, Fn&& fn) {
        for (Player p : runState.Players()) if (!fn(p)) return false;
        return true;
    }

    bool IsAllowed(RunState runState) const { return all_players(runState, [](Player p) { for (CardModel c : p.Deck().Cards()) if (c.Rarity() == CardRarity::Basic && c.IsRemovable()) return true; return false; }); }
};

}  // namespace sts2
