#include <iostream>
#include <stdexcept>
#include <string_view>

#include "engine/run_setup.h"
#include "env/env.h"
#include "game/All.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Models/Enchantments/Vigorous.h"
#include "game/Models/Powers/IllusionPower.h"
#include "game/Models/Powers/PersonalHivePower.h"

using namespace sts2;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Fixture {
    Env env;
    Sim* sim = nullptr;
    Player player;
    CombatState combat;

    Fixture(std::string_view seed, CharacterId character, EncounterId encounter) {
        env.ResetCombat(NewRunOptions{.seed = seed, .character = character}, encounter);
        sim = &env.sim();
        player = Player(sim, 1);
        combat = CombatState(sim);
        if (player.PlayerCombatState()) player.PlayerCombatState().SetEnergy(99);
    }

    Creature add_enemy(MonsterId id) {
        return CreatureCmd::Add(MonsterModel::ToMutable(sim, id), combat, CombatSide::Enemy);
    }
};

Creature find_enemy(CombatState combat, MonsterId id) {
    for (Creature creature : combat.Enemies()) {
        if (creature && creature.Monster().MonsterId_() == id) return creature;
    }
    return {};
}

int count_cards(const CardList& cards, CardId id) {
    int count = 0;
    for (CardModel card : cards) count += card.CardId_() == id;
    return count;
}

void test_personal_hive_accepts_osty_as_a_damage_dealer() {
    Fixture f("AUDITOSTYPERSONALHIVE", CharacterId::NECROBINDER, EncounterId::CULTISTS_NORMAL);
    Creature osty = f.player.PlayerCombatState().GetPet(MonsterId::OSTY);
    Creature enemy = f.combat.Enemies().front();
    require(osty && osty.IsAlive(), "Necrobinder combat did not create Osty");
    require(enemy && enemy.IsAlive(), "combat fixture has no live enemy");

    PlayerChoiceContext context;
    CardModel source = f.combat.CreateCard(CardId::POKE, f.player);
    (void)PowerCmd::Apply(PowerId::PERSONAL_HIVE_POWER, context, enemy, Dec(1),
                          f.player.Creature(), source);
    const int oldEnemyHp = enemy.CurrentHp();
    const CardList oldDraw = f.player.PlayerCombatState().DrawPile().Cards();
    const int oldDazed = count_cards(oldDraw, CardId::DAZED);

    CardPlay play{source, f.player};
    DamageCmd::Attack(Dec(1)).FromOsty(osty, source, &play).Targeting(enemy).Execute(context);

    const CardList newDraw = f.player.PlayerCombatState().DrawPile().Cards();
    require(enemy.CurrentHp() == oldEnemyHp - 1, "Osty's powered attack did not damage its target");
    require(count_cards(newDraw, CardId::DAZED) == oldDazed + 1,
            "Personal Hive did not add one Dazed after Osty's powered attack");
    for (CardModel card : newDraw) {
        if (card.CardId_() == CardId::DAZED)
            require(card.Owner().Creature() == f.player.Creature(),
                    "Personal Hive created Osty's Dazed with no owning player");
    }
}

void check_two_illusion_revive_cycles(Creature creature, CombatState combat) {
    require(creature && creature.HasPower(PowerId::ILLUSION_POWER),
            "illusion test creature did not receive IllusionPower from its source spawn hook");
    MonsterModel monster = creature.Monster();
    IllusionPower illusion(creature.GetPower(PowerId::ILLUSION_POWER));

    for (int death = 0; death < 2; ++death) {
        const std::string_view expectedFollowUp = monster.LastLoggedMoveId();
        CreatureCmd::Kill(creature, true);
        require(creature.IsDead() && illusion.IsReviving(), "Illusion death did not enter its revive phase");
        require(monster.NextMove().Id() == "REVIVE_MOVE" && !monster.NextMove().CanTransitionAway(),
                "the newly-created revive move was allowed to transition before performing");

        monster.SetMoveImmediate(expectedFollowUp);
        require(monster.NextMove().Id() == "REVIVE_MOVE",
                "Illusion transitioned away before performing its healing move");

        monster.PerformMove();
        require(creature.IsAlive() && creature.CurrentHp() == creature.MaxHp(),
                "Illusion revive move did not heal the monster back to full health");
        require(!illusion.IsReviving() && monster.NextMove().CanTransitionAway(),
                "performing the revive move did not clear the revive phase and its once-only gate");

        monster.RollMove(combat.PlayerCreatures());
        require(monster.NextMove().Id() == expectedFollowUp,
                "Illusion revive did not return to the last logged move");
    }
}

void test_illusion_revive_is_required_and_resets_for_a_second_death() {
    {
        Fixture f("AUDITEYEREVIVE", CharacterId::IRONCLAD, EncounterId::CULTISTS_NORMAL);
        Creature fogmog = f.add_enemy(MonsterId::FOGMOG);
        require(static_cast<bool>(fogmog), "failed to add Fogmog to the living combat fixture");
        MonsterModel fogmogModel = fogmog.Monster();
        fogmogModel.SetMoveImmediate("ILLUSION_MOVE", true);
        fogmogModel.PerformMove();  // Original Fogmog move spawns Eye With Teeth.

        Creature eye = find_enemy(f.combat, MonsterId::EYE_WITH_TEETH);
        require(static_cast<bool>(eye), "Fogmog's source summon did not add Eye With Teeth");
        check_two_illusion_revive_cycles(eye, f.combat);
    }

    {
        Fixture f("AUDITPARAFRIGHTREVIVE", CharacterId::IRONCLAD, EncounterId::CULTISTS_NORMAL);
        Creature parafright = f.add_enemy(MonsterId::PARAFRIGHT);
        require(static_cast<bool>(parafright), "failed to add Parafright to the living combat fixture");
        check_two_illusion_revive_cycles(parafright, f.combat);
    }
}

