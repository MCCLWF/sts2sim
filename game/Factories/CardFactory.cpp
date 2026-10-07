// Factories/CardFactory.cs
#include "game/Factories/CardFactory.h"

#include "game/Commands/CardCmd.h"
#include "game/Helpers/AscensionHelper.h"
#include "game/Hooks/Hook.h"
#include "game/Hooks/HookGuard.h"
#include "game/Models/Canonical.h"
#include "game/Odds/CardRarityOdds.h"
#include "game/Runs/CardCreationOptions.h"

namespace sts2::CardFactory {

namespace {

bool card_unlocked_by_epoch(CardId card, uint64_t unlocked_epoch_bits) {
    int32_t epoch = -1;
    switch (card) {
    case CardId::SCOURGE:
    case CardId::OBLIVION:
    case CardId::COUNTDOWN: epoch = 27; break;  // Necrobinder2Epoch
    case CardId::AFTERLIFE:
    case CardId::SACRIFICE:
    case CardId::CALCIFY: epoch = 30; break;  // Necrobinder5Epoch
    case CardId::SCULPTING_STRIKE:
    case CardId::VEILPIERCER:
    case CardId::BANSHEES_CRY: epoch = 32; break;  // Necrobinder7Epoch
    case CardId::FURNACE:
    case CardId::SPOILS_OF_BATTLE:
    case CardId::SWORD_SAGE: epoch = 38; break;  // Regent2Epoch
    case CardId::ARSENAL:
    case CardId::BEGONE:
    case CardId::SUPERMASSIVE: epoch = 41; break;  // Regent5Epoch
    case CardId::HEAVENLY_DRILL:
    case CardId::LUNAR_BLAST:
    case CardId::PATTER: epoch = 43; break;  // Regent7Epoch
    case CardId::SNAKEBITE:
    case CardId::BUBBLE_BUBBLE:
    case CardId::ACCELERANT: epoch = 50; break;  // Silent2Epoch
    case CardId::REFLEX:
    case CardId::MASTER_PLANNER:
    case CardId::HAND_TRICK: epoch = 53; break;  // Silent5Epoch
    case CardId::HIDDEN_DAGGERS:
    case CardId::BLADE_OF_INK:
    case CardId::PHANTOM_BLADES: epoch = 55; break;  // Silent7Epoch
    default: return true;
    }
    return (unlocked_epoch_bits & (uint64_t{1} << epoch)) != 0;
}

Dec UpgradedCardOddScaling(const Sim& sim) {
    return AscensionHelper::HasAscension(sim, AscensionLevel::Scarcity) ? Dec::lit(125, 3) : Dec::lit(25, 2);
}

// CardRarityExtensions.GetNextHighestRarityWithWrapping
CardRarity GetNextHighestRarityWithWrapping(CardRarity r) {
    switch (r) {
    case CardRarity::None: return CardRarity::None;
    case CardRarity::Basic: return CardRarity::Common;
    case CardRarity::Common: return CardRarity::Uncommon;
    case CardRarity::Uncommon: return CardRarity::Rare;
    case CardRarity::Rare: return CardRarity::Common;
    case CardRarity::Status:
    case CardRarity::Curse:
    case CardRarity::Event:
    case CardRarity::Token:
    case CardRarity::Quest:
    case CardRarity::Ancient: return CardRarity::None;
    default: throw GameError("GetNextHighestRarityWithWrapping: rarity out of range");
    }
}

// GetNextAllowedRarity(rarity, isAllowed). The `list2.Contains` check only
// ever holds the starting rarity.
template <class Allowed>
CardRarity GetNextAllowedRarity(CardRarity rarity, Allowed isAllowed) {
    CardRarity start = rarity;
    while (!isAllowed(rarity) && rarity != CardRarity::None) {
        rarity = GetNextHighestRarityWithWrapping(rarity);
        if (rarity == start) return CardRarity::None;
    }
    return rarity;
}

template <class Allowed>
CardRarity RollForRarity(Player player, CardRarityOddsType rollMethod, CardCreationSource source, Allowed isAllowed,
                         bool forceRarityOddsChange) {
    bool flag2 = forceRarityOddsChange;
    if (!flag2) {
        flag2 = source == CardCreationSource::Encounter &&
                static_cast<uint32_t>(static_cast<int32_t>(rollMethod) - 1) <= 2u;  // Regular/Elite/BossEncounter
    }
    CardRarity rarity = !flag2 ? CardRarityOdds::RollWithBaseOdds(player, rollMethod) : CardRarityOdds::Roll(player, rollMethod);
    return GetNextAllowedRarity(rarity, isAllowed);
}

// The rarity of a canonical card (CardModel.Rarity, which a card may override).
CardRarity RarityOf(Sim* sim, CardId id) {
    CanonicalCard c(sim, id);
    return c.get().Rarity();
}

// CreateForReward(player, blacklist, options)
CardModel CreateForReward(Player player, const StaticVec<CardId, 640>& blacklist, CardCreationOptionsState& options, Rng* rng_override) {
    Sim* sim = &player.sim();
    options = Hook::ModifyCardRewardCreationOptions(player.RunState(), player, options);
    // GetPossibleCards(player).Except(blacklist) (a set operation), then FilterForPlayerCount
    // (single player: not MultiplayerOnly, already so).
    StaticVec<CardId, 640> options2;
    for (CardId c : CardCreationOptions::GetPossibleCards(options, player)) {
        if (blacklist.contains(c) || options2.contains(c)) continue;
        options2.push_back(c);
    }
    StaticVec<CardRarity, 640> rarities;
    for (CardId c : options2) rarities.push_back(RarityOf(sim, c));
    StaticVec<CardId, 640> items;
    CardRarity selectedRarity = CardRarity::None;
    bool hasSelectedRarity = false;
    if (options.rarity_odds == CardRarityOddsType::Uniform) {
        for (size_t i = 0; i < options2.size(); ++i) {
            if (rarities[i] != CardRarity::Basic && rarities[i] != CardRarity::Ancient) items.push_back(options2[i]);
        }
    } else {
        auto allowed = [&](CardRarity r) { return rarities.contains(r); };
        selectedRarity = RollForRarity(player, options.rarity_odds, options.source, allowed,
                                       has_flag(options.flags, CardCreationFlags::ForceRarityOddsChange));
        hasSelectedRarity = true;
        if (selectedRarity == CardRarity::None) throw GameError("Tried to create a card for a reward, but we couldn't generate a valid rarity!");
        for (size_t i = 0; i < options2.size(); ++i) {
            if (rarities[i] == selectedRarity) items.push_back(options2[i]);
        }
    }
    (void)hasSelectedRarity;
    int32_t i = (rng_override ? rng_override->next_item_index(static_cast<int32_t>(items.size()))
                              : player.PlayerRng().Rewards().next_item_index(static_cast<int32_t>(items.size())));
    if (i < 0) throw GameError("Tried to create a card for a reward, but we couldn't generate a valid card!");
    return player.RunState().CreateCard(items[static_cast<size_t>(i)], player);
}

void RollForUpgrade(Player player, CardModel card, Dec baseChance, Rng& rng) {
    Dec num = Dec::from_float(rng.NextFloat());  // (decimal)rng.NextFloat()
    if (card.IsUpgradable()) {
        Dec originalOdds = baseChance;
        if (card.Rarity() != CardRarity::Rare) {
            int32_t currentActIndex = player.RunState().CurrentActIndex();
            originalOdds = originalOdds + Dec(currentActIndex) * UpgradedCardOddScaling(player.sim());
        }
        originalOdds = Hook::ModifyCardRewardUpgradeOdds(player.RunState(), player, card, originalOdds);
        if (num <= originalOdds) CardCmd::Upgrade(card);
    }
}

// The type of a canonical card (CardModel.Type).
CardType TypeOf(Sim* sim, CardId id) {
    CanonicalCard c(sim, id);
    return c.get().Type();
}

// options = Hook.ModifyMerchantCardPool(...) (IEnumerable<CardModel> is not
// ported), .Where(Rarity != Basic), FilterForPlayerCount (single player:
// not MultiplayerOnly).
StaticVec<CardId, 640> MerchantOptions(Player player, const StaticVec<CardId, 640>& options) {
    Sim* sim = &player.sim();
    RequireNoRunOverride(sim, Member::AbstractModel_ModifyMerchantCardPool, "ModifyMerchantCardPool");
    StaticVec<CardId, 640> out;
    for (CardId c : options) {
        if (RarityOf(sim, c) == CardRarity::Basic) continue;
        if (data(c).multiplayer == CardMultiplayerConstraint::MultiplayerOnly) continue;
        out.push_back(c);
    }
    return out;
}

}  // namespace

namespace {

// GetFilteredTransformationOptions(original, originalOptions, isInCombat)
std::vector<CardId> GetFilteredTransformationOptions(CardModel original, const std::vector<CardId>& originalOptions, bool isInCombat) {
    Sim* sim = &original.sim();
    std::vector<CardId> source = originalOptions;
    CardRarity rarity = original.Rarity();
    if (static_cast<uint32_t>(static_cast<int32_t>(rarity) - 8) > 1u) {  // not Status / Curse
        std::vector<CardId> kept;
        for (CardId c : source) {
            if (static_cast<uint32_t>(static_cast<int32_t>(RarityOf(sim, c)) - 2) <= 2u) kept.push_back(c);  // Common..Rare
        }
        source = kept;
    }
    if (isInCombat) {
        std::vector<CardId> kept;
        for (CardId c : source) {
            if (data(c).can_be_generated_in_combat) kept.push_back(c);
        }
        source = kept;
    }
    std::vector<CardId> array;
    for (CardId c : source) {
        if (c == original.CardId_()) continue;
        if (data(c).multiplayer == CardMultiplayerConstraint::MultiplayerOnly) continue;  // FilterForPlayerCount
        array.push_back(c);
    }
    if (array.empty()) throw GameError("All transformation options provided are invalid!");
    return array;
}

// original.CardScope.CreateCard(canonical, original.Owner)
CardModel CreateInScope(CardModel original, CardId canonical) {
    CombatState combat = original.CombatState();
    if (!combat) combat = original.Owner().Creature().CombatState();
    return combat ? combat.CreateCard(canonical, original.Owner()) : original.Owner().RunState().CreateCard(canonical, original.Owner());
}

}  // namespace

bool IsUnlockedByEpoch(CardId card, uint64_t unlocked_epoch_bits) {
    return card_unlocked_by_epoch(card, unlocked_epoch_bits);
}

std::vector<CardId> GetDefaultTransformationOptions(CardModel original, bool isInCombat) {
    CardRarity r = original.Rarity();
    CardPoolId pool = (original.Type() != CardType::Quest && r != CardRarity::Event && r != CardRarity::Ancient && r != CardRarity::Token)
                          ? original.Pool()
                          : CardPoolId::COLORLESS_CARD_POOL;
    StaticVec<CardId, 640> unlocked = GetUnlockedCards(pool, original.Owner().st().profile.unlocked_epoch_bits);
    return GetFilteredTransformationOptions(original, std::vector<CardId>(unlocked.begin(), unlocked.end()), isInCombat);
}

CardModel CreateRandomCardForTransform(CardModel original, bool isInCombat, Rng& rng) {
    std::vector<CardId> options = GetDefaultTransformationOptions(original, isInCombat);
    int32_t i = rng.next_item_index(static_cast<int32_t>(options.size()));
    return CreateInScope(original, options[static_cast<size_t>(i)]);
}

CardModel CreateRandomCardForTransform(CardModel original, const std::vector<CardId>& options, bool isInCombat, Rng& rng) {
    std::vector<CardId> filtered = GetFilteredTransformationOptions(original, options, isInCombat);
    int32_t i = rng.next_item_index(static_cast<int32_t>(filtered.size()));
    return CreateInScope(original, filtered[static_cast<size_t>(i)]);
}

CardList GetForCombat(Player player, CardPoolId pool, int32_t count, Rng& rng, CardType type, bool distinct,
                      CardId excluded, CardRarity rarity) {
    StaticVec<CardId, 640> options;
    for (CardId id : GetUnlockedCards(pool, player.st().profile.unlocked_epoch_bits)) {
        const CardData& card = data(id);
        if (id == excluded || card.rarity == CardRarity::Basic || card.rarity == CardRarity::Ancient ||
            card.rarity == CardRarity::Event || !card.can_be_generated_in_combat ||
            (type != CardType::None && card.type != type) ||
            (rarity != CardRarity::None && card.rarity != rarity) ||
            data(id).multiplayer == CardMultiplayerConstraint::MultiplayerOnly) continue;
        if (!options.contains(id)) options.push_back(id);
    }
    CardList result;
    if (distinct) {
        // GetDistinctForCombat uses IEnumerable.TakeRandom: materialize,
        // UnstableShuffle once, then take the prefix. It is not repeated
        // NextItem-with-removal (the two consume different permutations).
        rng.Shuffle(options);
        for (int32_t i = 0; i < count && i < static_cast<int32_t>(options.size()); ++i)
            result.push_back(player.Creature().CombatState().CreateCard(options[static_cast<size_t>(i)], player));
    } else {
        for (int32_t i = 0; i < count && !options.empty(); ++i) {
            int32_t index = rng.next_item_index(static_cast<int32_t>(options.size()));
            if (index < 0) break;
            CardId id = options[static_cast<size_t>(index)];
            result.push_back(player.Creature().CombatState().CreateCard(id, player));
        }
    }
    return result;
}

CardList GetForCombat(Player player, const StaticVec<CardPoolId, 8>& pools, int32_t count, Rng& rng, CardType type,
                      bool distinct, CardId excluded, CardRarity rarity) {
    StaticVec<CardId, 640> options;
    for (CardPoolId pool : pools) {
        for (CardId id : GetUnlockedCards(pool, player)) {
            const CardData& card = data(id);
            if (id == excluded || card.rarity == CardRarity::Basic || card.rarity == CardRarity::Ancient ||
                card.rarity == CardRarity::Event || !card.can_be_generated_in_combat ||
                (type != CardType::None && card.type != type) ||
                (rarity != CardRarity::None && card.rarity != rarity) ||
                data(id).multiplayer == CardMultiplayerConstraint::MultiplayerOnly || options.contains(id)) continue;
            options.push_back(id);
        }
    }
    CardList result;
    if (distinct) {
        // GetDistinctForCombat calls TakeRandom once over the fully filtered,
        // concatenated source. Do not shuffle each character pool separately.
        rng.Shuffle(options);
        for (int32_t i = 0; i < count && i < static_cast<int32_t>(options.size()); ++i)
            result.push_back(player.Creature().CombatState().CreateCard(options[static_cast<size_t>(i)], player));
    } else {
        for (int32_t i = 0; i < count && !options.empty(); ++i) {
            int32_t index = rng.next_item_index(static_cast<int32_t>(options.size()));
            if (index < 0) break;
            result.push_back(player.Creature().CombatState().CreateCard(options[static_cast<size_t>(index)], player));
        }
    }
    return result;
}

CardList GetForCombatZeroCost(Player player, CardPoolId pool, int32_t count, Rng& rng) {
    StaticVec<CardId, 640> options;
    for (CardId id : GetUnlockedCards(pool, player.st().profile.unlocked_epoch_bits)) {
        CanonicalCard canonical(&player.sim(), id);
        CardModel card = canonical.get();
        if (card.EnergyCost().Canonical() != 0 || card.EnergyCost().CostsX() ||
            !data(id).can_be_generated_in_combat || card.Rarity() == CardRarity::Basic ||
            card.Rarity() == CardRarity::Ancient || card.Rarity() == CardRarity::Event || options.contains(id)) continue;
        options.push_back(id);
    }
    CardList result;
    for (int32_t i = 0; i < count; ++i) {
        int32_t index = rng.next_item_index(static_cast<int32_t>(options.size()));
        if (index < 0) throw GameError("GetForCombatZeroCost: no eligible cards");
        result.push_back(player.Creature().CombatState().CreateCard(options[static_cast<size_t>(index)], player));
    }
    return result;
}

StaticVec<CardId, 640> GetUnlockedCards(CardPoolId pool) {
    return GetUnlockedCards(pool, 0x1ffffffffffffffULL);
}

StaticVec<CardId, 640> GetUnlockedCards(CardPoolId pool, uint64_t unlocked_epoch_bits) {
    StaticVec<CardId, 640> out;
    for (CardId c : data(pool).cards) {
        if (data(c).multiplayer == CardMultiplayerConstraint::MultiplayerOnly ||
            !card_unlocked_by_epoch(c, unlocked_epoch_bits)) continue;
        out.push_back(c);
    }
    return out;
}

StaticVec<CardId, 640> GetUnlockedCards(CardPoolId pool, Player player) {
    StaticVec<CardId, 640> out;
    for (CardId c : data(pool).cards) {
        if (!card_unlocked_by_epoch(c, player.st().profile.unlocked_epoch_bits)) continue;
        if (data(c).multiplayer == CardMultiplayerConstraint::MultiplayerOnly) continue;
        out.push_back(c);
    }
    return out;
}

CardModel CreateForMerchant(Player player, const StaticVec<CardId, 640>& options, CardType type) {
    Sim* sim = &player.sim();
    // player.Character is Deprived: the test character, never.
    StaticVec<CardId, 640> optionsArr = MerchantOptions(player, options);
    CardRarity rarity = Hook::ModifyMerchantCardRarity(
        player.RunState(), player,
        CardRarityOdds::RollWithoutChangingFutureOdds(player, CardRarityOddsType::Shop, player.st().odds.card_rarity));
    CardRarity rarity2 = GetNextAllowedRarity(rarity, [&](CardRarity r) {
        for (CardId c : optionsArr) {
            if (RarityOf(sim, c) == r && TypeOf(sim, c) == type) return true;
        }
        return false;
    });
    if (rarity2 == CardRarity::None) throw GameError("Can't generate valid rarity for merchant card type");
    StaticVec<CardId, 640> items;
    for (CardId c : optionsArr) {
        if (RarityOf(sim, c) == rarity2 && TypeOf(sim, c) == type) items.push_back(c);
    }
    int32_t i = player.PlayerRng().Shops().next_item_index(static_cast<int32_t>(items.size()));
    if (i < 0) throw GameError("CreateForMerchant: no card to create");
    CardModel cardModel = player.RunState().CreateCard(items[static_cast<size_t>(i)], player);
    RollForUpgrade(player, cardModel, Dec(-999999999), player.PlayerRng().Rewards());
    return cardModel;
}

CardModel CreateForMerchant(Player player, const StaticVec<CardId, 640>& options, CardRarity rarity) {
    Sim* sim = &player.sim();
    StaticVec<CardId, 640> source = MerchantOptions(player, options);
    CardRarity modifiedRarity = Hook::ModifyMerchantCardRarity(player.RunState(), player, rarity);
    StaticVec<CardId, 640> items;
    for (CardId c : source) {
        if (RarityOf(sim, c) == modifiedRarity) items.push_back(c);
    }
    int32_t i = player.PlayerRng().Shops().next_item_index(static_cast<int32_t>(items.size()));
    if (i < 0) throw GameError("CreateForMerchant: no card to create");
    CardModel cardModel = player.RunState().CreateCard(items[static_cast<size_t>(i)], player);
    RollForUpgrade(player, cardModel, Dec(-999999999), player.PlayerRng().Rewards());
    return cardModel;
}

CardList CreateForRewardWithRng(Player player, int32_t cardCount, CardCreationOptionsState& options, Rng* rng_override) {
    StaticVec<CardId, 640> list;  // the canonical cards already chosen
    CardList list2;
    for (int32_t i = 0; i < cardCount; ++i) {
        CardModel cardModel = CreateForReward(player, list, options, rng_override);
        list.push_back(cardModel.CardId_());
        list2.push_back(cardModel);
        if (!has_flag(options.flags, CardCreationFlags::NoUpgradeRoll)) {
            Rng& upgrade_rng = rng_override ? *rng_override : player.PlayerRng().Rewards();
            RollForUpgrade(player, cardModel, Dec(0), upgrade_rng);
        }
    }
    ModelList modifiers;
    if (!has_flag(options.flags, CardCreationFlags::NoModifyHooks) &&
        Hook::TryModifyCardRewardOptions(player.RunState(), player, list2, options, modifiers)) {
        Hook::AfterModifyingCardRewardOptions(player.RunState(), modifiers);
    }
    return list2;
}

CardList CreateForReward(Player player, int32_t cardCount, CardCreationOptionsState& options) {
    return CreateForRewardWithRng(player, cardCount, options, nullptr);
}

CardList CreateForReward(Player player, int32_t cardCount, const CardCreationOptionsState& options) {
    CardCreationOptionsState mutableOptions = options;
    return CreateForRewardWithRng(player, cardCount, mutableOptions, nullptr);
}

CardList CreateForReward(Player player, int32_t cardCount, CardCreationOptionsState& options, Rng& rng) {
    return CreateForRewardWithRng(player, cardCount, options, &rng);
}

CardList CreateForReward(Player player, int32_t cardCount, const CardCreationOptionsState& options, Rng& rng) {
    CardCreationOptionsState mutableOptions = options;
    return CreateForRewardWithRng(player, cardCount, mutableOptions, &rng);
}

}  // namespace sts2::CardFactory
