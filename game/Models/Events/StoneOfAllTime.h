// Models/Events/StoneOfAllTime.cs (v0.111.0).
#pragma once

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PotionCmd.h"
#include "game/Models/Events/AllowedHelpers.h"
#include "game/Models/EventModel.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct StoneOfAllTime : EventModel {
    SIM_MODEL(StoneOfAllTime, EventModel)

    void BeforeEventStarted(bool) const { Owner().SetCanUseOrRemovePotions(false); }
    void OnEventFinished() const { Owner().SetCanUseOrRemovePotions(true); }

    EventOptionList GenerateInitialOptions() const {
        PotionModel potion = Rng().NextItem(Owner().Potions());
        extra(0) = potion ? potion.Index() : 0;
        EventOptionList options;
        options.push_back(potion
                              ? EventOption(SIM_OPTION(StoneOfAllTime, Lift),
                                            "STONE_OF_ALL_TIME.pages.INITIAL.options.LIFT")
                              : EventOption(0, "STONE_OF_ALL_TIME.pages.INITIAL.options.LIFT_LOCKED"));
        bool canPush = false;
        for (CardModel card : Owner().Deck().Cards()) {
            if (CanEnchantVigorous(card)) {
                canPush = true;
                break;
            }
        }
        options.push_back(canPush
                              ? EventOption(SIM_OPTION(StoneOfAllTime, Push),
                                            "STONE_OF_ALL_TIME.pages.INITIAL.options.PUSH")
                                    .ThatDoesDamage(DynamicVars()["PushHpLoss"].BaseValue())
                              : EventOption(0, "STONE_OF_ALL_TIME.pages.INITIAL.options.PUSH_LOCKED"));
        return options;
    }

    void Lift(const EventOptionState&) const {
        if (extra(0) == 0) throw GameError("StoneOfAllTime: no selected potion");
        PotionCmd::Discard(PotionModel(&sim(), static_cast<uint32_t>(extra(0))));
        CreatureCmd::GainMaxHp(Owner().Creature(), DynamicVars()["DrinkMaxHpGain"].BaseValue());
        (void)Rng().NextInt(100);
        SetEventFinished(L10NLookup("STONE_OF_ALL_TIME.pages.LIFT.description"));
    }

    void Push(const EventOptionState&) const {
        PlayerChoiceContext choiceContext = PlayerChoiceContext::Throwing();
        CreatureCmd::Damage(choiceContext, Owner().Creature(), DynamicVars()["PushHpLoss"].BaseValue(),
                            ValueProp::Unblockable | ValueProp::Unpowered, Creature{});
        CardList cards = CardSelectCmd::FromDeckForEnchantment(Owner(), CardSelectorPrefs(1), CanEnchantVigorous);
        for (CardModel card : cards)
            CardCmd::Enchant(EnchantmentId::VIGOROUS, card, DynamicVars()["PushVigorousAmount"].BaseValue());
        (void)Rng().NextInt(100);
        SetEventFinished(L10NLookup("STONE_OF_ALL_TIME.pages.PUSH.description"));
    }

    static bool CanEnchantVigorous(CardModel card) {
        return event_allowed::BaseCanEnchant(card) && card.Type() == CardType::Attack;
    }

    template <class Fn> static bool all_players(RunState runState, Fn&& fn) {
        for (Player p : runState.Players()) if (!fn(p)) return false;
        return true;
    }

    bool IsAllowed(RunState runState) const { if (runState.CurrentActIndex() != 1) return false; return all_players(runState, [](Player p) { return !p.Potions().empty(); }); }
};

}  // namespace sts2
