#include <stdexcept>
#include <string_view>

#include "env/env.h"
#include "game/All.h"
#include "game/Commands/Builders/AttackContext.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/Powers/VigorPower.h"

using namespace sts2;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void test_omnislice_groups_primary_and_optional_splash_under_one_attack(EncounterId encounter,
                                                                       bool hasSplashTarget) {
    Env env;
    env.ResetCombat(NewRunOptions{.seed = hasSplashTarget ? "AUDITOMNISLICEPAIR" : "AUDITOMNISLICESINGLE",
                                  .character = CharacterId::SILENT}, encounter);
    Player player(&env.sim(), 1);
    CombatState combat(&env.sim());
    PlayerCombatState playerCombat = player.PlayerCombatState();
    playerCombat.SetEnergy(99);
    CreatureList enemies = combat.Enemies();
    require(enemies.size() >= (hasSplashTarget ? 2u : 1u), "Omnislice fixture has too few targets");
    for (Creature enemy : enemies) {
        enemy.st().max_hp = 1000;
        enemy.st().current_hp = 1000;
        enemy.st().block = 0;
    }

    CardModel omnislice = combat.CreateCard(CardId::OMNISLICE, player);
    CardPileCmd::Add(omnislice, PileType::Hand);
    PlayerChoiceContext choiceContext;
    (void)PowerCmd::Apply(PowerId::VIGOR_POWER, choiceContext, player.Creature(), Dec(8),
                          player.Creature(), CardModel{});

    const int primaryHp = enemies[0].CurrentHp();
    const int splashHp = hasSplashTarget ? enemies[1].CurrentHp() : 0;
    env.Step(Action{.kind = Action::PlayCard,
                    .index = playerCombat.Hand().Count() - 1,
                    .target = enemies[0].CombatId()});

    require(primaryHp - enemies[0].CurrentHp() == 16,
            "Omnislice primary hit did not receive Vigor's powered bonus");
    if (hasSplashTarget)
        require(splashHp - enemies[1].CurrentHp() == 16,
                "Omnislice splash did not resolve as its source-defined unpowered copied damage");
    PowerModel vigor = player.Creature().GetPower(PowerId::VIGOR_POWER);
    require(!vigor || vigor.Amount() == Dec(0),
            "Omnislice did not invoke AfterAttack to consume Vigor once for the grouped attack");
}

void test_echoing_slash_uses_the_same_attack_context_hooks() {
    Env env;
    env.ResetCombat(NewRunOptions{.seed = "AUDITECHOINGSLASHATTACK", .character = CharacterId::SILENT},
                    EncounterId::CULTISTS_NORMAL);
    Player player(&env.sim(), 1);
    CombatState combat(&env.sim());
    PlayerCombatState playerCombat = player.PlayerCombatState();
    playerCombat.SetEnergy(99);
    CreatureList enemies = combat.Enemies();
    require(enemies.size() >= 2, "Echoing Slash fixture needs multiple targets");
    for (size_t i = 0; i < enemies.size(); ++i) {
        Creature enemy = enemies[i];
        enemy.st().max_hp = 1000;
        // The first powered wave kills one target, forcing the source loop to
        // attack again while another target remains alive.
        enemy.st().current_hp = i == 0 ? 14 : 1000;
        enemy.st().block = 0;
    }

    CardModel slash = combat.CreateCard(CardId::ECHOING_SLASH, player);
    CardPileCmd::Add(slash, PileType::Hand);
    PlayerChoiceContext choiceContext;
    (void)PowerCmd::Apply(PowerId::VIGOR_POWER, choiceContext, player.Creature(), Dec(4),
                          player.Creature(), CardModel{});
    const int firstHp = enemies[0].CurrentHp();
    const int secondHp = enemies[1].CurrentHp();

    env.Step(Action{.kind = Action::PlayCard, .index = playerCombat.Hand().Count() - 1});

    require(firstHp == 14 && enemies[0].IsDead(),
            "Echoing Slash did not kill the first target in its powered wave");
    require(secondHp == 1000 && secondHp - enemies[1].CurrentHp() == 28 && enemies[1].IsAlive(),
            "Echoing Slash did not keep Vigor active across the kill-triggered second wave");
    PowerModel vigor = player.Creature().GetPower(PowerId::VIGOR_POWER);
    require(!vigor || vigor.Amount() == Dec(0),
            "Echoing Slash did not invoke AfterAttack after its grouped damage calls");
}

