#include <cstdint>
#include <span>

#include "engine/run_setup.h"
#include "game/Commands/OrbCmd.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/PotionCmd.h"
#include "game/Factories/PotionFactory.h"
#include "game/Models/Cards/EchoForm.h"
#include "game/Models/Cards/ImitationLearning.h"
#include "game/Models/Cards/Ignition.h"
#include "game/Models/Powers/ImitationLearningPower.h"
#include "game/Hooks/Hook.h"
#include "game/Runs/RunFlow.h"
#include "game/Runs/RunManager.h"

using namespace sts2;

namespace {

struct Fixture {
    GameState state;
    Sim sim;
    Player player;
    Creature enemy;

    explicit Fixture(PotionId potion, CharacterId character = CharacterId::IRONCLAD) : sim(state), player(&sim, 1) {
        new_run(state, NewRunOptions{.seed = "POTIONBATCH", .character = character, .ascension = 0});
        RunManager::SetActInternal(&sim, 0);
        if (!PotionCmd::TryToProcure(potion, player).success) throw GameError("potion could not be procured");
        RunManager::EnterMapCoordDebug(&sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        enemy = CombatState(&sim).Enemies().front();
    }
};

bool use(PotionId id, Creature target, Fixture& f) {
    PotionModel potion = f.player.Potions().front();
    if (potion.PotionId_() != id) return false;
    PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
    potion.OnUseWrapper(ctx, target);
    return f.player.Potions().empty();
}

bool use_choice(PotionId id, Creature target, Fixture& f) {
    PotionModel potion = f.player.Potions().front();
    if (potion.PotionId_() != id) return false;
    const int32_t answers[] = {1, 0};  // choose one offered card, then its first index
    f.sim.set_prefix(std::span<const int32_t>(answers));
    PlayerChoiceContext ctx;
    potion.OnUseWrapper(ctx, target);
    return f.sim.prefix_consumed() && f.player.Potions().empty();
}

bool use_all_choices(PotionId id, Creature target, Fixture& f, int32_t count) {
    PotionModel potion = f.player.Potions().front();
    if (potion.PotionId_() != id) return false;
    int32_t answers[16]{};
    answers[0] = count;
    for (int32_t i = 0; i < count; ++i) answers[i + 1] = i;
    f.sim.set_prefix(std::span<const int32_t>(answers, static_cast<size_t>(count + 1)));
    PlayerChoiceContext ctx;
    potion.OnUseWrapper(ctx, target);
    return f.sim.prefix_consumed() && f.player.Potions().empty();
}

bool contains(const StaticVec<PotionId, 96>& options, PotionId potion) {
    return options.contains(potion);
}

bool test_epoch_pool_coverage() {
    struct PoolCase {
        CharacterId character;
        uint32_t bit;
        PotionId potions[3];
    };
    const PoolCase pools[] = {
        {CharacterId::IRONCLAD, 22, {PotionId::BLOOD_POTION, PotionId::SOLDIERS_STEW, PotionId::ASHWATER}},
        {CharacterId::DEFECT, 13, {PotionId::FOCUS_POTION, PotionId::ESSENCE_OF_DARKNESS, PotionId::POTION_OF_CAPACITY}},
        {CharacterId::REGENT, 40, {PotionId::STAR_POTION, PotionId::COSMIC_CONCOCTION, PotionId::KINGS_COURAGE}},
        {CharacterId::NECROBINDER, 29, {PotionId::POTION_OF_DOOM, PotionId::POT_OF_GHOULS, PotionId::BONE_BREW}},
        {CharacterId::SILENT, 52, {PotionId::POISON_POTION, PotionId::GHOST_IN_A_JAR, PotionId::CUNNING_POTION}},
    };
    for (const PoolCase& pool : pools) {
        GameState state;
        Sim sim(state);
        new_run(state, NewRunOptions{.seed = "EPOCHPOOL1", .character = pool.character, .ascension = 0});
        Player player(&sim, 1);
        player.st().profile.unlocked_epoch_bits = 0;
        StaticVec<PotionId, 96> options = PotionFactory::GetPotionOptions(player);
        for (PotionId potion : pool.potions)
            if (contains(options, potion)) return false;
        if (!contains(options, PotionId::BLOCK_POTION) || contains(options, PotionId::AMBERGRIS) ||
            contains(options, PotionId::DEPRECATED_POTION) || contains(options, PotionId::POTION_SHAPED_ROCK)) return false;

        player.st().profile.unlocked_epoch_bits = uint64_t{1} << pool.bit;
        options = PotionFactory::GetPotionOptions(player);
        for (PotionId potion : pool.potions)
            if (!contains(options, potion)) return false;
    }

    GameState state;
    Sim sim(state);
    new_run(state, NewRunOptions{.seed = "EPOCHPOOL2", .character = CharacterId::IRONCLAD,
        .ascension = 0});
    Player player(&sim, 1);
    player.st().profile.unlocked_epoch_bits = 0;
    StaticVec<PotionId, 96> options = PotionFactory::GetPotionOptions(player);
    const PotionId potion1[] = {PotionId::BEETLE_JUICE, PotionId::DROPLET_OF_PRECOGNITION, PotionId::MAZALETHS_GIFT};
    const PotionId potion2[] = {PotionId::POWDERED_DEMISE, PotionId::SHIP_IN_A_BOTTLE, PotionId::TOUCH_OF_INSANITY};
    for (PotionId potion : potion1) if (contains(options, potion)) return false;
    for (PotionId potion : potion2) if (contains(options, potion)) return false;
    player.st().profile.unlocked_epoch_bits = uint64_t{1} << 35;
    options = PotionFactory::GetPotionOptions(player);
    for (PotionId potion : potion1) if (!contains(options, potion)) return false;
    for (PotionId potion : potion2) if (contains(options, potion)) return false;
    player.st().profile.unlocked_epoch_bits |= uint64_t{1} << 36;
    options = PotionFactory::GetPotionOptions(player);
    for (PotionId potion : potion2) if (!contains(options, potion)) return false;
    return true;
}

}  // namespace

int main() {
    if (!test_epoch_pool_coverage()) return 106;
    {
        Fixture f(PotionId::BLOCK_POTION);
        int before = f.player.Creature().Block();
        if (!use(PotionId::BLOCK_POTION, f.player.Creature(), f) || f.player.Creature().Block() != before + 12) return 1;
    }
    {
        Fixture f(PotionId::ENERGY_POTION);
        int before = f.player.PlayerCombatState().Energy();
        if (!use(PotionId::ENERGY_POTION, f.player.Creature(), f) || f.player.PlayerCombatState().Energy() != before + 2) return 2;
    }
    {
        Fixture f(PotionId::FIRE_POTION);
        int before = f.enemy.CurrentHp();
        if (!use(PotionId::FIRE_POTION, f.enemy, f) || f.enemy.CurrentHp() >= before) return 3;
    }
    {
        Fixture f(PotionId::STRENGTH_POTION);
        if (!use(PotionId::STRENGTH_POTION, f.player.Creature(), f) || f.player.Creature().GetPowerAmount(PowerId::STRENGTH_POWER) != 2) return 4;
    }
    {
        Fixture f(PotionId::DEXTERITY_POTION);
        if (!use(PotionId::DEXTERITY_POTION, f.player.Creature(), f) || f.player.Creature().GetPowerAmount(PowerId::DEXTERITY_POWER) != 2) return 5;
    }
    {
        Fixture f(PotionId::VULNERABLE_POTION);
        if (!use(PotionId::VULNERABLE_POTION, f.enemy, f) || f.enemy.GetPowerAmount(PowerId::VULNERABLE_POWER) != 3) return 6;
    }
    {
        Fixture f(PotionId::WEAK_POTION);
        if (!use(PotionId::WEAK_POTION, f.enemy, f) || f.enemy.GetPowerAmount(PowerId::WEAK_POWER) != 3) return 7;
    }
    {
        Fixture f(PotionId::POISON_POTION);
        if (!use(PotionId::POISON_POTION, f.enemy, f) || f.enemy.GetPowerAmount(PowerId::POISON_POWER) != 6) return 8;
    }
    {
        Fixture f(PotionId::SWIFT_POTION);
        int before = f.player.PlayerCombatState().Hand().Count();
        if (!use(PotionId::SWIFT_POTION, f.player.Creature(), f) || f.player.PlayerCombatState().Hand().Count() != before + 3) return 9;
    }
    {
        Fixture f(PotionId::FOCUS_POTION, CharacterId::DEFECT);
        if (!use(PotionId::FOCUS_POTION, f.player.Creature(), f) || f.player.Creature().GetPowerAmount(PowerId::FOCUS_POWER) != 2) return 10;
    }
    {
        Fixture f(PotionId::LIQUID_BRONZE);
        if (!use(PotionId::LIQUID_BRONZE, f.player.Creature(), f) || f.player.Creature().GetPowerAmount(PowerId::THORNS_POWER) != 3) return 11;
    }
    {
        Fixture f(PotionId::HEART_OF_IRON);
        if (!use(PotionId::HEART_OF_IRON, f.player.Creature(), f) || f.player.Creature().GetPowerAmount(PowerId::PLATING_POWER) != 7) return 12;
    }
    {
        Fixture f(PotionId::BLOOD_POTION);
        Creature player = f.player.Creature();
        player.SetCurrentHpInternal(player.MaxHp() - 20);
        if (!use(PotionId::BLOOD_POTION, player, f) || player.CurrentHp() <= player.MaxHp() - 20) return 13;
    }
    {
        Fixture f(PotionId::FRUIT_JUICE);
        int before = f.player.Creature().MaxHp();
        if (!use(PotionId::FRUIT_JUICE, f.player.Creature(), f) || f.player.Creature().MaxHp() != before + 5) return 14;
    }
    {
        Fixture f(PotionId::BEETLE_JUICE);
        if (!use(PotionId::BEETLE_JUICE, f.enemy, f) || f.enemy.GetPowerAmount(PowerId::SHRINK_POWER) != 4) return 15;
    }
    {
        Fixture f(PotionId::FORTIFIER);
        f.player.Creature().GainBlockInternal(5);
        if (!use(PotionId::FORTIFIER, f.player.Creature(), f) || f.player.Creature().Block() != 15) return 16;
    }
    {
        Fixture f(PotionId::FYSH_OIL);
        if (!use(PotionId::FYSH_OIL, f.player.Creature(), f) ||
            f.player.Creature().GetPowerAmount(PowerId::STRENGTH_POWER) != 1 ||
            f.player.Creature().GetPowerAmount(PowerId::DEXTERITY_POWER) != 1) return 17;
    }
    {
        Fixture f(PotionId::MAZALETHS_GIFT);
        if (!use(PotionId::MAZALETHS_GIFT, f.player.Creature(), f) || f.player.Creature().GetPowerAmount(PowerId::RITUAL_POWER) != 1) return 18;
    }
    {
        Fixture f(PotionId::POTION_OF_BINDING);
        if (!use(PotionId::POTION_OF_BINDING, f.enemy, f) ||
            f.enemy.GetPowerAmount(PowerId::WEAK_POWER) != 1 ||
            f.enemy.GetPowerAmount(PowerId::VULNERABLE_POWER) != 1) return 19;
    }
    {
        Fixture f(PotionId::POTION_SHAPED_ROCK);
        int before = f.enemy.CurrentHp();
        if (!use(PotionId::POTION_SHAPED_ROCK, f.enemy, f) || f.enemy.CurrentHp() >= before) return 20;
    }
    {
        Fixture f(PotionId::SHIP_IN_A_BOTTLE);
        if (!use(PotionId::SHIP_IN_A_BOTTLE, f.player.Creature(), f) ||
            f.player.Creature().Block() != 10 ||
            f.player.Creature().GetPowerAmount(PowerId::BLOCK_NEXT_TURN_POWER) != 10) return 21;
    }
    {
        Fixture f(PotionId::STAR_POTION);
        int before = f.player.PlayerCombatState().Stars();
        if (!use(PotionId::STAR_POTION, f.player.Creature(), f) || f.player.PlayerCombatState().Stars() != before + 3) return 22;
    }
    {
        Fixture f(PotionId::EXPLOSIVE_AMPOULE);
        int before = f.enemy.CurrentHp();
        if (!use(PotionId::EXPLOSIVE_AMPOULE, Creature{}, f) || f.enemy.CurrentHp() >= before) return 23;
    }
    {
        Fixture f(PotionId::FLEX_POTION);
        if (!use(PotionId::FLEX_POTION, f.player.Creature(), f) ||
            f.player.Creature().GetPowerAmount(PowerId::FLEX_POTION_POWER) != 5 ||
            f.player.Creature().GetPowerAmount(PowerId::STRENGTH_POWER) != 5) return 24;
        PlayerChoiceContext ctx;
        f.player.Creature().GetPower(PowerId::FLEX_POTION_POWER).AfterSideTurnEnd(ctx, CombatSide::Player, CreatureList{f.player.Creature()});
        if (f.player.Creature().GetPower(PowerId::FLEX_POTION_POWER) || f.player.Creature().GetPowerAmount(PowerId::STRENGTH_POWER) != 0) return 25;
    }
    {
        Fixture f(PotionId::SPEED_POTION);
        if (!use(PotionId::SPEED_POTION, f.player.Creature(), f) ||
            f.player.Creature().GetPowerAmount(PowerId::SPEED_POTION_POWER) != 5 ||
            f.player.Creature().GetPowerAmount(PowerId::DEXTERITY_POWER) != 5) return 26;
    }
    {
        Fixture f(PotionId::REGEN_POTION);
        Creature player = f.player.Creature();
        player.SetCurrentHpInternal(player.MaxHp() - 20);
        if (!use(PotionId::REGEN_POTION, player, f)) return 27;
        PlayerChoiceContext ctx;
        player.GetPower(PowerId::REGEN_POWER).BeforeSideTurnEndEarly(ctx, CombatSide::Player, CreatureList{player});
        if (player.CurrentHp() != player.MaxHp() - 15 || player.GetPowerAmount(PowerId::REGEN_POWER) != 4) return 28;
    }
    {
        Fixture f(PotionId::SHACKLING_POTION);
        if (!use(PotionId::SHACKLING_POTION, Creature{}, f) ||
            f.enemy.GetPowerAmount(PowerId::SHACKLING_POTION_POWER) != 7 ||
            f.enemy.GetPowerAmount(PowerId::STRENGTH_POWER) != -7) return 29;
    }
    {
        Fixture f(PotionId::GIGANTIFICATION_POTION);
        if (!use(PotionId::GIGANTIFICATION_POTION, f.player.Creature(), f) ||
            f.player.Creature().GetPowerAmount(PowerId::GIGANTIFICATION_POWER) != 1) return 30;
    }
    {
        Fixture f(PotionId::ATTACK_POTION);
        int before = f.player.PlayerCombatState().Hand().Count();
        if (!use_choice(PotionId::ATTACK_POTION, f.player.Creature(), f) ||
            f.player.PlayerCombatState().Hand().Count() != before + 1) return 31;
    }
    {
        Fixture f(PotionId::POWER_POTION);
        int before = f.player.PlayerCombatState().Hand().Count();
        if (!use_choice(PotionId::POWER_POTION, f.player.Creature(), f) ||
            f.player.PlayerCombatState().Hand().Count() != before + 1) return 32;
    }
    {
        Fixture f(PotionId::SKILL_POTION);
        int before = f.player.PlayerCombatState().Hand().Count();
        if (!use_choice(PotionId::SKILL_POTION, f.player.Creature(), f) ||
            f.player.PlayerCombatState().Hand().Count() != before + 1) return 33;
    }
    {
        Fixture f(PotionId::BLESSING_OF_THE_FORGE);
        if (!use(PotionId::BLESSING_OF_THE_FORGE, f.player.Creature(), f)) return 34;
        for (CardModel card : f.player.PlayerCombatState().Hand().Cards()) {
            if (card.IsUpgradable() && card.CurrentUpgradeLevel() == 0) return 35;
        }
    }
    {
        Fixture f(PotionId::CURE_ALL);
        int energy = f.player.PlayerCombatState().Energy();
        int hand = f.player.PlayerCombatState().Hand().Count();
        if (!use(PotionId::CURE_ALL, f.player.Creature(), f) ||
            f.player.PlayerCombatState().Energy() != energy + 1 ||
            f.player.PlayerCombatState().Hand().Count() != hand + 2) return 36;
    }
    {
        Fixture f(PotionId::LUCKY_TONIC);
        if (!use(PotionId::LUCKY_TONIC, f.player.Creature(), f) ||
            f.player.Creature().GetPowerAmount(PowerId::BUFFER_POWER) != 1) return 37;
    }
    {
        Fixture f(PotionId::STABLE_SERUM);
        if (!use(PotionId::STABLE_SERUM, f.player.Creature(), f) ||
            f.player.Creature().GetPowerAmount(PowerId::RETAIN_HAND_POWER) != 2) return 38;
        PlayerChoiceContext ctx;
        f.player.Creature().GetPower(PowerId::RETAIN_HAND_POWER).AfterSideTurnEnd(ctx, CombatSide::Player, CreatureList{f.player.Creature()});
        if (f.player.Creature().GetPowerAmount(PowerId::RETAIN_HAND_POWER) != 1) return 39;
    }
    {
        Fixture f(PotionId::POWDERED_DEMISE);
        int before = f.enemy.CurrentHp();
        if (!use(PotionId::POWDERED_DEMISE, f.enemy, f) ||
            f.enemy.GetPowerAmount(PowerId::DEMISE_POWER) != 9) return 40;
        PlayerChoiceContext ctx;
        f.enemy.GetPower(PowerId::DEMISE_POWER).AfterSideTurnEnd(ctx, CombatSide::Enemy, CreatureList{f.enemy});
        if (f.enemy.CurrentHp() != before - 9) return 41;
    }
    {
        Fixture f(PotionId::DUPLICATOR);
        if (!use(PotionId::DUPLICATOR, f.player.Creature(), f) ||
            f.player.Creature().GetPowerAmount(PowerId::DUPLICATION_POWER) != 1) return 42;
        CardModel card = f.player.PlayerCombatState().Hand().Cards().front();
        PowerModel power = f.player.Creature().GetPower(PowerId::DUPLICATION_POWER);
        if (power.ModifyCardPlayCount(card, f.player.Creature(), 1) != 2) return 43;
        power.AfterModifyingCardPlayCount(card);
        if (f.player.Creature().GetPowerAmount(PowerId::DUPLICATION_POWER) != 0) return 44;
    }
    {
        Fixture f(PotionId::ASHWATER);
        const int hand = f.player.PlayerCombatState().Hand().Count();
        if (!use_all_choices(PotionId::ASHWATER, f.player.Creature(), f, hand) ||
            f.player.PlayerCombatState().Hand().Count() != 0 ||
            f.player.PlayerCombatState().ExhaustPile().Count() != hand) return 45;
    }
    {
        Fixture f(PotionId::BOTTLED_POTENTIAL);
        if (!use(PotionId::BOTTLED_POTENTIAL, f.player.Creature(), f) || f.player.PlayerCombatState().Hand().Count() != 5) return 46;
    }
    {
        Fixture f(PotionId::DISTILLED_CHAOS);
        if (!use(PotionId::DISTILLED_CHAOS, f.player.Creature(), f)) return 47;
    }
    {
        Fixture f(PotionId::GAMBLERS_BREW);
        if (!use_choice(PotionId::GAMBLERS_BREW, f.player.Creature(), f)) return 48;
    }
    {
        Fixture f(PotionId::CLARITY);
        if (!use(PotionId::CLARITY, f.player.Creature(), f) ||
            f.player.Creature().GetPowerAmount(PowerId::CLARITY_POWER) != 3) return 49;
    }
    {
        Fixture f(PotionId::RADIANT_TINCTURE);
        int before = f.player.PlayerCombatState().Energy();
        if (!use(PotionId::RADIANT_TINCTURE, f.player.Creature(), f) ||
            f.player.PlayerCombatState().Energy() != before + 1 ||
            f.player.Creature().GetPowerAmount(PowerId::RADIANCE_POWER) != 3) return 50;
    }
    {
        Fixture f(PotionId::DROPLET_OF_PRECOGNITION);
        if (!use_choice(PotionId::DROPLET_OF_PRECOGNITION, f.player.Creature(), f)) return 51;
    }
    {
        Fixture f(PotionId::LIQUID_MEMORIES);
        PlayerChoiceContext setup = PlayerChoiceContext::Throwing();
        CardCmd::Discard(setup, f.player.PlayerCombatState().Hand().Cards().front());
        PotionModel potion = f.player.Potions().front();
        const int32_t answers[] = {1, 0};
        f.sim.set_prefix(std::span<const int32_t>(answers));
        PlayerChoiceContext ctx;
        potion.OnUseWrapper(ctx, f.player.Creature());
        if (!f.player.Potions().empty()) return 52;
    }
    {
        Fixture f(PotionId::ENTROPIC_BREW);
        PotionModel potion = f.player.Potions().front();
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        potion.OnUseWrapper(ctx, f.player.Creature());
        if (f.player.Potions().empty()) return 53;
    }
    {
        Fixture f(PotionId::SNECKO_OIL);
        if (!use(PotionId::SNECKO_OIL, f.player.Creature(), f)) return 54;
    }
    {
        Fixture f(PotionId::TOUCH_OF_INSANITY);
        if (!use_choice(PotionId::TOUCH_OF_INSANITY, f.player.Creature(), f)) return 55;
    }
    {
        Fixture f(PotionId::OROBIC_ACID);
        int before = f.player.PlayerCombatState().Hand().Count();
        if (!use(PotionId::OROBIC_ACID, f.player.Creature(), f) ||
            f.player.PlayerCombatState().Hand().Count() != before + 3) return 56;
    }
    {
        Fixture f(PotionId::COLORLESS_POTION);
        CardList before = f.player.PlayerCombatState().Hand().Cards();
        if (!use_choice(PotionId::COLORLESS_POTION, f.player.Creature(), f)) return 57;
        CardModel generated;
        for (CardModel card : f.player.PlayerCombatState().Hand().Cards()) {
            if (!before.contains(card)) generated = card;
        }
        if (!generated || generated.Pool() != CardPoolId::COLORLESS_CARD_POOL ||
            generated.EnergyCost().GetWithModifiers(CostModifiers::All) != 0) return 58;
    }
    {
        Fixture f(PotionId::SOLDIERS_STEW);
        CardList cards = f.player.PlayerCombatState().AllCards();
        if (!use(PotionId::SOLDIERS_STEW, f.player.Creature(), f)) return 59;
        int strikes = 0;
        for (CardModel card : cards) {
            if (card.HasTag(CardTag::Strike)) {
                ++strikes;
                if (card.BaseReplayCount() != 1) return 60;
            } else if (card.BaseReplayCount() != 0) return 61;
        }
        if (strikes != 5) return 62;
    }
    {
        Fixture f(PotionId::GLOWWATER_POTION);
        CardList before = f.player.PlayerCombatState().Hand().Cards();
        if (!use(PotionId::GLOWWATER_POTION, f.player.Creature(), f)) return 63;
        if (f.player.PlayerCombatState().ExhaustPile().Count() != static_cast<int32_t>(before.size())) return 64;
        for (CardModel card : before)
            if (!f.player.PlayerCombatState().ExhaustPile().Cards().contains(card)) return 65;
        if (f.player.PlayerCombatState().Hand().Count() == 0) return 66;
    }
    {
        Fixture f(PotionId::AMBERGRIS);
        Creature player = f.player.Creature();
        player.SetCurrentHpInternal(30);
        if (!use(PotionId::AMBERGRIS, player, f) || player.CurrentHp() != 70) return 67;
        PowerModel power = player.GetPower(PowerId::AMBERGRIS_POWER);
        CombatState combat(&f.sim);
        if (!power || !Hook::ShouldTakeExtraTurn(combat, f.player)) return 68;
        Hook::AfterTakingExtraTurn(combat, f.player);
        if (player.GetPowerAmount(PowerId::AMBERGRIS_POWER) != 0) return 69;
    }
    {
        GameState state;
        Sim sim(state);
        new_run(state, NewRunOptions{.seed = "AMBERGRIS_OUT_OF_COMBAT", .character = CharacterId::IRONCLAD, .ascension = 0});
        Player player(&sim, 1);
        RunManager::SetActInternal(&sim, 0);
        if (!PotionCmd::TryToProcure(PotionId::AMBERGRIS, player).success) return 70;
        player.Creature().SetCurrentHpInternal(30);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        player.Potions().front().OnUseWrapper(ctx, player.Creature());
        if (player.Creature().CurrentHp() != 70 || !player.Potions().empty() ||
            player.Creature().GetPowerAmount(PowerId::AMBERGRIS_POWER) != 0) return 71;
    }
    {
        Fixture f(PotionId::FOUL_POTION);
        Creature owner = f.player.Creature();
        int32_t ownerHp = owner.CurrentHp();
        int32_t enemyHp = f.enemy.CurrentHp();
        if (!f.player.Potions().front().PassesCustomUsabilityCheck() ||
            !use(PotionId::FOUL_POTION, Creature{}, f)) return 72;
        if (owner.CurrentHp() != ownerHp - 12 || f.enemy.CurrentHp() != enemyHp - 12) return 73;
    }
    {
        GameState state;
        Sim sim(state);
        new_run(state, NewRunOptions{.seed = "FOUL_SHOP", .character = CharacterId::IRONCLAD, .ascension = 0});
        Player player(&sim, 1);
        RunManager::SetActInternal(&sim, 0);
        if (!PotionCmd::TryToProcure(PotionId::FOUL_POTION, player).success) return 74;
        if (player.Potions().front().PassesCustomUsabilityCheck()) return 75;
        bool rejected = false;
        try { RunFlow::UsePotion(&sim, player.GetPotionSlotIndex(player.Potions().front()), -1); }
        catch (const GameError&) { rejected = true; }
        if (!rejected || player.Potions().empty()) return 84;
        MapCoord shop{};
        bool found = false;
        for (const MapPoint& point : state.run.map.points)
            if (point.type == MapPointType::Shop) { shop = point.coord; found = true; break; }
        if (!found) return 76;
        RunManager::EnterMapCoord(&sim, shop);
        if (!player.Potions().front().PassesCustomUsabilityCheck()) return 77;
        int32_t before = player.Gold();
        RunFlow::UsePotion(&sim, player.GetPotionSlotIndex(player.Potions().front()), -1);
        if (player.Gold() != before + 100 || !player.Potions().empty()) return 78;
    }
    {
        GameState state;
        Sim sim(state);
        new_run(state, NewRunOptions{.seed = "FOUL_EVENT_ALLOWED", .character = CharacterId::IRONCLAD, .ascension = 0});
        Player player(&sim, 1);
        RunManager::SetActInternal(&sim, 1);
        player.SetGold(0);
        EventModel event = EventModel::Canonical(&sim, EventId::FAKE_MERCHANT);
        if (event.IsAllowed(RunState(&sim))) return 79;
        if (!PotionCmd::TryToProcure(PotionId::BLOOD_POTION, player).success) return 80;
        if (event.IsAllowed(RunState(&sim))) return 81;
        if (!PotionCmd::TryToProcure(PotionId::FOUL_POTION, player).success) return 82;
        if (!event.IsAllowed(RunState(&sim))) return 83;
    }
    {
        Fixture f(PotionId::ESSENCE_OF_DARKNESS, CharacterId::DEFECT);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        const int32_t capacity = f.player.PlayerCombatState().st().orb_capacity;
        f.player.Potions().front().OnUseWrapper(ctx, f.player.Creature());
        const auto& orbs = f.player.PlayerCombatState().st().orbs;
        if (capacity != 3 || !f.player.Potions().empty() || orbs.size() != static_cast<size_t>(capacity)) return 85;
        for (OrbRef ref : orbs) {
            OrbModel orb(&f.sim, ref);
            if (orb.OrbId_() != OrbId::DARK_ORB || orb.EvokeVal() != Dec(6)) return 86;
        }
    }
    {
        Fixture f(PotionId::POTION_OF_CAPACITY, CharacterId::DEFECT);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        const int32_t capacity = f.player.PlayerCombatState().st().orb_capacity;
        f.player.Potions().front().OnUseWrapper(ctx, f.player.Creature());
        if (!f.player.Potions().empty() || f.player.PlayerCombatState().st().orb_capacity != capacity + 2) return 87;
    }
    {
        Fixture f(PotionId::POTION_OF_CAPACITY, CharacterId::DEFECT);
        PlayerCombatData& pcs = f.player.PlayerCombatState().st();
        for (OrbRef ref : pcs.orbs) OrbModel(&f.sim, ref).RemoveInternal();
        pcs.orbs.clear();
        pcs.orb_capacity = 3;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();

        OrbCmd::Channel(OrbId::LIGHTNING_ORB, ctx, f.player);
        OrbRef lightning = pcs.orbs.back();
        OrbCmd::Channel(OrbId::FROST_ORB, ctx, f.player);
        OrbCmd::Channel(OrbId::PLASMA_ORB, ctx, f.player);
        OrbCmd::Channel(OrbId::DARK_ORB, ctx, f.player);  // full queue evokes the oldest orb
        if (pcs.orbs.size() != 3 || !OrbModel(&f.sim, lightning).HasBeenRemovedFromState() ||
            OrbModel(&f.sim, pcs.orbs[0]).OrbId_() != OrbId::FROST_ORB ||
            OrbModel(&f.sim, pcs.orbs[1]).OrbId_() != OrbId::PLASMA_ORB ||
            OrbModel(&f.sim, pcs.orbs[2]).OrbId_() != OrbId::DARK_ORB) return 88;

        OrbCmd::AddSlots(f.player, 20);
        if (pcs.orb_capacity != 10) return 89;
        OrbCmd::RemoveSlots(f.player, 8);  // reducing capacity discards from the queue's back
        if (pcs.orb_capacity != 2 || pcs.orbs.size() != 2 ||
            OrbModel(&f.sim, pcs.orbs.back()).OrbId_() != OrbId::PLASMA_ORB) return 90;

        int energy = f.player.PlayerCombatState().Energy();
        OrbRef plasma = pcs.orbs.back();
        OrbCmd::EvokeLast(ctx, f.player);
        if (f.player.PlayerCombatState().Energy() != energy + 2 || !OrbModel(&f.sim, plasma).HasBeenRemovedFromState() ||
            pcs.orbs.size() != 1) return 91;
        int block = f.player.Creature().Block();
        OrbCmd::BeforeTurnEnd(ctx, f.player);
        if (f.player.Creature().Block() != block + 2) return 92;  // Frost passive
    }
    {
        Fixture f(PotionId::POTION_OF_CAPACITY, CharacterId::DEFECT);
        PlayerCombatData& pcs = f.player.PlayerCombatState().st();
        for (OrbRef ref : pcs.orbs) OrbModel(&f.sim, ref).RemoveInternal();
        pcs.orbs.clear();
        pcs.orb_capacity = 3;
        f.player.PlayerCombatState().SetEnergy(0);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        OrbCmd::Channel(OrbId::PLASMA_ORB, ctx, f.player);
        OrbRef plasma = pcs.orbs.front();
        OrbCmd::AfterTurnStart(ctx, f.player);
        if (f.player.PlayerCombatState().Energy() != 1 || OrbModel(&f.sim, plasma).HasBeenRemovedFromState()) return 93;
        OrbCmd::EvokeNext(ctx, f.player);
        if (f.player.PlayerCombatState().Energy() != 3 || !pcs.orbs.empty()) return 94;
    }
    {
        Fixture f(PotionId::POTION_OF_CAPACITY, CharacterId::DEFECT);
        PlayerCombatData& pcs = f.player.PlayerCombatState().st();
        for (OrbRef ref : pcs.orbs) OrbModel(&f.sim, ref).RemoveInternal();
        pcs.orbs.clear();
        pcs.orb_capacity = 3;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        OrbCmd::Channel(OrbId::DARK_ORB, ctx, f.player);
        OrbRef dark = pcs.orbs.front();
        OrbCmd::BeforeTurnEnd(ctx, f.player);
        if (OrbModel(&f.sim, dark).EvokeVal() != Dec(12)) return 95;
        OrbCmd::EvokeNext(ctx, f.player);
        if (!OrbModel(&f.sim, dark).HasBeenRemovedFromState() || !pcs.orbs.empty()) return 96;

        OrbCmd::Channel(OrbId::GLASS_ORB, ctx, f.player);
        OrbRef glass = pcs.orbs.front();
        OrbCmd::BeforeTurnEnd(ctx, f.player);
        if (OrbModel(&f.sim, glass).PassiveVal() != Dec(3) || OrbModel(&f.sim, glass).EvokeVal() != Dec(6)) return 97;
        OrbCmd::EvokeNext(ctx, f.player);
        if (!OrbModel(&f.sim, glass).HasBeenRemovedFromState()) return 98;
    }
    {
        Fixture f(PotionId::POTION_OF_CAPACITY, CharacterId::DEFECT);
        PlayerCombatData& pcs = f.player.PlayerCombatState().st();
        for (OrbRef ref : pcs.orbs) OrbModel(&f.sim, ref).RemoveInternal();
        pcs.orbs.clear();
        pcs.orb_capacity = 0;
        f.player.st().base_orb_slots = 0;
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        OrbCmd::Channel(OrbId::PLASMA_ORB, ctx, f.player);
        if (pcs.orb_capacity != 1 || pcs.orbs.size() != 1 ||
            OrbModel(&f.sim, pcs.orbs.front()).OrbId_() != OrbId::PLASMA_ORB) return 99;
    }
    {
        Fixture f(PotionId::POTION_OF_CAPACITY, CharacterId::DEFECT);
        PlayerCombatData& pcs = f.player.PlayerCombatState().st();
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        const int32_t capacity = pcs.orb_capacity;
        const size_t queued = pcs.orbs.size();
        const uint16_t allocated = f.state.orbs.size();
        const int32_t hp = f.player.Creature().CurrentHp();
        const int32_t energy = f.player.PlayerCombatState().Energy();
        const int32_t block = f.player.Creature().Block();
        if (pcs.orbs.empty()) return 100;
        f.state.combat.is_in_progress = false;
        OrbCmd::AddSlots(f.player, 2);
        OrbCmd::RemoveSlots(f.player, 1);
        OrbCmd::Channel(OrbId::FROST_ORB, ctx, f.player);
        OrbCmd::EvokeNext(ctx, f.player);
        OrbCmd::EvokeLast(ctx, f.player);
        OrbCmd::Passive(ctx, OrbModel(&f.sim, pcs.orbs.front()), f.enemy, false);
        if (pcs.orb_capacity != capacity || pcs.orbs.size() != queued ||
            f.state.orbs.size() != allocated || f.player.Creature().CurrentHp() != hp ||
            f.player.PlayerCombatState().Energy() != energy || f.player.Creature().Block() != block) return 100;
    }
    {
        Fixture f(PotionId::POTION_OF_CAPACITY, CharacterId::DEFECT);
        CombatState combat(&f.sim);
        PlayerChoiceContext ctx = PlayerChoiceContext::Throwing();
        CardModel ignition = combat.CreateCard(CardId::IGNITION, f.player);
        CardPlay ignitionPlay{.Card = ignition, .Player = f.player, .Target = f.player.Creature()};
        Ignition(ignition).OnPlay(ctx, &ignitionPlay);
        bool plasma = false;
        for (OrbRef ref : f.player.PlayerCombatState().st().orbs)
            plasma |= OrbModel(&f.sim, ref).OrbId_() == OrbId::PLASMA_ORB;
        if (!plasma) return 101;

        CardModel imitate = combat.CreateCard(CardId::IMITATION_LEARNING, f.player);
        CardPileCmd::Add(imitate, PileType::Play);
        CardPlay imitatePlay{.Card = imitate, .Player = f.player, .Target = f.player.Creature()};
        ImitationLearning(imitate).OnPlay(ctx, &imitatePlay);
        PowerModel power = f.player.Creature().GetPower(PowerId::IMITATION_LEARNING_POWER);
        if (!power) return 102;
        ImitationLearningPower imitation(power);
        if (imitation.PlayerTarget() != f.player) return 103;

        CardModel echo = combat.CreateCard(CardId::ECHO_FORM, f.player);
        CardPileCmd::Add(echo, PileType::Play);
        CardPlay echoPlay{.Card = echo, .Player = f.player, .Target = Creature{}, .PlayIndex = 0, .PlayCount = 1};
        imitation.BeforeCardPlayed(&echoPlay);
        CardRef cloneRef = static_cast<CardRef>(echo.st().extra[2]);
        if (!cloneRef || CardModel(&f.sim, cloneRef).Owner() != f.player ||
            CardModel(&f.sim, cloneRef).CloneOf() != echo) return 104;
        imitation.AfterCardPlayed(ctx, &echoPlay);
        if (f.player.Creature().GetPowerAmount(PowerId::ECHO_FORM_POWER) != 2 ||
            f.player.Creature().GetPower(PowerId::IMITATION_LEARNING_POWER)) return 105;
    }
    return 0;
}
