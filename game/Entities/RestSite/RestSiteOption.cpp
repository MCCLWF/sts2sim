// Entities/RestSite/{RestSiteOption,HealRestSiteOption,SmithRestSiteOption}.cs,
// Multiplayer/Game/RestSiteSynchronizer.cs (single player).
#include "game/Entities/RestSite/RestSiteOption.h"

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Commands/RewardsCmd.h"
#include "game/Factories/RelicFactory.h"
#include "game/Hooks/Hook.h"
#include "game/Hooks/HookGuard.h"
#include "game/Models/Cards/ByrdonisEgg.h"
#include "game/Models/Relics/Girya.h"
#include "game/Models/Relics/PumpkinCandle.h"
#include "game/Models/Relics/Shovel.h"

namespace sts2::RestSite {

namespace {

// HealRestSiteOption.GetBaseHealAmount: (decimal)creature.MaxHp * 0.3m
Dec GetBaseHealAmount(Creature creature) { return Dec(creature.MaxHp()) * Dec::lit(3, 1); }

// SmithRestSiteOption.OnSelect (SmithCount 1: only rest site option hooks change it).
bool SmithOnSelect(Player owner) {
    CardSelectorPrefs prefs(1);  // (UpgradeSelectionPrompt, SmithCount)
    prefs.Cancelable = true;
    prefs.RequireManualConfirmation = true;
    CardList selection = CardSelectCmd::FromDeckForUpgrade(owner, prefs);
    if (selection.empty()) return false;
    for (CardModel item : selection) CardCmd::Upgrade(item);  // CardPreviewStyle.None
    Hook::AfterRestSiteSmith(owner.RunState(), owner);
    return true;
}

bool OnSelect(Player owner, RestSiteOptionId option) {
    switch (option) {
    case RestSiteOptionId::Heal:
        ExecuteRestSiteHeal(owner, /*isMimicked=*/false);
        return true;
    case RestSiteOptionId::Smith: return SmithOnSelect(owner);
    case RestSiteOptionId::Cook: {
        CardSelectorPrefs prefs(2);
        prefs.Cancelable = true;
        prefs.RequireManualConfirmation = true;
        CardList selection = CardSelectCmd::FromDeckForRemoval(owner, prefs);
        if (selection.empty()) return false;
        CardPileCmd::RemoveFromDeck(selection);
        CreatureCmd::GainMaxHp(owner.Creature(), Dec(5));
        return true;
    }
    case RestSiteOptionId::Hatch:
        RelicCmd::Obtain(RelicId::BYRDPIP, owner);
        return true;
    case RestSiteOptionId::Lift: {
        RelicModel relic = owner.GetRelic(RelicId::GIRYA);
        if (!relic) throw GameError("Lift rest site option requires Girya");
        Girya(&owner.sim(), relic.Index()).IncrementTimesLifted();
        return true;
    }
    case RestSiteOptionId::Dig:
        RelicCmd::Obtain(RelicFactory::PullNextRelicFromFront(owner), owner);
        return true;
    case RestSiteOptionId::Kindle: {
        RelicModel relic = owner.GetRelic(RelicId::PUMPKIN_CANDLE);
        if (relic) PumpkinCandle(&owner.sim(), relic.Index()).Rekindle();
        return true;
    }
    case RestSiteOptionId::Clone: {
        CardList cards = owner.Deck().Cards();
        for (CardModel card : cards) {
            EnchantmentModel enchantment = card.Enchantment();
            if (!enchantment || enchantment.EnchantmentId_() != EnchantmentId::CLONE) continue;
            CardPileCmd::Add(owner.RunState().CloneCard(card), PileType::Deck);
        }
        return true;
    }
    default: throw Unported("rest site option " + std::string(OptionId(option)));
    }
}

}  // namespace

std::string_view OptionId(RestSiteOptionId option) {
    switch (option) {
    case RestSiteOptionId::Heal: return "HEAL";
    case RestSiteOptionId::Smith: return "SMITH";
    case RestSiteOptionId::Mend: return "MEND";
    case RestSiteOptionId::Lift: return "LIFT";
    case RestSiteOptionId::Dig: return "DIG";
    case RestSiteOptionId::Cook: return "COOK";
    case RestSiteOptionId::Clone: return "CLONE";
    case RestSiteOptionId::Hatch: return "HATCH";
    case RestSiteOptionId::Kindle: return "KINDLE";
    }
    return "?";
}

bool IsEnabled(Player player, RestSiteOptionId option) {
    switch (option) {
    case RestSiteOptionId::Heal: return true;
    case RestSiteOptionId::Smith: return player.Deck().UpgradableCardCount() != 0;
    case RestSiteOptionId::Cook: {
        int32_t removable = 0;
        for (CardModel card : player.Deck().Cards()) removable += card.IsRemovable();
        return removable >= 2;
    }
    case RestSiteOptionId::Lift: {
        RelicModel relic = player.GetRelic(RelicId::GIRYA);
        return relic && Girya(&player.sim(), relic.Index()).TimesLifted() < Girya::kMaxLifts;
    }
    case RestSiteOptionId::Dig: return static_cast<bool>(player.GetRelic(RelicId::SHOVEL));
    case RestSiteOptionId::Hatch: return true;
    case RestSiteOptionId::Kindle: return true;
    case RestSiteOptionId::Clone: return true;
    default: throw Unported("rest site option " + std::string(OptionId(option)));
    }
}

StaticVec<RestSiteOptionId, 8> Generate(Player player) {
    // TestMode's generateForTests is never set.
    StaticVec<RestSiteOptionId, 8> list;
    list.push_back(RestSiteOptionId::Heal);
    list.push_back(RestSiteOptionId::Smith);
    // Players.Count > 1: MendRestSiteOption (single player: never).
    // Single-player relic/card additions preserve Hook.ModifyRestSiteOptions order.
    RunState state = player.RunState();
    for (AbstractModel model : state.IterateHookListeners(CombatState{})) {
        if (!state.Contains(model) || !model.Overrides(Member::AbstractModel_TryModifyRestSiteOptions)) continue;
        if (model.Kind() == ModelKind::Card) {
            if (CardModel(&player.sim(), model.Index()).CardId_() == CardId::BYRDONIS_EGG)
                list.push_back(RestSiteOptionId::Hatch);
            else
                throw_unported(model, "TryModifyRestSiteOptions");
        } else if (model.Kind() == ModelKind::Relic) {
            RelicModel relic(&player.sim(), model.Index());
            switch (relic.RelicId_()) {
            case RelicId::GIRYA:
                if (player == relic.Owner() && Girya(&player.sim(), model.Index()).TimesLifted() < Girya::kMaxLifts)
                    list.push_back(RestSiteOptionId::Lift);
                break;
            case RelicId::SHOVEL:
                if (player == relic.Owner()) list.push_back(RestSiteOptionId::Dig);
                break;
            case RelicId::PUMPKIN_CANDLE:
                if (player == relic.Owner()) list.push_back(RestSiteOptionId::Kindle);
                break;
            case RelicId::PAELS_GROWTH:
                if (player == relic.Owner()) list.push_back(RestSiteOptionId::Clone);
                break;
            case RelicId::MEAT_CLEAVER:
                if (player == relic.Owner()) list.push_back(RestSiteOptionId::Cook);
                break;
            default: throw_unported(model, "TryModifyRestSiteOptions");
            }
        } else {
            throw_unported(model, "TryModifyRestSiteOptions");
        }
    }
    return list;
}

Dec GetHealAmount(Player player) {
    return Hook::ModifyRestSiteHealAmount(player.RunState(), player.Creature(), GetBaseHealAmount(player.Creature()));
}

void ExecuteRestSiteHeal(Player player, bool isMimicked) {
    CreatureCmd::Heal(player.Creature(), GetHealAmount(player));
    Hook::AfterRestSiteHeal(player.RunState(), player, isMimicked);
    // List<Reward> rewards = new(); Hook.ModifyRestSiteHealRewards(...);
    // RewardsCmd.OfferCustom(player, rewards): the list is the set's.
    RewardsSet set = RewardsCmd::NewCustomSet(&player.sim());
    Hook::ModifyRestSiteHealRewards(player.RunState(), player, set.Rewards(), isMimicked);
    RewardsCmd::OfferAndAwait(set);
}

void BeginRestSite(Sim* sim) {
    RestSiteState& r = sim->g->run.rest_site;
    r = RestSiteState{};
    r.options = Generate(Player(sim, 1));
}

bool ChooseOption(Sim* sim, int32_t optionIndex) {
    RestSiteState& restSite = sim->g->run.rest_site;
    Player player(sim, 1);
    if (restSite.completed) throw GameError("the rest site has already been completed");
    if (optionIndex < 0 || optionIndex >= static_cast<int32_t>(restSite.options.size())) {
        throw GameError("rest site option index out of range");
    }
    RestSiteOptionId option = restSite.options[static_cast<size_t>(optionIndex)];
    bool flag = OnSelect(player, option);
    sim->g->run.rest_site.last_chosen = static_cast<int8_t>(optionIndex);
    if (!flag) return false;
    // CurrentMapPointHistoryEntry...RestSiteChoices: run history only.
    RestSiteState& r = sim->g->run.rest_site;  // OnSelect may have offered rewards; the state is the same object
    if (r.completed) return true;
    if (Hook::ShouldDisableRemainingRestSiteOptions(player.RunState(), player)) {
        r.options.clear();
    } else {
        r.options.erase_at(static_cast<size_t>(optionIndex));
    }
    if (r.options.empty()) r.completed = true;
    return true;
}

void BeforeLocalRestSiteExited(Sim* sim) {
    RestSiteState& r = sim->g->run.rest_site;
    if (!r.options.empty()) {
        r.options.clear();
        r.completed = true;
    }
}

}  // namespace sts2::RestSite
