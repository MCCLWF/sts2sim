#include <stdexcept>

#include "engine/sim/state_hash.h"
#include "env/env.h"
#include "game/All.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CardSelectCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Models/Relics/Claws.h"
#include "game/Runs/RunState.h"

using namespace sts2;

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

static void test_can_enchant_predicates() {
    Env env;
    env.Reset(NewRunOptions{.seed = "AUDITENCHANT01", .character = CharacterId::IRONCLAD});
    Player player(&env.sim(), 1);
    auto card = [&](CardId id) { return player.RunState().CreateCard(id, player); };

    CardModel strike = card(CardId::STRIKE_IRONCLAD);
    CardModel defend = card(CardId::DEFEND_IRONCLAD);
    require(CardCmd::CanEnchant(EnchantmentId::CLONE, strike), "base enchantment predicate rejected a legal Strike");
    require(!CardCmd::CanEnchant(EnchantmentId::CLONE, card(CardId::WOUND)), "base predicate accepted a Status");
    require(!CardCmd::CanEnchant(EnchantmentId::CLONE, card(CardId::ASCENDERS_BANE)), "base predicate accepted a Curse");
    require(!CardCmd::CanEnchant(EnchantmentId::CLONE, card(CardId::DOWSING)), "base predicate accepted a Quest");

    CardModel deckUnplayable = card(CardId::STRIKE_IRONCLAD);
    deckUnplayable.AddKeyword(CardKeyword::Unplayable);
    CardPileCmd::Add(deckUnplayable, PileType::Deck);
    require(!CardCmd::CanEnchant(EnchantmentId::CLONE, deckUnplayable),
            "base predicate accepted an Unplayable card in the Deck");

    CardModel alreadyEnchanted = card(CardId::STRIKE_IRONCLAD);
    CardCmd::Enchant(EnchantmentId::CLONE, alreadyEnchanted, Dec(1));
    require(!CardCmd::CanEnchant(EnchantmentId::CLONE, alreadyEnchanted),
            "base predicate accepted a card that already has a non-stackable enchantment");

    CardModel rejected = card(CardId::STRIKE_IRONCLAD);
    const uint64_t beforeRejectedEnchant = hash_state(env.state());
    bool rejectedByExtraPredicate = false;
    try {
        CardCmd::Enchant(EnchantmentId::GOOPY, rejected, Dec(1));
    } catch (const GameError&) {
        rejectedByExtraPredicate = true;
    }
    require(rejectedByExtraPredicate && hash_state(env.state()) == beforeRejectedEnchant && !rejected.Enchantment(),
            "failed extra-predicate enchant did not throw without changing state");

    for (EnchantmentId id : {EnchantmentId::CORRUPTED, EnchantmentId::INSTINCT, EnchantmentId::MOMENTUM,
                             EnchantmentId::SHARP, EnchantmentId::VIGOROUS}) {
        require(CardCmd::CanEnchant(id, strike), "Attack-only enchantment rejected an Attack");
        require(!CardCmd::CanEnchant(id, defend), "Attack-only enchantment accepted a Skill");
    }
    require(CardCmd::CanEnchant(EnchantmentId::IMBUED, defend), "Imbued rejected a Skill");
    require(!CardCmd::CanEnchant(EnchantmentId::IMBUED, strike), "Imbued accepted an Attack");
    require(CardCmd::CanEnchant(EnchantmentId::ROYALLY_APPROVED, strike) &&
                CardCmd::CanEnchant(EnchantmentId::ROYALLY_APPROVED, defend),
            "Royally Approved rejected an Attack or Skill");
    require(!CardCmd::CanEnchant(EnchantmentId::ROYALLY_APPROVED, card(CardId::DEMON_FORM)),
            "Royally Approved accepted a Power");

    require(CardCmd::CanEnchant(EnchantmentId::GOOPY, defend), "Goopy rejected a Defend-tagged card");
    require(!CardCmd::CanEnchant(EnchantmentId::GOOPY, strike), "Goopy accepted a card without the Defend tag");
    require(CardCmd::CanEnchant(EnchantmentId::NIMBLE, defend), "Nimble rejected a card that gains Block");
    require(!CardCmd::CanEnchant(EnchantmentId::NIMBLE, strike), "Nimble accepted a card that does not gain Block");

    require(CardCmd::CanEnchant(EnchantmentId::SLITHER, strike), "Slither rejected an ordinary playable card");
    CardModel unplayableOutsideDeck = card(CardId::STRIKE_IRONCLAD);
    unplayableOutsideDeck.AddKeyword(CardKeyword::Unplayable);
    require(!CardCmd::CanEnchant(EnchantmentId::SLITHER, unplayableOutsideDeck),
            "Slither accepted Unplayable outside the Deck");
    require(!CardCmd::CanEnchant(EnchantmentId::SLITHER, card(CardId::WHIRLWIND)),
            "Slither accepted an X-cost card");

    CardModel localExhaust = card(CardId::STRIKE_IRONCLAD);
    localExhaust.AddKeyword(CardKeyword::Exhaust);
    require(CardCmd::CanEnchant(EnchantmentId::SOULS_POWER, localExhaust),
            "SoulsPower rejected a card with local Exhaust");
    require(!CardCmd::CanEnchant(EnchantmentId::SOULS_POWER, strike),
            "SoulsPower accepted a card without local Exhaust");

    require(CardCmd::CanEnchant(EnchantmentId::SPIRAL, strike), "Spiral rejected a Basic Strike");
    require(CardCmd::CanEnchant(EnchantmentId::SPIRAL, defend), "Spiral rejected a Basic Defend");
    require(!CardCmd::CanEnchant(EnchantmentId::SPIRAL, card(CardId::BASH)),
            "Spiral accepted a Basic card without Strike or Defend tag");
    require(!CardCmd::CanEnchant(EnchantmentId::SPIRAL, card(CardId::SCULPTING_STRIKE)),
            "Spiral accepted a non-Basic Strike-tagged card");

    CardModel zeroAmount = card(CardId::STRIKE_IRONCLAD);
    EnchantmentModel zero = CardCmd::Enchant(EnchantmentId::CLONE, zeroAmount, Dec(0));
    require(zero && zero.Amount() == 0, "Enchant rejected or changed a zero Amount");
    CardModel negativeAmount = card(CardId::STRIKE_IRONCLAD);
    EnchantmentModel negative = CardCmd::Enchant(EnchantmentId::CLONE, negativeAmount, Dec(-1));
    require(negative && negative.Amount() == -1, "Enchant rejected or changed a negative Amount");
}

