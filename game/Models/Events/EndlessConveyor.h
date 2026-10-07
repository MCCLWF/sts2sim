// Models/Events/EndlessConveyor.cs
#pragma once

#include "engine/sim/alloc.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Commands/RewardsCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Factories/PotionFactory.h"
#include "game/Models/Events/AllowedHelpers.h"
#include "game/Models/EventModel.h"
#include "game/Runs/CardCreationOptions.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct EndlessConveyor : EventModel {
    SIM_MODEL(EndlessConveyor, EventModel)

    enum Dish : int32_t { None, Caviar, SpicySnappy, JellyLiver, FriedEel, SuspiciousCondiment, ClamRoll,
                          GoldenFysh, SeapunkSalad };
    struct WeightedDish { Dish id; float weight; };

    static const char* DishName(Dish dish) {
        switch (dish) {
        case Caviar: return "CAVIAR";
        case SpicySnappy: return "SPICY_SNAPPY";
        case JellyLiver: return "JELLY_LIVER";
        case FriedEel: return "FRIED_EEL";
        case SuspiciousCondiment: return "SUSPICIOUS_CONDIMENT";
        case ClamRoll: return "CLAM_ROLL";
        case GoldenFysh: return "GOLDEN_FYSH";
        case SeapunkSalad: return "SEAPUNK_SALAD";
        default: return "";
        }
    }

    Dish CurrentDish() const { return static_cast<Dish>(extra(2)); }

    void RollDish() const {
        ++extra(0);
        if (extra(0) % 5 == 0) {
            extra(1) = extra(2) = SeapunkSalad;
            return;
        }
        StaticVec<WeightedDish, 8> candidates;
        candidates.push_back({Caviar, 6.f});
        candidates.push_back({SpicySnappy, 3.f});
        candidates.push_back({JellyLiver, 3.f});
        candidates.push_back({FriedEel, 3.f});
        if (Owner().HasOpenPotionSlots()) candidates.push_back({SuspiciousCondiment, 3.f});
        if (Owner().Creature().CurrentHp() != Owner().Creature().MaxHp()) candidates.push_back({ClamRoll, 6.f});
        if (extra(0) > 1) candidates.push_back({GoldenFysh, 1.f});
        float total = 0.f;
        for (auto dish : candidates) if (dish.id != extra(1)) total += dish.weight;
        float roll = Rng().NextFloat() * total;
        float running = 0.f;
        for (auto dish : candidates) {
            if (dish.id == extra(1)) continue;
            running += dish.weight;
            if (roll < running) {
                extra(1) = extra(2) = dish.id;
                return;
            }
        }
        throw GameError("EndlessConveyor: dish roll out of range");
    }

    void CalculateVars() const { RollDish(); }

    EventOption GrabOption() const {
        if (Owner().Gold() < DynamicVars().Gold().IntValue())
            return EventOption(0, OptionKey("ALL", "LOCKED"));
        return EventOption(SIM_OPTION(EndlessConveyor, GrabSomethingOffTheBelt),
                           OptionKey("ALL", DishName(CurrentDish())));
    }

    EventOptionList GenerateInitialOptions() const {
        EventOptionList list;
        list.push_back(GrabOption());
        list.push_back(EventOption(SIM_OPTION(EndlessConveyor, ObserveChef), InitialOptionKey("OBSERVE_CHEF")));
        return list;
    }

    void GrabSomethingOffTheBelt(const EventOptionState&) const {
        if (CurrentDish() != GoldenFysh)
            PlayerCmd::LoseGold(Dec(DynamicVars().Gold().IntValue()), Owner(), GoldLossType::Spent);
        switch (CurrentDish()) {
        case ClamRoll: CreatureCmd::Heal(Owner().Creature(), Dec(10)); break;
        case Caviar: CreatureCmd::GainMaxHp(Owner().Creature(), Dec(4)); break;
        case SuspiciousCondiment: {
            PotionId id = Owner().PlayerRng().Rewards().NextItem(PotionFactory::GetPotionOptions(Owner()));
            if (id != PotionId::NONE) {
                StaticVec<Reward, 8> rewards;
                rewards.push_back(Reward::NewPotionReward(Owner(), PotionModel(&sim(), alloc_potion(*sim_->g, id, 0))));
                RewardsCmd::OfferCustom(Owner(), rewards);
            }
            break;
        }
        case JellyLiver: {
            CardList selected = CardSelectCmd::FromDeckForTransformation(Owner(), CardSelectorPrefs(1));
            if (!selected.empty()) CardCmd::TransformToRandom(selected[0], Rng());
            break;
        }
        case SeapunkSalad: {
            CardPileCmd::Add(Owner().RunState().CreateCard(CardId::FEEDING_FRENZY, Owner()), PileType::Deck);
            break;
        }
        case FriedEel: {
            CardCreationOptionsState options = CardCreationOptions::ForNonCombatWithDefaultOdds({CardPoolId::COLORLESS_CARD_POOL});
            CardList cards = CardFactory::CreateForReward(Owner(), 1, options);
            if (!cards.empty()) CardPileCmd::Add(cards[0], PileType::Deck);
            break;
        }
        case GoldenFysh: PlayerCmd::GainGold(Dec(75), Owner()); break;
        case SpicySnappy: {
            CardList upgradable;
            for (CardModel card : Owner().Deck().Cards()) if (card.IsUpgradable()) upgradable.push_back(card);
            if (!upgradable.empty()) CardCmd::Upgrade(Rng().NextItem(upgradable));
            break;
        }
        default: throw GameError("EndlessConveyor: invalid dish");
        }
        RollDish();
        EventOptionList list;
        list.push_back(GrabOption());
        list.push_back(EventOption(SIM_OPTION(EndlessConveyor, Leave), OptionKey("GRAB_SOMETHING_OFF_THE_BELT", "LEAVE")));
        SetEventState(L10NLookup("ENDLESS_CONVEYOR.pages.GRAB_SOMETHING_OFF_THE_BELT.description"), list);
    }

    void ObserveChef(const EventOptionState&) const {
        CardList upgradable;
        for (CardModel card : Owner().Deck().Cards()) if (card.IsUpgradable()) upgradable.push_back(card);
        if (!upgradable.empty()) CardCmd::Upgrade(Rng().NextItem(upgradable));
        SetEventFinished(L10NLookup("ENDLESS_CONVEYOR.pages.OBSERVE_CHEF.description"));
    }

    void Leave(const EventOptionState&) const {
        SetEventFinished(L10NLookup("ENDLESS_CONVEYOR.pages.LEAVE.description"));
    }

    template <class Fn> static bool all_players(RunState runState, Fn&& fn) {
        for (Player p : runState.Players()) if (!fn(p)) return false;
        return true;
    }

    bool IsAllowed(RunState runState) const { return all_players(runState, [](Player p) { return p.Gold() >= 120; }); }
};

}  // namespace sts2
