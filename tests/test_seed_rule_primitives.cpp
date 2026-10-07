#include <cstdio>
#include <stdexcept>

#include "env/env.h"
#include "game/All.h"
#include "game/Combat/CombatManager.h"
#include "game/Combat/History/CombatHistory.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/Powers/GigantificationPower.h"
#include "game/Models/Powers/UnmovablePower.h"

using namespace sts2;

static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

static void unmovable_history() {
    Env env;
    env.ResetCombat({.seed = "UNMOVABLEHISTORY", .character = CharacterId::IRONCLAD}, EncounterId::CULTISTS_NORMAL);
    Player player(&env.sim(), 1);
    CombatState combat(&env.sim());
    CardModel card = combat.CreateCard(CardId::DEFEND_IRONCLAD, player);
    CardPlay a{card, player, player.Creature()}, b{card, player, player.Creature()};
    PlayerChoiceContext ctx;
    PowerModel raw = PowerCmd::Apply(PowerId::UNMOVABLE_POWER, ctx, player.Creature(), Dec(1), player.Creature(), {});
    UnmovablePower power(raw);
    auto history = CombatManager(&env.sim()).History();
    history.Clear();
    // Golden multipliers come from tools/UnmovableChecks against the shipped
    // v0.111.0 DLL with TestMode=false, not from simulator output.
    auto query = [&](const char* name, const CardPlay* play, int expected, ValueProp props = ValueProp::Move) {
        require(power.ModifyBlockMultiplicative(player.Creature(), Dec(5), props, card, play) == Dec(expected), name);
    };
    auto block = [&](const CardPlay* play, ValueProp props = ValueProp::Move) {
        history.BlockGained(combat, player.Creature(), 5, props, play);
    };
    query("empty-first", &a, 2);
    block(&a);
    query("same-play-one", &a, 2); query("different-play-one", &b, 1); query("null-play-one", nullptr, 1);
    block(&a); query("same-play-two", &a, 2);
    power.SetAmount(2, true); query("different-play-two-entries-limit-two", &b, 1);
    block(&b); power.SetAmount(1, true);
    query("outer-after-inner", &a, 1); query("non-move", &b, 1, ValueProp::Unpowered);
    combat.SetCurrentSide(CombatSide::Enemy); query("new-side", &a, 2);
    block(&a); query("same-play-new-side", &a, 2); query("different-play-new-side", &b, 1);
    combat.SetCurrentSide(CombatSide::Player); combat.SetRoundNumber(combat.RoundNumber() + 1);
    query("new-round", &b, 2);
    block(&a, ValueProp::Move | ValueProp::Unpowered); query("move-even-if-unpowered-counts", &b, 1);
    player.PlayerCombatState().st().turn_number = 2; query("extra-player-turn", &b, 2);
    history.Clear(); block(&a, ValueProp::Unpowered); block(nullptr);
    query("non-move-and-null-entries-ignored", &b, 2);
    block(&a); history.Clear(); block(&b); query("clear-excludes-old-own-entries", &a, 1);

    const std::string snapshot = env.ExportSnapshotJson();
    auto restored = Env::ImportSnapshotJson(snapshot);
    require(restored->ExportSnapshotJson() == snapshot, "block history phase/count failed snapshot round-trip");
    for (Env* current : {&env, restored.get()}) {
        Sim& sim = current->sim();
        Player owner(&sim, 1);
        CardModel model(&sim, card.Index());
        CardPlay next{model, owner, owner.Creature()};
        UnmovablePower copy(&sim, raw.Index());
        require(copy.ModifyBlockMultiplicative(owner.Creature(), Dec(5), ValueProp::Move, model, &next) == Dec(1),
                "snapshot successor forgot another play's block entry");
        CombatManager(&sim).History().BlockGained(CombatState(&sim), owner.Creature(), 5, ValueProp::Move, &next);
    }
    require(env.Hash() == restored->Hash(), "block history snapshot successors differ");
}

static void gigantification_command_identity() {
    Env env;
    env.ResetCombat({.seed = "GIGANTPET", .character = CharacterId::NECROBINDER}, EncounterId::CULTISTS_NORMAL);
    Sim& sim = env.sim();
    Player player(&sim, 1);
    CombatState combat(&sim);
    CardModel card = combat.CreateCard(CardId::STRIKE_NECROBINDER, player);
    CardModel other = combat.CreateCard(CardId::REAP, player);
    Creature osty = player.PlayerCombatState().GetPet(MonsterId::OSTY);
    require(bool(osty), "Gigantification fixture has no Osty");
    PlayerChoiceContext ctx;
    PowerModel raw = PowerCmd::Apply(PowerId::GIGANTIFICATION_POWER, ctx, player.Creature(), Dec(2), player.Creature(), {});
    GigantificationPower power(raw);
    auto query = [&](const char* name, CardModel source, int multiplier, int amount, ValueProp props = ValueProp::Move) {
        require(power.ModifyDamageMultiplicative({}, Dec(1), props, osty, source, nullptr) == Dec(multiplier), name);
        require(power.Amount() == amount, name);
    };
    AttackCommand outer(Dec(1)), same(Dec(1)), different(Dec(1)), noSource(Dec(1));
    outer.FromOsty(osty, card, nullptr); same.FromOsty(osty, card, nullptr); different.FromCard(other, nullptr);
    query("no-active-attack", card, 3, 2);
    power.BeforeAttack(noSource); query("null-model-before-attack", card, 3, 2);
    power.BeforeAttack(outer);
    query("pet-active-same-card", card, 3, 2); query("pet-active-other-card", other, 1, 2);
    query("unpowered", card, 1, 2, ValueProp::Move | ValueProp::Unpowered); query("null-card", {}, 1, 2);
    power.BeforeAttack(same); power.AfterAttack(ctx, same); query("nested-same-did-not-consume", card, 3, 2);
    power.BeforeAttack(different); power.AfterAttack(ctx, different); query("nested-other-did-not-consume", card, 3, 2);
    power.AfterAttack(ctx, outer); query("outer-consumed-once", other, 3, 1);
    power.AfterAttack(ctx, outer); query("duplicate-after-did-not-consume", other, 3, 1);
}

static void inky_canonical_variable() {
    Env env;
    env.ResetCombat({.seed = "INKYCANONICAL", .character = CharacterId::SILENT}, EncounterId::CULTISTS_NORMAL);
    Player player(&env.sim(), 1);
    CardModel shiv = CombatState(&env.sim()).CreateCard(CardId::SHIV, player);
    EnchantmentModel inky = CardCmd::Enchant(EnchantmentId::INKY, shiv, Dec(1));
    require(DynamicVarSet(inky).Weak().BaseValue() == Dec(1), "native Inky canonical Weak must be one");
}

int main() {
    unmovable_history();
    gigantification_command_identity();
    inky_canonical_variable();
    std::puts("native block history, nested pet attack identity and Inky variables passed");
}