void test_vigorous_only_buffs_the_first_powered_attack() {
    Fixture f("AUDITVIGOROUSHIT", CharacterId::IRONCLAD, EncounterId::CULTISTS_NORMAL);
    const auto enemies = f.combat.Enemies();
    require(enemies.size() >= 2, "Cultists fixture no longer contains two targets");
    Creature firstTarget = enemies[0];
    Creature secondTarget = enemies[1];
    require(firstTarget.Block() == 0 && secondTarget.Block() == 0,
            "Cultists fixture unexpectedly starts with block");

    CardModel hemokinesis = f.combat.CreateCard(CardId::HEMOKINESIS, f.player);
    CardPileCmd::Add(hemokinesis, PileType::Hand);
    EnchantmentModel vigorous = CardCmd::Enchant(EnchantmentId::VIGOROUS, hemokinesis, Dec(5));
    PlayerChoiceContext context;

    const int firstEnemyHp = firstTarget.CurrentHp();
    const int firstPlayerHp = f.player.Creature().CurrentHp();
    CardCmd::AutoPlay(context, hemokinesis, firstTarget);
    require(firstEnemyHp - firstTarget.CurrentHp() == 20,
            "Vigorous did not add five to Hemokinesis's powered 15-damage attack");
    require(firstPlayerHp - f.player.Creature().CurrentHp() == 2,
            "Vigorous incorrectly increased Hemokinesis's unpowered self-damage");
    require(vigorous.Status() == EnchantmentStatus::Disabled,
            "Vigorous was not disabled after the enchanted card was played");

    const int secondEnemyHp = secondTarget.CurrentHp();
    const int secondPlayerHp = f.player.Creature().CurrentHp();
    CardCmd::AutoPlay(context, hemokinesis, secondTarget);
    require(secondEnemyHp - secondTarget.CurrentHp() == 15,
            "a later Hemokinesis play retained Vigorous's powered-attack bonus");
    require(secondPlayerHp - f.player.Creature().CurrentHp() == 2,
            "a later play changed Hemokinesis's unpowered self-damage");
}

void test_personal_hive_status_has_no_player_creator() {
    Fixture f("AUDITHIVEROCKET", CharacterId::DEFECT, EncounterId::CULTISTS_NORMAL);
    CardModel rocket = f.combat.CreateCard(CardId::ROCKET_PUNCH, f.player);
    CardPileCmd::Add(rocket, PileType::Hand);
    PlayerChoiceContext context;
    Creature enemy = f.combat.Enemies().front();
    (void)PowerCmd::Apply(PowerId::PERSONAL_HIVE_POWER, context, enemy, Dec(1), f.player.Creature(), {});
    CardModel strike = f.combat.CreateCard(CardId::STRIKE_DEFECT, f.player);
    CardPlay play{strike, f.player};
    DamageCmd::Attack(Dec(1)).FromCard(strike, &play).Targeting(enemy).Execute(context);
    require(rocket.EnergyCost().GetWithModifiers(CostModifiers::All) == 2,
            "Personal Hive incorrectly attributes its status generation to the player");
    CardPileCmd::AddToCombatAndPreview(CardId::DAZED, f.player.Creature(), PileType::Discard, 1, f.player);
    require(rocket.EnergyCost().GetWithModifiers(CostModifiers::All) == 1,
            "Rocket Punch stopped responding to a genuinely player-generated status");
}

void test_pen_nib_tenth_osty_attack_and_quiet_counter() {
    Fixture f("AUDITPENNIBOSTY", CharacterId::NECROBINDER, EncounterId::CULTISTS_NORMAL);
    RelicModel nib = RelicCmd::Obtain(RelicId::PEN_NIB, f.player);
    Creature enemy = f.combat.Enemies().front();
    CreatureCmd::SetMaxHp(enemy, Dec(1000));
    CreatureCmd::SetCurrentHp(enemy, Dec(1000));
    PlayerChoiceContext context;
    CardModel poke = f.combat.CreateCard(CardId::POKE, f.player);
    CardPileCmd::Add(poke, PileType::Hand);
    for (int attack = 1; attack <= 10; ++attack) {
        const int before = enemy.CurrentHp();
        CardCmd::AutoPlay(context, poke, enemy);
        require(before - enemy.CurrentHp() == (attack == 10 ? 12 : 6),
                "Pen Nib did not double precisely the tenth Osty attack");
    }
    require(nib.DisplayAmount() == 0, "Pen Nib quiet counter did not wrap after ten attacks");
}

}  // namespace

int main() {
    try {
        test_personal_hive_accepts_osty_as_a_damage_dealer();
        test_illusion_revive_is_required_and_resets_for_a_second_death();
        test_vigorous_only_buffs_the_first_powered_attack();
        test_personal_hive_status_has_no_player_creator();
        test_pen_nib_tenth_osty_attack_and_quiet_counter();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "audit interactions regression: " << error.what() << '\n';
        return 1;
    }
}