void test_attack_context_finishes_after_nonchoice_body_failure() {
    Env env;
    env.ResetCombat(NewRunOptions{.seed = "AUDITATTACKBODYFAULT", .character = CharacterId::SILENT},
                    EncounterId::CULTISTS_NORMAL);
    Player player(&env.sim(), 1);
    CombatState combat(&env.sim());
    Creature target = combat.Enemies().front();
    CardModel card = combat.CreateCard(CardId::OMNISLICE, player);
    CardPlay play{card, player, target};
    PlayerChoiceContext choiceContext;
    (void)PowerCmd::Apply(PowerId::VIGOR_POWER, choiceContext, player.Creature(), Dec(4),
                          player.Creature(), CardModel{});

    AttackContext attack(combat, choiceContext, card, &play);
    bool originalFailurePreserved = false;
    try {
        attack.Run([] { throw Unported("injected attack body failure"); });
    } catch (const Unported& error) {
        originalFailurePreserved = std::string_view(error.what()) == "injected attack body failure";
    }
    require(originalFailurePreserved, "AttackContext did not preserve its original body exception");
    VigorPower vigor(player.Creature().GetPower(PowerId::VIGOR_POWER));
    require(!vigor || vigor.Amount() == 0,
            "AttackContext did not invoke AfterAttack while unwinding an ordinary body failure");
}

void test_attack_context_leaves_needchoice_suspended() {
    Env env;
    env.ResetCombat(NewRunOptions{.seed = "AUDITATTACKCHOICE", .character = CharacterId::SILENT},
                    EncounterId::CULTISTS_NORMAL);
    Player player(&env.sim(), 1);
    CombatState combat(&env.sim());
    Creature target = combat.Enemies().front();
    CardModel card = combat.CreateCard(CardId::OMNISLICE, player);
    CardPlay play{card, player, target};
    PlayerChoiceContext choiceContext;
    (void)PowerCmd::Apply(PowerId::VIGOR_POWER, choiceContext, player.Creature(), Dec(4),
                          player.Creature(), CardModel{});

    AttackContext attack(combat, choiceContext, card, &play);
    ChoiceRequest request;
    request.prompt = "audit attack suspension";
    request.option_count = 2;
    bool originalChoicePreserved = false;
    try {
        attack.Run([&] { throw NeedChoice{request}; });
    } catch (const NeedChoice& choice) {
        originalChoicePreserved = choice.request.prompt == request.prompt &&
                                  choice.request.option_count == request.option_count;
    }
    require(originalChoicePreserved, "AttackContext changed or swallowed the suspended choice request");
    VigorPower vigor(player.Creature().GetPower(PowerId::VIGOR_POWER));
    require(vigor && vigor.Amount() == 4 && vigor.Active() && vigor.AmountWhenAttackStarted() == 4,
            "NeedChoice prematurely ran AfterAttack instead of leaving the attack context suspended");
}

}  // namespace

int main() {
    test_omnislice_groups_primary_and_optional_splash_under_one_attack(
        EncounterId::FROG_KNIGHT_NORMAL, false);
    test_omnislice_groups_primary_and_optional_splash_under_one_attack(
        EncounterId::CULTISTS_NORMAL, true);
    test_echoing_slash_uses_the_same_attack_context_hooks();
    test_attack_context_finishes_after_nonchoice_body_failure();
    test_attack_context_leaves_needchoice_suspended();
}
