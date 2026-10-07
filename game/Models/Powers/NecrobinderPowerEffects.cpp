#include "game/Models/Powers/NecrobinderPowers.h"

#include <algorithm>

#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/OstyCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Models/Cards/NecrobinderCards.h"
#include "game/Models/Canonical.h"
#include "game/Rewards/Reward.h"
#include "game/ValueProps/ValuePropExtensions.h"

namespace sts2::NecrobinderPowerEffects {
namespace {

bool contains(const CreatureList& participants, Creature creature) {
    return creature && std::find(participants.begin(), participants.end(), creature) != participants.end();
}

CreatureList hittable(PowerModel model) {
    return model.CombatState() ? model.CombatState().HittableEnemies() : CreatureList{};
}

void apply_doom(PowerModel model, Creature target, Dec amount, Creature applier) {
    if (target && amount != Dec(0)) {
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        PowerCmd::Apply(PowerId::DOOM_POWER, ctx, target, amount, applier, CardModel{});
    }
}

}  // namespace

bool TryModifyEnergyCostInCombat(PowerModel model, CardModel card, Dec originalCost, Dec& modifiedCost) {
    modifiedCost = originalCost;
    if (card.Owner().Creature() != model.Owner()) return false;
    modifiedCost = originalCost + Dec(model.Amount());
    return true;
}

bool TryModifyEnergyCostInCombatLate(PowerModel model, CardModel card, Dec originalCost, Dec& modifiedCost) {
    modifiedCost = originalCost;
    if (card.Owner().Creature() != model.Owner() || !card.Keywords().Contains(CardKeyword::Ethereal)) return false;
    CardPile pile = card.Pile();
    if (!pile || (pile.Type() != PileType::Hand && pile.Type() != PileType::Play)) return false;
    modifiedCost = Dec(0);
    return true;
}

Dec ModifyDamageAdditive(PowerModel model, Creature, Dec, ValueProp props, Creature dealer, CardModel, const CardPlay*) {
    if (!dealer || !dealer.IsPet() || dealer.Monster().MonsterId_() != MonsterId::OSTY) return Dec(0);
    if (model.Owner() != dealer.PetOwner().Creature() || !IsPoweredAttack(props)) return Dec(0);
    return Dec(model.Amount());
}

Dec ModifyDamageMultiplicative(PowerModel model, Creature target, Dec, ValueProp props, Creature dealer,
                               CardModel cardSource, const CardPlay*) {
    switch (model.PowerId_()) {
    case PowerId::HANG_POWER:
        if (target == model.Owner() && cardSource && cardSource.CardId_() == CardId::HANG)
            return Dec(model.Amount());
        return Dec(1);
    case PowerId::LETHALITY_POWER: {
        if (!IsPoweredAttack(props) || !cardSource || cardSource.Owner().Creature() != model.Owner()) return Dec(1);
        CardPile pile = cardSource.Pile();
        if (pile && pile.Type() == PileType::Play && cardSource.CurrentPlayIndex() > 0) return Dec(1);
        int32_t attacks = model.CombatState().st().player_attack_cards_started_this_turn;
        int32_t currentInPlay = pile && pile.Type() == PileType::Play ? 1 : 0;
        return attacks > currentInPlay ? Dec(1) : Dec(1) + Dec(model.Amount()) / Dec(100);
    }
    default:
        return Dec(1);
    }
}

Dec ModifyHandDraw(PowerModel model, Player player, Dec count) {
    if (model.PowerId_() == PowerId::DEMESNE_POWER && player == model.Owner().Player()) return count + Dec(model.Amount());
    return count;
}

Dec ModifyMaxEnergy(PowerModel model, Player player, Dec amount) {
    if (player != model.Owner().Player()) return amount;
    if (model.PowerId_() == PowerId::DEMESNE_POWER || model.PowerId_() == PowerId::FRIENDSHIP_POWER)
        return amount + Dec(model.Amount());
    return amount;
}

void AfterApplied(PowerModel, Creature, CardModel) {}

void AfterCardDrawn(PowerModel model, PlayerChoiceContext& ctx, CardModel card, bool) {
    if (model.PowerId_() == PowerId::CACOPHONY_POWER) {
        DynamicVar counter = model.DynamicVars()["Cards"];
        counter.SetBaseValue(counter.BaseValue() - Dec(1));
        if (counter.IntValue() <= 0) {
            counter.SetBaseValue(Dec(33));
            Creature enemy = model.Owner().Player().RunState().Rng().CombatTargets().NextItem(hittable(model));
            if (enemy) CreatureCmd::Damage(ctx, enemy, Dec(model.Amount()), ValueProp::Unpowered, model.Owner());
        }
        return;
    }
    if (model.PowerId_() == PowerId::PAGESTORM_POWER && card.Owner().Creature() == model.Owner() &&
        card.Keywords().Contains(CardKeyword::Ethereal)) {
        CardPileCmd::Draw(ctx, Dec(model.Amount()), model.Owner().Player());
    }
}

void AfterCardGeneratedForCombat(PowerModel model, CardModel card, Player creator) {
    if (model.PowerId_() != PowerId::SOULBOUND_POWER || !creator || creator.Creature() != model.Applier() ||
        card.CardId_() != CardId::SOUL || model.st().extra[0] != 0) return;
    model.st().extra[0] = 1;
    CardList souls;
    for (int32_t i = 0; i < model.Amount(); ++i) souls.push_back(model.CombatState().CreateCard(CardId::SOUL, model.Owner().Player()));
    if (!souls.empty()) CardPileCmd::AddGeneratedCardsToCombat(souls, PileType::Draw, model.Owner().Player(), CardPilePosition::Random);
    model.st().extra[0] = 0;
}

void AfterCardPlayed(PowerModel model, PlayerChoiceContext& ctx, const CardPlay* play) {
    if (!play) return;
    CardModel card = play->Card;
    switch (model.PowerId_()) {
    case PowerId::DEVOUR_LIFE_POWER:
        if (card.CardId_() == CardId::SOUL && card.Owner().Creature() == model.Owner())
            OstyCmd::Summon(ctx, card.Owner(), Dec(model.Amount()), model);
        break;
    case PowerId::HAUNT_POWER:
        if (card.CardId_() == CardId::SOUL && card.Owner().Creature() == model.Owner()) {
            Creature enemy = model.Owner().Player().RunState().Rng().CombatTargets().NextItem(hittable(model));
            if (enemy) CreatureCmd::Damage(ctx, enemy, Dec(model.Amount()), ValueProp::Unblockable | ValueProp::Unpowered, Creature{});
        }
        break;
    case PowerId::OBLIVION_POWER: {
        int32_t captured = 0;
        if (model.CombatState().TakeCardPlayPowerAmount(model, card, captured))
            PowerCmd::Apply(PowerId::DOOM_POWER, ctx, model.Owner(), Dec(captured), model.Applier(), CardModel{});
        break;
    }
    default: break;
    }
}

void AfterCombatEnd(PowerModel model, CombatRoom) {
    if (model.PowerId_() != PowerId::FORBIDDEN_GRIMOIRE_POWER) return;
    for (int32_t i = 0; i < model.Amount(); ++i)
        model.sim().g->run.combat_extra_rewards.push_back(Reward::NewCardRemovalReward(model.Owner().Player()).Index());
}

void AfterCurrentHpChanged(PowerModel model, Creature creature, Dec delta) {
    if (model.PowerId_() != PowerId::NECRO_MASTERY_POWER || delta >= Dec(0) || !creature.IsPet() ||
        creature.Monster().MonsterId_() != MonsterId::OSTY || creature.PetOwner().Creature() != model.Owner()) return;
    PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
    CreatureCmd::Damage(ctx, hittable(model), -delta * Dec(model.Amount()),
                        ValueProp::Unblockable | ValueProp::Unpowered, model.Owner());
}

void AfterDamageGiven(PowerModel model, PlayerChoiceContext& ctx, Creature dealer, const DamageResult& result,
                      ValueProp props, Creature target, CardModel) {
    switch (model.PowerId_()) {
    case PowerId::REAPER_FORM_POWER:
        if (dealer && (dealer == model.Owner() ||
                       (dealer.IsPet() && dealer.PetOwner() && dealer.PetOwner().Creature() == model.Owner())) &&
            IsPoweredAttack(props) && result.TotalDamage() > 0)
            PowerCmd::Apply(PowerId::DOOM_POWER, ctx, target, Dec(result.TotalDamage()) * Dec(model.Amount()), model.Owner(), CardModel{});
        break;
    case PowerId::SIC_EM_POWER:
        if (dealer && dealer.IsPet() && dealer.Monster().MonsterId_() == MonsterId::OSTY && dealer.PetOwner() &&
            dealer.PetOwner().Creature() == model.Applier() && target == model.Owner())
            OstyCmd::Summon(ctx, dealer.PetOwner(), Dec(model.Amount()), model);
        break;
    case PowerId::UNDERWORLD_POWER:
        if (dealer && dealer.Side() == model.Owner().Side() && dealer != model.Owner() &&
            dealer.PetOwner() != model.Owner().Player() && IsPoweredAttack(props) && result.TotalDamage() > 0)
            PowerCmd::Apply(PowerId::DOOM_POWER, ctx, target, Dec(result.TotalDamage()) * Dec(model.Amount()), model.Owner(), CardModel{});
        break;
    default: break;
    }
}

void AfterPlayerTurnStart(PowerModel model, PlayerChoiceContext& ctx, Player player) {
    if (model.PowerId_() == PowerId::SUMMON_NEXT_TURN_POWER && player == model.Owner().Player() &&
        model.AmountOnTurnStart() != 0) {
        OstyCmd::Summon(ctx, player, Dec(model.Amount()), model);
        PowerCmd::Remove(model);
    }
}

void AfterPowerAmountChanged(PowerModel model, PlayerChoiceContext& ctx, PowerModel changed, Dec amount,
                             Creature applier, CardModel) {
    if (model.PowerId_() == PowerId::SHROUD_POWER) {
        if (applier == model.Owner() && changed.PowerId_() == PowerId::DOOM_POWER)
            CreatureCmd::GainBlock(model.Owner(), Dec(model.Amount()), ValueProp::Unpowered, nullptr);
        return;
    }
    if (model.PowerId_() == PowerId::SLEIGHT_OF_FLESH_POWER && amount != Dec(0) &&
        changed.GetTypeForAmount(amount) == PowerType::Debuff && changed.Owner().IsEnemy() &&
        applier == model.Owner() && !changed.IsTemporaryPower())
        CreatureCmd::Damage(ctx, changed.Owner(), Dec(model.Amount()), ValueProp::Unpowered, model.Owner());
}

void AfterRemoved(PowerModel, Creature) {}

void AfterSideTurnEnd(PowerModel model, PlayerChoiceContext&, CombatSide side, const CreatureList& participants) {
    switch (model.PowerId_()) {
    case PowerId::BORROWED_TIME_POWER:
        if (contains(participants, model.Owner())) PowerCmd::Remove(model);
        break;
    case PowerId::OBLIVION_POWER:
        if (side == CombatSide::Player) {
            model.CombatState().ForgetCardPlayPowerCaptures(model);
            PowerCmd::Remove(model);
        }
        break;
    case PowerId::SIC_EM_POWER:
        if (contains(participants, model.Owner())) PowerCmd::Remove(model);
        break;
    case PowerId::UNDERWORLD_POWER:
        if (side == CombatSide::Enemy) PowerCmd::Remove(model);
        break;
    default: break;
    }
}

void AfterSideTurnStart(PowerModel model, CombatSide, const CreatureList& participants, CombatState) {
    if (model.PowerId_() == PowerId::COUNTDOWN_POWER && contains(participants, model.Owner())) {
        Creature enemy = model.Owner().Player().RunState().Rng().CombatTargets().NextItem(hittable(model));
        apply_doom(model, enemy, Dec(model.Amount()), model.Owner());
    } else if (model.PowerId_() == PowerId::NEUROSURGE_POWER && contains(participants, model.Owner())) {
        apply_doom(model, model.Owner(), Dec(model.Amount()), model.Owner());
    }
}

void BeforeCardPlayed(PowerModel model, const CardPlay* play) {
    if (!play) return;
    CardModel card = play->Card;
    switch (model.PowerId_()) {
    case PowerId::DANSE_MACABRE_POWER:
        if (card.Owner().Creature() == model.Owner() && card.EnergyCost().GetResolved() >= model.DynamicVars()["Energy"].IntValue())
            CreatureCmd::GainBlock(model.Owner(), Dec(model.Amount()), ValueProp::Unpowered, nullptr);
        break;
    case PowerId::OBLIVION_POWER:
        if (model.Applier().Player() && card.Owner() == model.Applier().Player())
            model.CombatState().CaptureCardPlayPowerAmount(model, card, model.Amount());
        break;
    case PowerId::SPIRIT_OF_ASH_POWER:
        if (card.Owner().Creature() == model.Owner() && card.Keywords().Contains(CardKeyword::Ethereal))
            CreatureCmd::GainBlock(model.Owner(), Dec(model.Amount()), ValueProp::Unpowered, nullptr);
        break;
    case PowerId::VEILPIERCER_POWER: {
        CardPile pile = card.Pile();
        if (card.Owner().Creature() == model.Owner() && card.Keywords().Contains(CardKeyword::Ethereal) && pile &&
            (pile.Type() == PileType::Hand || pile.Type() == PileType::Play)) PowerCmd::Decrement(model);
        break;
    }
    default: break;
    }
}

void BeforeHandDraw(PowerModel model, Player player, PlayerChoiceContext&, CombatState combatState) {
    if (player != model.Owner().Player()) return;
    if (model.PowerId_() == PowerId::SENTRY_MODE_POWER) {
        for (int32_t i = 0; i < model.Amount(); ++i) {
            CardModel gaze = combatState.CreateCard(CardId::SWEEPING_GAZE, player);
            CardPileCmd::AddGeneratedCardToCombat(gaze, PileType::Hand, player);
        }
        return;
    }
    if (model.PowerId_() == PowerId::CALL_OF_THE_VOID_POWER) {
        StaticVec<CardId, 640> options;
        CardPoolId pool = data(player.Character()).card_pool;
        for (CardId id : CardFactory::GetUnlockedCards(pool, player.st().profile.unlocked_epoch_bits)) {
            CanonicalCard canonical(&player.sim(), id);
            CardModel candidate = canonical.get();
            if (candidate.Rarity() != CardRarity::Basic && candidate.Rarity() != CardRarity::Ancient &&
                candidate.Rarity() != CardRarity::Event && candidate.CanBeGeneratedInCombat())
                options.push_back(id);
        }
        if (options.empty()) return;
        CardList generated;
        Rng& rng = model.Owner().Player().RunState().Rng().CombatCardGeneration();
        for (int32_t i = 0; i < model.Amount(); ++i) {
            StaticVec<CardId, 640> shuffled = options;
            rng.Shuffle(shuffled);
            CardModel generatedCard = combatState.CreateCard(shuffled[0], player);
            CardCmd::ApplyKeyword(generatedCard, {CardKeyword::Ethereal});
            generated.push_back(generatedCard);
        }
        if (!generated.empty()) CardPileCmd::AddGeneratedCardsToCombat(generated, PileType::Hand, player);
    }
}

}  // namespace sts2::NecrobinderPowerEffects
