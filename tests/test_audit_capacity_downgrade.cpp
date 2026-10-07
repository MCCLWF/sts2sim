#include <memory>
#include <stdexcept>
#include <string>

#include "engine/run_setup.h"
#include "engine/sim/alloc.h"
#include "engine/sim/state_hash.h"
#include "env/env.h"
#include "game/All.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/PotionCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Localization/DynamicVars/DynamicVarSet.h"
#include "game/Models/Relics/AlchemicalCoffer.h"
#include "game/Models/Relics/PhialHolster.h"
#include "game/Models/Relics/PotionBelt.h"
#include "game/Runs/RunState.h"

using namespace sts2;

static void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

static CardModel make_card(Player player, CardId id) {
    return player.RunState().CreateCard(id, player);
}

static void test_potion_capacity_and_snapshot() {
    Env env;
    env.ResetCombat({.seed = "AUDITPOTIONCAP", .character = CharacterId::IRONCLAD},
                    EncounterId::CULTISTS_NORMAL);
    Player player(&env.sim(), 1);
    require(player.MaxPotionCount() == 3, "run did not start with three potion slots");

    RelicCmd::Obtain(RelicId::ALCHEMICAL_COFFER, player);
    require(player.MaxPotionCount() == 7, "Alchemical Coffer did not add four potion slots");
    RelicCmd::Obtain(RelicId::POTION_BELT, player);
    require(player.MaxPotionCount() == 9, "Potion Belt did not raise the known combination to nine slots");
    RelicCmd::Obtain(RelicId::PHIAL_HOLSTER, player);
    require(player.MaxPotionCount() == 10, "Phial Holster did not raise the known combination to ten slots");
    require(player.PotionSlots().size() == 10, "ten potion slots were not enumerable");

    auto procured = PotionCmd::TryToProcure(PotionId::STRENGTH_POTION, player, 9);
    require(procured.success && player.GetPotionAtSlotIndex(9), "the tenth potion slot could not hold a potion");

    const std::string snapshot = env.ExportSnapshotJson();
    auto restored = Env::ImportSnapshotJson(snapshot);
    require(restored->Hash() == env.Hash(), "ten-slot snapshot changed the logical state hash");
    require(restored->ExportSnapshotJson() == snapshot, "ten-slot snapshot did not round-trip exactly");
}

static void test_enchantment_modify_card_after_downgrade() {
    auto state = std::make_unique<GameState>();
    new_run(*state, NewRunOptions{.seed = "AUDITDOWNGRADE", .character = CharacterId::IRONCLAD});
    Sim sim(*state);
    Player player(&sim, 1);

    CardModel ember = make_card(player, CardId::STRIKE_IRONCLAD);
    CardCmd::Upgrade(ember);
    CardCmd::Enchant(EnchantmentId::TEZCATARAS_EMBER, ember, Dec(1));
    require(ember.EnergyCost().GetWithModifiers(CostModifiers::None) == 0 &&
                ember.Keywords().Contains(CardKeyword::Eternal),
            "Tezcataras Ember did not initially make the Strike free and Eternal");
    CardCmd::Downgrade(ember);
    require(ember.CurrentUpgradeLevel() == 0 &&
                ember.EnergyCost().GetWithModifiers(CostModifiers::None) == 0 &&
                ember.Keywords().Contains(CardKeyword::Eternal),
            "downgrade did not reapply Tezcataras Ember's cost and keyword changes");

    CardModel steady = make_card(player, CardId::STRIKE_IRONCLAD);
    CardCmd::Upgrade(steady);
    CardCmd::Enchant(EnchantmentId::STEADY, steady, Dec(1));
    CardCmd::Downgrade(steady);
    require(steady.Keywords().Contains(CardKeyword::Retain), "downgrade lost Steady's Retain keyword");

    CardModel royal = make_card(player, CardId::STRIKE_IRONCLAD);
    CardCmd::Upgrade(royal);
    CardCmd::Enchant(EnchantmentId::ROYALLY_APPROVED, royal, Dec(1));
    CardCmd::Downgrade(royal);
    require(royal.Keywords().Contains(CardKeyword::Innate) && royal.Keywords().Contains(CardKeyword::Retain),
            "downgrade lost Royally Approved's Innate or Retain keyword");

    CardModel goopy = make_card(player, CardId::DEFEND_IRONCLAD);
    CardCmd::Upgrade(goopy);
    CardCmd::Enchant(EnchantmentId::GOOPY, goopy, Dec(1));
    CardCmd::Downgrade(goopy);
    require(goopy.Keywords().Contains(CardKeyword::Exhaust), "downgrade lost Goopy's Exhaust keyword");

    CardModel souls = make_card(player, CardId::FEED);
    require(souls.Keywords().Contains(CardKeyword::Exhaust), "Feed fixture has no canonical Exhaust keyword");
    CardCmd::Upgrade(souls);
    CardCmd::Enchant(EnchantmentId::SOULS_POWER, souls, Dec(1));
    require(!souls.Keywords().Contains(CardKeyword::Exhaust), "SoulsPower did not initially remove Exhaust");
    CardCmd::Downgrade(souls);
    require(!souls.Keywords().Contains(CardKeyword::Exhaust), "downgrade did not reapply SoulsPower's keyword change");

    CardModel adroit = make_card(player, CardId::DEFEND_IRONCLAD);
    CardCmd::Upgrade(adroit);
    CardCmd::Enchant(EnchantmentId::ADROIT, adroit, Dec(3));
    require(DynamicVarSet(adroit.Enchantment()).Block().BaseValue() == Dec(3), "Adroit did not recalculate its Block value on enchant");
    CardCmd::Downgrade(adroit);
    require(DynamicVarSet(adroit.Enchantment()).Block().BaseValue() == Dec(3), "downgrade did not recalculate Adroit's Block value");
    adroit.st().enchantment_status = EnchantmentStatus::Disabled;
    const uint64_t beforeRejectedReenchant = hash_state(*state);
    bool rejectedReenchant = false;
    try {
        CardCmd::Enchant(EnchantmentId::ADROIT, adroit, Dec(1));
    } catch (const GameError&) {
        rejectedReenchant = true;
    }
    require(rejectedReenchant, "source-invalid same-type re-enchant did not throw");
    require(hash_state(*state) == beforeRejectedReenchant && adroit.Enchantment().Amount() == 3 &&
                adroit.Enchantment().Status() == EnchantmentStatus::Disabled,
            "rejected re-enchant changed simulation or enchantment state");
}

int main() {
    test_potion_capacity_and_snapshot();
    test_enchantment_modify_card_after_downgrade();
}
