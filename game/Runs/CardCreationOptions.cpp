// Runs/CardCreationOptions.cs
#include "game/Runs/CardCreationOptions.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/Canonical.h"

namespace sts2::CardCreationOptions {

Options Make(std::initializer_list<CardPoolId> cardPools, CardCreationSource source, CardRarityOddsType rarityOdds) {
    Options o;
    for (CardPoolId p : cardPools) o.pools.push_back(p);
    o.source = source;
    o.rarity_odds = rarityOdds;
    return o;
}

Options ForRoom(Player player, RoomType roomType) {
    CardCreationSource source;
    switch (roomType) {
    case RoomType::Monster:
    case RoomType::Elite:
    case RoomType::Boss: source = CardCreationSource::Encounter; break;
    case RoomType::Shop: source = CardCreationSource::Shop; break;
    case RoomType::Event: throw GameError("ForRoom should not be used in event rooms");
    default: source = CardCreationSource::Other; break;
    }
    CardRarityOddsType odds;
    switch (roomType) {
    case RoomType::Monster: odds = CardRarityOddsType::RegularEncounter; break;
    case RoomType::Elite: odds = CardRarityOddsType::EliteEncounter; break;
    case RoomType::Boss: odds = CardRarityOddsType::BossEncounter; break;
    case RoomType::Shop: odds = CardRarityOddsType::Shop; break;
    default: odds = CardRarityOddsType::RegularEncounter; break;
    }
    return Make({data(player.Character()).card_pool}, source, odds);
}

Options ForNonCombatWithDefaultOdds(std::initializer_list<CardPoolId> cardPools) {
    Options o = Make(cardPools, CardCreationSource::Other, CardRarityOddsType::RegularEncounter);
    return WithFlags(o, CardCreationFlags::NoUpgradeRoll);
}

Options ForNonCombatWithUniformOdds(std::initializer_list<CardPoolId> cardPools) {
    Options o = Make(cardPools, CardCreationSource::Other, CardRarityOddsType::Uniform);
    return WithFlags(o, CardCreationFlags::NoUpgradeRoll);
}

// CardPools.SelectMany(p => p.GetUnlockedCards(unlockState, runState.CardMultiplayerConstraint))
// .Where(CardPoolFilter), followed by CardFactory.FilterForPlayerCount.
StaticVec<CardId, 640> GetPossibleCards(const Options& o, Player player) {
    if (o.filter != 0 && o.filter != ZeroCanonicalCostNonX && o.filter != OnlyMultiplayerCards)
        throw Unported("CardCreationOptions.CardPoolFilter");
    StaticVec<CardId, 640> out;
    const bool multiplayerRun = player.RunState().Players().size() > 1;
    for (CardPoolId pool : o.pools) {
        for (CardId c : data(pool).cards) {
            if (!CardFactory::IsUnlockedByEpoch(c, player.st().profile.unlocked_epoch_bits)) continue;
            CanonicalCard canonical(&player.sim(), c);
            CardModel card = canonical.get();
            const CardMultiplayerConstraint constraint = card.MultiplayerConstraint();
            if (multiplayerRun && constraint == CardMultiplayerConstraint::SingleplayerOnly) continue;
            if (!multiplayerRun && constraint == CardMultiplayerConstraint::MultiplayerOnly &&
                o.filter != OnlyMultiplayerCards) continue;
            if (o.excluded_ids.contains(c)) continue;
            if (o.filter == OnlyMultiplayerCards &&
                constraint != CardMultiplayerConstraint::MultiplayerOnly) continue;
            if (o.required_rarity != CardRarity::None || o.required_type != CardType::None || o.filter != 0) {
                if (o.required_rarity != CardRarity::None && card.Rarity() != o.required_rarity) continue;
                if (o.required_type != CardType::None && card.Type() != o.required_type) continue;
                if (o.filter == ZeroCanonicalCostNonX &&
                    (card.EnergyCost().Canonical() != 0 || card.EnergyCost().CostsX())) continue;
            }
            out.push_back(c);
        }
    }
    return out;
}

}  // namespace sts2::CardCreationOptions
