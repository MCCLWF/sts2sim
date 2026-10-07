// Factories/PotionFactory.cs
#include "game/Factories/PotionFactory.h"

namespace sts2::PotionFactory {

namespace {

bool potion_unlocked_by_epoch(PotionId potion, uint64_t unlocked_epoch_bits) {
    switch (potion) {
    case PotionId::BEETLE_JUICE:
    case PotionId::DROPLET_OF_PRECOGNITION:
    case PotionId::MAZALETHS_GIFT:
        return (unlocked_epoch_bits & (uint64_t{1} << 35)) != 0;  // Potion1Epoch
    case PotionId::POWDERED_DEMISE:
    case PotionId::SHIP_IN_A_BOTTLE:
    case PotionId::TOUCH_OF_INSANITY:
        return (unlocked_epoch_bits & (uint64_t{1} << 36)) != 0;  // Potion2Epoch
    default:
        return true;
    }
}

bool potion_pool_unlocked_by_epoch(PotionPoolId pool, uint64_t unlocked_epoch_bits) {
    int32_t epoch = -1;
    switch (pool) {
    case PotionPoolId::IRONCLAD_POTION_POOL: epoch = 22; break;  // Ironclad4Epoch
    case PotionPoolId::DEFECT_POTION_POOL: epoch = 13; break;  // Defect4Epoch
    case PotionPoolId::REGENT_POTION_POOL: epoch = 40; break;  // Regent4Epoch
    case PotionPoolId::NECROBINDER_POTION_POOL: epoch = 29; break;  // Necrobinder4Epoch
    case PotionPoolId::SILENT_POTION_POOL: epoch = 52; break;  // Silent4Epoch
    default: return true;
    }
    return (unlocked_epoch_bits & (uint64_t{1} << epoch)) != 0;
}

// CreateRandomPotions(options, count, rng). Potion rarity is a constant per
// potion (PotionData).
StaticVec<PotionId, 8> CreateRandomPotions(StaticVec<PotionId, 96> list, int32_t count, Rng& rng) {
    StaticVec<PotionId, 8> list2;
    for (int32_t i = 0; i < count; ++i) {
        float num = rng.NextFloat();
        PotionRarity rarity = num <= 0.1f ? PotionRarity::Rare : (!(num <= 0.35f) ? PotionRarity::Common : PotionRarity::Uncommon);
        StaticVec<PotionId, 96> matching;
        for (PotionId p : list) {
            if (data(p).rarity == rarity) matching.push_back(p);
        }
        PotionId item = rng.NextItem(matching);  // default (NONE) when none match
        list2.push_back(item);
        list.remove(item);
    }
    return list2;
}

}  // namespace

// GetPotionOptions: unlocked character-pool potions, then the shared pool.
StaticVec<PotionId, 96> GetPotionOptions(Player player) {
    StaticVec<PotionId, 96> out;
    const uint64_t epochs = player.st().profile.unlocked_epoch_bits;
    PotionPoolId character_pool = data(player.Character()).potion_pool;
    if (potion_pool_unlocked_by_epoch(character_pool, epochs))
        for (PotionId p : data(character_pool).potions) out.push_back(p);
    for (PotionId p : data(PotionPoolId::SHARED_POTION_POOL).potions)
        if (potion_unlocked_by_epoch(p, epochs)) out.push_back(p);
    return out;
}

StaticVec<PotionId, 8> CreateRandomPotionsOutOfCombat(Player player, int32_t count, Rng& rng) {
    return CreateRandomPotions(GetPotionOptions(player), count, rng);
}

PotionId CreateRandomPotionOutOfCombat(Player player, Rng& rng) { return CreateRandomPotionsOutOfCombat(player, 1, rng)[0]; }

PotionId CreateRandomPotionInCombat(Player player, Rng& rng) {
    StaticVec<PotionId, 96> options;
    for (PotionId p : GetPotionOptions(player)) {
        if (data(p).can_be_generated_in_combat) options.push_back(p);
    }
    return CreateRandomPotions(options, 1, rng)[0];
}

}  // namespace sts2::PotionFactory
