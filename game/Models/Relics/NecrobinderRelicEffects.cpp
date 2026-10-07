#include "game/Models/Relics/NecrobinderRelics.h"

#include <algorithm>

#include "game/Commands/CardPileCmd.h"
#include "game/Commands/Builders/AttackCommand.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/Canonical.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2::NecrobinderRelicEffects {
namespace {

bool contains(const CreatureList& participants, Creature creature) {
    return creature && std::find(participants.begin(), participants.end(), creature) != participants.end();
}

}  // namespace

Dec ModifyDamageMultiplicative(RelicModel relic, Creature target, Dec amount, ValueProp props, Creature dealer,
                               CardModel, const CardPlay*) {
    if (relic.RelicId_() != RelicId::UNDYING_SIGIL || !dealer || !IsPoweredAttack(props) ||
        target != relic.Owner().Creature() || dealer == relic.Owner().Creature() ||
        dealer.CurrentHp() > dealer.GetPowerAmount(PowerId::DOOM_POWER)) return Dec(1);
    return relic.DynamicVars()["DamageDecrease"].BaseValue();
}

void AfterAttack(RelicModel relic, PlayerChoiceContext&, AttackCommand& command) {
    Creature attacker = command.Attacker();
    if (!attacker || !attacker.IsPet() || attacker.Monster().MonsterId_() != MonsterId::OSTY ||
        attacker.PetOwner() != relic.Owner()) return;
    CreatureCmd::GainBlock(relic.Owner().Creature(), relic.DynamicVars().Block(), nullptr);
}

void AfterCardPlayed(RelicModel relic, PlayerChoiceContext&, const CardPlay* play) {
    if (relic.RelicId_() != RelicId::IVORY_TILE || !play || play->Card.Owner() != relic.Owner() ||
        play->Resources.EnergyValue < relic.DynamicVars()["EnergyThreshold"].IntValue()) return;
    PlayerCmd::GainEnergy(relic.DynamicVars().Energy().BaseValue(), relic.Owner());
}

void AfterDiedToDoom(RelicModel relic, PlayerChoiceContext&, const CreatureList& creatures) {
    if (relic.RelicId_() != RelicId::BOOK_REPAIR_KNIFE) return;
    int32_t fatalDeaths = 0;
    for (Creature creature : creatures) {
        if (creature == relic.Owner().Creature()) continue;
        bool triggersFatal = true;
        for (PowerModel power : creature.Powers()) triggersFatal &= power.ShouldOwnerDeathTriggerFatal();
        if (triggersFatal) ++fatalDeaths;
    }
    if (fatalDeaths > 0) CreatureCmd::Heal(relic.Owner().Creature(), relic.DynamicVars().Heal().BaseValue() * Dec(fatalDeaths));
}

void AfterFlush(RelicModel relic, PlayerChoiceContext&, Player player, const CardList&, const CardList& retained) {
    if (relic.RelicId_() != RelicId::BOOKMARK || player != relic.Owner()) return;
    CardList options;
    for (CardModel card : retained) {
        if (!card.EnergyCost().CostsX() && card.EnergyCost().GetWithModifiers(CostModifiers::Local) > 0)
            options.push_back(card);
    }
    if (options.empty()) return;
    CardModel selected = player.RunState().Rng().CombatCardSelection().NextItem(options);
    if (selected) selected.EnergyCost().AddUntilPlayed(-1);
}

void AfterSideTurnStart(RelicModel relic, CombatSide, const CreatureList& participants, CombatState combat) {
    Player owner = relic.Owner();
    if (relic.RelicId_() != RelicId::BIG_HAT || !contains(participants, owner.Creature()) ||
        owner.PlayerCombatState().TurnNumber() > 1) return;
    StaticVec<CardId, 640> options;
    for (CardId id : CardFactory::GetUnlockedCards(data(owner.Character()).card_pool, owner)) {
        CanonicalCard canonical(&owner.sim(), id);
        CardModel card = canonical.get();
        if (card.Keywords().Contains(CardKeyword::Ethereal) && card.Rarity() != CardRarity::Basic &&
            card.Rarity() != CardRarity::Ancient && card.Rarity() != CardRarity::Event && card.CanBeGeneratedInCombat())
            options.push_back(id);
    }
    if (options.empty()) return;
    owner.RunState().Rng().CombatCardGeneration().Shuffle(options);
    CardList generated;
    int32_t count = std::min(relic.DynamicVars().Cards().IntValue(), static_cast<int32_t>(options.size()));
    for (int32_t i = 0; i < count; ++i) generated.push_back(combat.CreateCard(options[static_cast<size_t>(i)], owner));
    if (!generated.empty()) CardPileCmd::AddGeneratedCardsToCombat(generated, PileType::Hand, owner);
}

void BeforeHandDraw(RelicModel relic, Player player, PlayerChoiceContext&, CombatState combat) {
    if (relic.RelicId_() != RelicId::FUNERARY_MASK || player != relic.Owner() ||
        player.PlayerCombatState().TurnNumber() != 1) return;
    int32_t count = relic.DynamicVars().Cards().IntValue();
    for (int32_t i = 0; i < count; ++i) {
        CardModel soul = combat.CreateCard(CardId::SOUL, player);
        CardPileCmd::AddGeneratedCardToCombat(soul, PileType::Draw, player, CardPilePosition::Random);
    }
}

}  // namespace sts2::NecrobinderRelicEffects