int main() {
    test_can_enchant_predicates();

    Env env;
    env.Reset(NewRunOptions{.seed = "AUDITCLAWS01", .character = CharacterId::NECROBINDER});
    Player player(&env.sim(), 1);

    // Keep only controlled source cards so the real pickup selector
    // transforms all four in a single batch.
    player.Deck().refs().clear();

    CardModel upgradedAttack = player.RunState().CreateCard(CardId::STRIKE_NECROBINDER, player);
    CardCmd::Upgrade(upgradedAttack);
    CardCmd::Enchant(EnchantmentId::MOMENTUM, upgradedAttack, Dec(2));
    upgradedAttack.st().enchantment_extra[0] = 7;  // Mutable Momentum.ExtraDamage.
    CardPileCmd::Add(upgradedAttack, PileType::Deck);

    CardModel plainAttack = player.RunState().CreateCard(CardId::STRIKE_NECROBINDER, player);
    CardPileCmd::Add(plainAttack, PileType::Deck);

    CardModel usedAttack = player.RunState().CreateCard(CardId::STRIKE_NECROBINDER, player);
    CardCmd::Enchant(EnchantmentId::SWIFT, usedAttack, Dec(2));
    usedAttack.st().enchantment_status = EnchantmentStatus::Disabled;  // Swift disables after its first use.
    CardPileCmd::Add(usedAttack, PileType::Deck);

    CardModel blockSkill = player.RunState().CreateCard(CardId::DEFEND_NECROBINDER, player);
    CardCmd::Enchant(EnchantmentId::NIMBLE, blockSkill, Dec(2));
    CardPileCmd::Add(blockSkill, PileType::Deck);

    {
        CardSelectCmd::ScopedDeterministicSelector select(&env.sim());
        RelicCmd::Obtain(RelicId::CLAWS, player);
    }

    CardList deck = player.Deck().Cards();
    require(deck.size() == 4, "Claws did not replace every selected card");
    int upgradedMauls = 0;
    int unupgradedMauls = 0;
    int unenchantedMauls = 0;
    int momentumMauls = 0;
    int disabledSwiftMauls = 0;
    for (CardModel card : deck) {
        require(card.CardId_() == CardId::MAUL, "Claws replacement was not a Maul");
        if (card.CurrentUpgradeLevel() == 1) ++upgradedMauls;
        if (card.CurrentUpgradeLevel() == 0) ++unupgradedMauls;
        if (card.Enchantment()) {
            if (card.Enchantment().EnchantmentId_() == EnchantmentId::MOMENTUM) {
                require(card.Enchantment().Amount() == 2 && card.st().enchantment_extra[0] == 7,
                        "Claws did not preserve the legal mutable Momentum enchantment state");
                ++momentumMauls;
            } else if (card.Enchantment().EnchantmentId_() == EnchantmentId::SWIFT) {
                require(card.Enchantment().Amount() == 2 &&
                            card.Enchantment().Status() == EnchantmentStatus::Disabled,
                        "Claws did not preserve the disabled Swift enchantment state");
                ++disabledSwiftMauls;
            } else {
                throw std::runtime_error("Claws cloned an enchantment that is illegal on Maul");
            }
        } else {
            ++unenchantedMauls;
        }
    }
    require(upgradedMauls == 1, "Claws did not preserve an original upgrade");
    require(unupgradedMauls == 3, "Claws upgraded an original card that was not upgraded");
    require(unenchantedMauls == 2, "Claws did not skip an enchantment that is illegal on Maul");
    require(momentumMauls == 1 && disabledSwiftMauls == 1,
            "Claws did not clone each legal enchantment exactly once");
}
