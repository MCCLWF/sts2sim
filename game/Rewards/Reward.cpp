// Rewards/Reward.cs, GoldReward.cs, PotionReward.cs, RelicReward.cs, CardReward.cs,
// Entities/CardRewardAlternatives/CardRewardAlternative.cs
#include "game/Rewards/Reward.h"

#include "engine/sim/alloc.h"
#include "game/All.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Commands/PotionCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Factories/PotionFactory.h"
#include "game/Factories/RelicFactory.h"
#include "game/Hooks/Hook.h"
#include "game/Runs/CardCreationOptions.h"
#include "game/Hooks/HookGuard.h"
#include "game/Models/Relics/PaelsWing.h"

namespace sts2 {

namespace {

Reward NewReward(Player player, RewardType type) {
    Sim* sim = &player.sim();
    RewardRef r = sim->g->rewards.alloc();
    sim->g->rewards[r].type = type;
    return Reward(sim, r);
}

// CardRewardAlternative (the ones the game itself defines).
enum class CardRewardAlternative : uint8_t { Skip, Reroll, Sacrifice };
using Alternatives = StaticVec<CardRewardAlternative, 2>;

// CardRewardAlternative.Generate(cardReward)
Alternatives GenerateAlternatives(Reward cardReward) {
    Alternatives list;
    if (cardReward.CanSkip()) list.push_back(CardRewardAlternative::Skip);
    if (cardReward.CanReroll()) list.push_back(CardRewardAlternative::Reroll);
    // Hook.ModifyCardRewardAlternatives iterates run listeners in order.
    RunState runState(&cardReward.sim());
    for (AbstractModel model : runState.IterateHookListeners(CombatState{})) {
        if (!runState.Contains(model) || !model.Overrides(Member::AbstractModel_TryModifyCardRewardAlternatives)) continue;
        if (model.Kind() == ModelKind::Relic && RelicModel(&cardReward.sim(), model.Index()).RelicId_() == RelicId::PAELS_WING) {
            if (list.size() >= 2) throw GameError("More than 2 card reward alternatives are not supported.");
            list.push_back(CardRewardAlternative::Sacrifice);
        } else {
            throw_unported(model, "TryModifyCardRewardAlternatives");
        }
    }
    return list;
}

}  // namespace

// ------------------------------------------------------------ constructors

Reward Reward::NewGoldReward(sts2::Player player, int32_t min, int32_t max, bool wasGoldStolenBack) {
    Reward r = NewReward(player, RewardType::Gold);
    r.st().gold_min = min;
    r.st().gold_max = max;
    r.st().gold_stolen_back = wasGoldStolenBack;
    return r;
}

Reward Reward::NewGoldReward(sts2::Player player, int32_t amount, bool wasGoldStolenBack) {
    Reward r = NewGoldReward(player, amount, amount, wasGoldStolenBack);
    r.st().gold_amount = amount;
    return r;
}

Reward Reward::NewPotionReward(sts2::Player player) { return NewReward(player, RewardType::Potion); }

Reward Reward::NewPotionReward(sts2::Player player, PotionModel potion) {
    Reward r = NewReward(player, RewardType::Potion);
    r.st().potion = potion.Index();
    return r;
}

Reward Reward::NewRelicReward(sts2::Player player) { return NewReward(player, RewardType::Relic); }

Reward Reward::NewRelicReward(sts2::Player player, RelicRarity rarity) {
    Reward r = NewReward(player, RewardType::Relic);
    r.st().relic_rarity = rarity;
    return r;
}

Reward Reward::NewRelicReward(sts2::Player player, RelicModel relic) {
    Reward r = NewReward(player, RewardType::Relic);
    r.st().relic = relic.Index();
    r.st().relic_predetermined = true;
    return r;
}

Reward Reward::NewCardRemovalReward(sts2::Player player) {
    return NewReward(player, RewardType::RemoveCard);
}

// CardReward(options, cardCount, player): Options and RerollOptions are the
// same record (WithFlags mutates it), with IsCardReward.
Reward Reward::NewCardReward(sts2::Player player, const CardList& cards) {
    Reward r = NewReward(player, RewardType::Card);
    r.st().cards_manually_set = true;
    r.st().can_skip = true;
    r.st().relic_obtained_subscribed = true;
    for (CardModel c : cards) r.st().cards.push_back(c.Index());
    return r;
}

Reward Reward::NewCardReward(sts2::Player player, CardCreationOptionsState options, int32_t cardCount) {
    Reward r = NewReward(player, RewardType::Card);
    r.st().option_count = cardCount;
    CardCreationOptions::WithFlags(options, CardCreationFlags::IsCardReward);
    r.st().options = options;
    r.st().reroll_options = options;
    r.st().relic_obtained_subscribed = true;  // player.RelicObtained += OnRelicObtained
    return r;
}

Reward Reward::NewSpecialCardReward(sts2::Player player, CardModel card) {
    Reward r = NewReward(player, RewardType::SpecialCard);
    r.st().cards.push_back(card.Index());
    return r;
}

// ------------------------------------------------------------------ Reward

sts2::Player Reward::Player() const { return sts2::Player(sim_, 1); }

int32_t Reward::RewardsSetIndex() const {
    switch (Type()) {
    case RewardType::Gold: return 1;
    case RewardType::Potion: return 2;
    case RewardType::Relic: return 3;
    case RewardType::SpecialCard: return 4;
    case RewardType::Card: return 5;
    case RewardType::RemoveCard: return 7;
    default: throw Unported("RewardsSetIndex of this reward type");
    }
}

bool Reward::IsPopulated() const {
    switch (Type()) {
    case RewardType::Gold: return st().gold_amount >= 0;
    case RewardType::Potion: return st().potion != 0;
    case RewardType::Relic: return st().relic != 0;
    case RewardType::SpecialCard:
    case RewardType::Card: return !st().cards.empty();
    case RewardType::RemoveCard: return true;
    default: throw Unported("IsPopulated of this reward type");
    }
}

PotionModel Reward::Potion() const { return PotionModel(sim_, st().potion); }
RelicModel Reward::Relic() const { return RelicModel(sim_, st().relic); }

CardList Reward::Cards() const {
    CardList out;
    for (CardRef c : st().cards) out.push_back(CardModel(sim_, c));
    return out;
}

void Reward::Populate() const {
    RewardState& s = st();
    sts2::Player player = Player();
    Rng& reward_rng = s.use_event_rng ? sim_->g->run.events[0].rng : player.PlayerRng().Rewards();
    switch (Type()) {
    case RewardType::SpecialCard: return;  // Already holds the specific mutable card.
    case RewardType::RemoveCard: return;  // CardRemovalReward has nothing to populate.
    case RewardType::Gold:  // GoldReward.Populate (no _rngOverride)
        s.gold_amount = reward_rng.NextInt(s.gold_min, s.gold_max + 1);
        return;
    case RewardType::Potion:  // PotionReward.Populate
        if (s.potion == 0) {
            PotionId potion = PotionFactory::CreateRandomPotionOutOfCombat(player, reward_rng);
            s.potion = alloc_potion(*sim_->g, potion, 0);  // ToMutable
        }
        return;
    case RewardType::Relic: {  // RelicReward.Populate
        if (s.relic != 0) return;
        RelicId relic = s.relic_rarity == RelicRarity::None ? RelicFactory::PullNextRelicFromFront(player, reward_rng)
                                                            : RelicFactory::PullNextRelicFromFront(player, s.relic_rarity);
        s.relic = alloc_relic(*sim_->g, relic, 0);  // ToMutable
        return;
    }
    case RewardType::Card: {  // CardReward.Populate
        CardCreationOptionsState& options = s.has_been_rerolled ? s.reroll_options : s.options;
        if (s.cards_manually_set && !s.has_been_rerolled) {
            CardList cards = Cards();
            ModelList modifiers;
            if (Hook::TryModifyCardRewardOptions(player.RunState(), player, cards, options, modifiers)) {
                Hook::AfterModifyingCardRewardOptions(player.RunState(), modifiers);
            }
            s.cards.clear();
            for (CardModel c : cards) s.cards.push_back(c.Index());
        } else if (s.cards.empty()) {
            CardList collection = s.use_event_rng
                ? CardFactory::CreateForReward(player, s.option_count, options, reward_rng)
                : CardFactory::CreateForReward(player, s.option_count, options);
            RewardState& s2 = st();  // CreateForReward may run hooks; re-read the state
            // Ordinary CardReward stores the same mutable record as Options
            // and RerollOptions. Creation hooks mutate it (DingyRug and
            // PrismaticGem), so preserve that alias in the POD copies.
            if (!s2.cards_manually_set) {
                if (s2.has_been_rerolled) s2.options = s2.reroll_options;
                else s2.reroll_options = s2.options;
            }
            s2.cards.clear();
            for (CardModel c : collection) s2.cards.push_back(c.Index());
            GenerateAlternatives(*this);  // extraOptions: only the screen reads them
            if (s2.upgrade_after_generated) {
                for (CardModel card : collection) CardCmd::Upgrade(card);
            }
            // RefreshOptions is UI only.
        }
        return;
    }
    default: throw Unported("Populate of this reward type");
    }
}

// CardReward.OnRelicObtained: the relic may change the offered cards.
void Reward::OnRelicObtained(RelicModel relic) const {
    if (Type() != RewardType::Card) return;
    CardList cards = Cards();
    if (relic.TryModifyCardRewardOptions(Player(), cards, st().options)) relic.AfterModifyingRewards();
    if (relic.TryModifyCardRewardOptionsLate(Player(), cards, st().options)) relic.AfterModifyingRewards();
    st().cards.clear();
    for (CardModel c : cards) st().cards.push_back(c.Index());
}

void Reward::Reroll() const {
    st().can_reroll = false;
    // CardChoices (run history) is not modelled.
    st().has_been_rerolled = true;
    st().cards.clear();
    Populate();
}

bool Reward::OnSelect() const {
    RewardState& s = st();
    sts2::Player player = Player();
    switch (Type()) {
    case RewardType::Gold:
        PlayerCmd::GainGold(Dec(s.gold_amount), player, s.gold_stolen_back);
        return true;
    case RewardType::Potion: {
        sts2::Player::PotionProcureResult result = PotionCmd::TryToProcure(Potion(), player);
        if (result.success) {
            st().potion_was_taken = true;  // ClaimedPotion = Potion
            return true;
        }
        if (result.failureReason == PotionProcureFailureReason::TooFull) return false;
        st().potion_was_taken = true;
        return true;
    }
    case RewardType::Relic:
        RelicCmd::Obtain(Relic(), player);
        st().relic_was_taken = true;
        return true;
    case RewardType::SpecialCard:
        CardPileCmd::Add(CardModel(sim_, st().cards[0]), PileType::Deck);
        return true;
    case RewardType::RemoveCard: {
        CardSelectorPrefs prefs(1);
        prefs.Cancelable = true;
        prefs.RequireManualConfirmation = true;
        CardList selected = CardSelectCmd::FromDeckForRemoval(player, prefs);
        if (selected.empty()) return false;
        CardPileCmd::RemoveFromDeck(selected[0]);
        return true;
    }
    case RewardType::Card: return CardRewardOnSelect();
    default: throw Unported("OnSelect of this reward type");
    }
}

// CardReward.OnSelect. The card selection screen is a player choice: the
// answer is an index into the cards followed by the alternatives (Skip,
// Reroll), or -1 for no selection.
bool Reward::CardRewardOnSelect() const {
    bool rewardComplete = false;
    bool endSelection = false;
    Alternatives cardRewardOption = GenerateAlternatives(*this);
    while (!endSelection) {
        int32_t cardCount = static_cast<int32_t>(st().cards.size());
        ChoiceRequest request;
        request.kind = ChoiceKind::CardSelect;
        request.option_count = cardCount + static_cast<int32_t>(cardRewardOption.size());
        request.min_select = 0;
        request.max_select = 1;
        request.prompt = "CardReward";
        for (CardRef c : st().cards) {
            request.cards.push_back(c);
            request.options.push_back({.kind = ChoiceOptionKind::Card, .cards = {c}});
        }
        for (CardRewardAlternative alternative : cardRewardOption)
            request.options.push_back({.kind = ChoiceOptionKind::Alternative,
                .key = alternative == CardRewardAlternative::Skip ? "Skip" :
                       alternative == CardRewardAlternative::Reroll ? "REROLL" : "SACRIFICE",
                // Populate refreshes the actual screen after Reroll. The
                // backend callback list is retained, but its old REROLL
                // index is no longer offered by the UI.
                .enabled = alternative != CardRewardAlternative::Reroll || CanReroll()});
        request.can_skip = CanSkip();
        request.source = "CardSelectCmd";  // the selector is asked from CardReward.OnSelect
        int32_t num = -1;
        if (CardSelectCmd::FirstSelectorActive(sim_)) {
            // VakuuCardSelector.GetSelectedCardReward returns the first offered
            // card, or null when there are no cards, and ignores alternatives.
            if (cardCount > 0) num = 0;
        } else {
            num = sim_->Choose(request);
        }
        if (num < -1 || num >= request.option_count) throw GameError("CardReward: choice out of range");
        CardModel obtainedCard;
        bool hasAlternative = false;
        CardRewardAlternative alternative{};
        if (num >= 0) {
            if (num < cardCount) {
                obtainedCard = CardModel(sim_, st().cards[static_cast<size_t>(num)]);
                rewardComplete = true;
                endSelection = !Hook::ShouldAllowSelectingMoreCardRewards(Player().RunState(), Player(), *this);
            } else {
                hasAlternative = true;
                alternative = cardRewardOption[static_cast<size_t>(num - cardCount)];
                // Skip leaves the reward untaken; Sacrifice completes it.
                rewardComplete = alternative == CardRewardAlternative::Sacrifice;
                endSelection = alternative != CardRewardAlternative::Reroll;
            }
        } else {
            rewardComplete = false;
            endSelection = true;
        }
        if (!(obtainedCard || hasAlternative || rewardComplete)) continue;
        if (obtainedCard) {
            CardPileAddResult cardPileAddResult = CardPileCmd::Add(obtainedCard, PileType::Deck);
            if (cardPileAddResult.success) {
                obtainedCard = cardPileAddResult.cardAdded;
                st().cards.remove(obtainedCard.Index());  // _cards.RemoveAll(c => c.Card == obtainedCard)
            }
        } else if (hasAlternative) {
            if (alternative == CardRewardAlternative::Reroll) Reroll();
            if (alternative == CardRewardAlternative::Sacrifice) {
                bool found = false;
                for (RelicModel relic : Player().Relics()) {
                    if (relic.RelicId_() != RelicId::PAELS_WING) continue;
                    PaelsWing(&sim(), relic.Index()).OnSacrifice();
                    found = true;
                    break;
                }
                if (!found) throw GameError("Pael's Wing reward alternative has no source relic");
            }
        }
    }
    st().relic_obtained_subscribed = false;  // player.RelicObtained -= OnRelicObtained
    // CardChoices (run history) is not modelled.
    return rewardComplete;
}

void Reward::OnSkipped() const {
    // Potion / relic / card choices go to the run history only.
    if (Type() == RewardType::Card) st().relic_obtained_subscribed = false;
}

// Reward.SelectUnsynchronized (no LinkedRewardSet parent).
bool Reward::SelectUnsynchronized() const {
    bool success = OnSelect();
    if (success) {
        Hook::AfterRewardTaken(Player().RunState(), Player(), *this);
        st().successfully_selected = true;
    }
    return success;
}

// -------------------------------------------------------------- RewardList

size_t RewardList::size() const { return sim_->g->run.rewards.rewards.size(); }
Reward RewardList::operator[](size_t i) const { return Reward(sim_, sim_->g->run.rewards.rewards[i]); }
void RewardList::Add(Reward reward) const { sim_->g->run.rewards.rewards.push_back(reward.Index()); }
bool RewardList::contains(Reward reward) const { return sim_->g->run.rewards.rewards.contains(reward.Index()); }

}  // namespace sts2
