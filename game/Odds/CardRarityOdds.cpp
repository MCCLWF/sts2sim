// Odds/CardRarityOdds.cs
#include "game/Odds/CardRarityOdds.h"

#include <algorithm>

#include "game/Helpers/AscensionHelper.h"

namespace sts2::CardRarityOdds {

namespace {

constexpr float kBaseRarityOffset = -0.05f;
constexpr float kMaxRarityOffset = 0.4f;

float RarityGrowth(const Sim& sim) { return AscensionHelper::GetValueIfAscension(sim, AscensionLevel::Scarcity, 0.005f, 0.01f); }

// GetBaseOdds for the Rare and Uncommon rows (the only ones the rolls read;
// regularCommonOdds, a static initialized once per process, is never read).
float GetBaseOdds(const Sim& sim, CardRarityOddsType type, CardRarity rarity) {
    auto asc = [&](float a, float b) { return AscensionHelper::GetValueIfAscension(sim, AscensionLevel::Scarcity, a, b); };
    switch (type) {
    case CardRarityOddsType::EliteEncounter:
        if (rarity == CardRarity::Uncommon) return 0.4f;
        if (rarity == CardRarity::Rare) return asc(0.05f, 0.1f);  // EliteRareOdds
        if (rarity == CardRarity::Common) return asc(0.549f, 0.5f);
        break;
    case CardRarityOddsType::BossEncounter:
        if (rarity == CardRarity::Uncommon) return 0.0f;
        if (rarity == CardRarity::Rare) return 1.0f;
        if (rarity == CardRarity::Common) return 0.0f;
        break;
    case CardRarityOddsType::Shop:
        if (rarity == CardRarity::Uncommon) return 0.37f;
        if (rarity == CardRarity::Rare) return asc(0.045f, 0.09f);  // ShopRareOdds
        if (rarity == CardRarity::Common) return asc(0.585f, 0.54f);
        break;
    case CardRarityOddsType::RegularEncounter:
        if (rarity == CardRarity::Uncommon) return 0.37f;
        if (rarity == CardRarity::Rare) return asc(0.0149f, 0.03f);  // RegularRareOdds
        if (rarity == CardRarity::Common) return 1.0f - GetBaseOdds(sim, type, CardRarity::Rare) - GetBaseOdds(sim, type, CardRarity::Uncommon);
        break;
    case CardRarityOddsType::Uniform:
        if (rarity == CardRarity::Common || rarity == CardRarity::Uncommon || rarity == CardRarity::Rare) return 0.33f;
        break;
    default: throw GameError("CardRarityOdds: type out of range");
    }
    throw GameError("CardRarityOdds: rarity out of range");
}

}  // namespace

CardRarity RollWithoutChangingFutureOdds(Player player, CardRarityOddsType type, float offset) {
    const Sim& sim = player.sim();
    float num = player.PlayerRng().Rewards().NextFloat();
    float num2 = GetBaseOdds(sim, type, CardRarity::Rare) + offset;
    float baseOdds = GetBaseOdds(sim, type, CardRarity::Uncommon);
    if (num < num2) return CardRarity::Rare;
    if (num < baseOdds + num2) return CardRarity::Uncommon;
    return CardRarity::Common;
}

CardRarity Roll(Player player, CardRarityOddsType type) {
    float& currentValue = player.st().odds.card_rarity;
    CardRarity cardRarity = RollWithoutChangingFutureOdds(player, type, type == CardRarityOddsType::BossEncounter ? 0.0f : currentValue);
    if (cardRarity == CardRarity::Rare) {
        currentValue = kBaseRarityOffset;
    } else {
        currentValue = std::min(currentValue + RarityGrowth(player.sim()), kMaxRarityOffset);
    }
    return cardRarity;
}

CardRarity RollWithBaseOdds(Player player, CardRarityOddsType type) {
    const Sim& sim = player.sim();
    float baseOdds = GetBaseOdds(sim, type, CardRarity::Rare);
    float baseOdds2 = GetBaseOdds(sim, type, CardRarity::Uncommon);
    float num = player.PlayerRng().Rewards().NextFloat();
    if (num < baseOdds) return CardRarity::Rare;
    if (num < baseOdds2 + baseOdds) return CardRarity::Uncommon;
    return CardRarity::Common;
}

}  // namespace sts2::CardRarityOdds
