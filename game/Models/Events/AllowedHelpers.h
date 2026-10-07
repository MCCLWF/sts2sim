// Shared predicates used by the EventModel.IsAllowed partial ports.
#pragma once

#include "game/All.h"
#include "game/Models/Enchantments/SoulsPower.h"
#include "game/Runs/RelicGrabBag.h"

namespace sts2::event_allowed {

inline bool BaseCanEnchant(CardModel card) {
    if (!card) return false;
    CardType type = card.Type();
    if (type == CardType::Status || type == CardType::Curse || type == CardType::Quest) return false;
    if (card.Pile() && card.Pile().Type() == PileType::Deck && card.Keywords().Contains(CardKeyword::Unplayable)) return false;
    if (card.Enchantment()) return false;
    return true;
}

inline bool CanEnchant(CardModel card, EnchantmentId enchantment) {
    if (!BaseCanEnchant(card)) return false;
    switch (enchantment) {
    case EnchantmentId::SOULS_POWER:
        return SoulsPower::CanEnchant(card);
    case EnchantmentId::SPIRAL:
        return card.Rarity() == CardRarity::Basic &&
               (card.HasTag(CardTag::Strike) || card.HasTag(CardTag::Defend));
    case EnchantmentId::PERFECT_FIT:
        return true;
    default:
        return false;
    }
}

inline bool HasEventPet(Player player) {
    for (RelicModel relic : player.Relics()) {
        // Player.HasEventPet uses the AddsPet virtual.  This is distinct from
        // SpawnsPets: Bound Phylactery and other pet spawning relics do not
        // satisfy the event's pet ownership predicate.
        if (relic.AddsPet()) return true;
    }
    for (CardModel card : player.Deck().Cards()) {
        if (card.CardId_() == CardId::BYRDONIS_EGG) return true;
    }
    return false;
}

inline bool IsTradable(RelicModel relic) {
    if (!relic || relic.IsMelted()) return false;
    const RelicData& d = relic.data();
    if (d.spawns_pets || relic.RelicId_() == RelicId::BYRDPIP || relic.RelicId_() == RelicId::PAELS_LEGION) return false;
    if (d.rarity == RelicRarity::Starter || d.rarity == RelicRarity::Event || d.rarity == RelicRarity::Ancient) return false;
    // IsUsedUp/HasUponPickupEffect are model virtuals.  Their default is false;
    // ported concrete relics override them when the stateful check is needed.
    if (relic.IsUsedUp() || relic.HasUponPickupEffect()) return false;
    return true;
}

inline bool HasAvailableRelics(RunState runState, Player player) {
    return RelicGrabBagOps::HasAvailableRelics(&runState.sim(), player.st().relic_grab_bag);
}

}  // namespace sts2::event_allowed
