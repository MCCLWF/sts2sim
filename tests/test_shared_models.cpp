#include <span>
#include <iostream>
#include <stdexcept>

#include "engine/run_setup.h"
#include "engine/sim/state_hash.h"
#include "env/env.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/PotionCmd.h"
#include "game/Hooks/Hook.h"
#include "game/Models/Cards/MadScience.h"
#include "game/Models/Powers/ToricToughnessPower.h"
#include "game/Models/Powers/CuriousPower.h"
#include "game/Models/Powers/DoomPower.h"
#include "game/Models/Powers/ImprovementPower.h"
#include "game/Runs/RunManager.h"

using namespace sts2;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Fixture {
    GameState state;
    Sim sim;
    Player player;
    Creature enemy;

    explicit Fixture(CharacterId character = CharacterId::IRONCLAD)
        : sim(state), player(&sim, 1) {
        new_run(state, NewRunOptions{.seed = "SHAREDMODELS", .character = character, .ascension = 0});
        RunManager::SetActInternal(&sim, 0);
        RunManager::EnterMapCoordDebug(&sim, MapCoord{0, 0}, EncounterId::CULTISTS_NORMAL);
        enemy = CombatState(&sim).Enemies().front();
        player.PlayerCombatState().SetEnergy(99);
    }

    CardModel add(CardId id) {
        CardModel card = CombatState(&sim).CreateCard(id, player);
        CardPileCmd::Add(card, PileType::Hand);
        return card;
    }

    CardModel add_mad_science(CardType type, MadScienceRider rider) {
        CardModel card = add(CardId::MAD_SCIENCE);
        MadScience mad(card);
        mad.SetTinkerTimeType(type);
        mad.SetTinkerTimeRider(rider);
        return card;
    }

    void play(CardModel card, Creature target = {}) {
        PlayerChoiceContext context;
        CardCmd::AutoPlay(context, card, target);
    }
};

void test_mad_science_variants() {
    {
        Fixture f;
        int hp = f.enemy.CurrentHp();
        CardModel card = f.add_mad_science(CardType::Attack, MadScienceRider::Violence);
        f.play(card, f.enemy);
        require(f.enemy.CurrentHp() == hp - 36, "Mad Science Violence must hit three times");
    }
    {
        Fixture f;
        CardModel card = f.add_mad_science(CardType::Attack, MadScienceRider::Sapping);
        f.play(card, f.enemy);
        require(f.enemy.GetPowerAmount(PowerId::WEAK_POWER) == 2 &&
                f.enemy.GetPowerAmount(PowerId::VULNERABLE_POWER) == 2,
                "Mad Science Sapping must apply both two-stack debuffs");
    }
    {
        Fixture f;
        CardModel card = f.add_mad_science(CardType::Attack, MadScienceRider::Choking);
        f.play(card, f.enemy);
        require(f.enemy.GetPowerAmount(PowerId::STRANGLE_POWER) == 6,
                "Mad Science Choking must apply Strangle");
    }
    {
        Fixture f;
        int energy = f.player.PlayerCombatState().Energy();
        CardModel card = f.add_mad_science(CardType::Skill, MadScienceRider::Energized);
        f.play(card, f.player.Creature());
        require(f.player.Creature().Block() == 8 &&
                f.player.PlayerCombatState().Energy() == energy + 2,
                "Mad Science Energized must block and gain two energy");
    }
    {
        Fixture f;
        const int hand = f.player.PlayerCombatState().Hand().Count();
        CardModel card = f.add_mad_science(CardType::Skill, MadScienceRider::Wisdom);
        f.play(card, f.player.Creature());
        require(f.player.PlayerCombatState().Hand().Count() == hand + 3,
                "Mad Science Wisdom must draw three cards");
    }
    {
        Fixture f;
        CardModel card = f.add_mad_science(CardType::Skill, MadScienceRider::Chaos);
        CardList before = f.player.PlayerCombatState().Hand().Cards();
        f.play(card, f.player.Creature());
        CardModel generated;
        for (CardModel item : f.player.PlayerCombatState().Hand().Cards())
            if (!before.contains(item)) generated = item;
        require(generated && generated != card &&
                generated.EnergyCost().GetWithModifiers(CostModifiers::All) == Dec(0),
                "Mad Science Chaos must add one free generated card to hand");
    }
    {
        Fixture f;
        CardModel card = f.add_mad_science(CardType::Power, MadScienceRider::Expertise);
        f.play(card, f.player.Creature());
        require(f.player.Creature().GetPowerAmount(PowerId::STRENGTH_POWER) == 2 &&
                f.player.Creature().GetPowerAmount(PowerId::DEXTERITY_POWER) == 2,
                "Mad Science Expertise must grant two Strength and Dexterity");
    }
    {
        Fixture f;
        CardModel card = f.add_mad_science(CardType::Power, MadScienceRider::Curious);
        f.play(card, f.player.Creature());
        PowerModel curious = f.player.Creature().GetPower(PowerId::CURIOUS_POWER);
        CardModel demonForm = CombatState(&f.sim).CreateCard(CardId::DEMON_FORM, f.player);
        CardPileCmd::Add(demonForm, PileType::Hand);
        const Dec originalCost = demonForm.EnergyCost().Canonical();
        const Dec modifiedCost = demonForm.EnergyCost().GetWithModifiers(CostModifiers::All);
        require(curious && curious.Amount() == 1 && originalCost > Dec(0) && modifiedCost == originalCost - Dec(1),
                "Mad Science Curious must reduce positive Power-card costs by one");
    }
    {
        Fixture f;
        CardModel card = f.add_mad_science(CardType::Power, MadScienceRider::Improvement);
        f.play(card, f.player.Creature());
        PowerModel improvement = f.player.Creature().GetPower(PowerId::IMPROVEMENT_POWER);
        require(improvement && improvement.Amount() == 1, "Mad Science Improvement power missing");
        int before = 0;
        for (CardModel item : f.player.Deck().Cards()) before += item.CurrentUpgradeLevel() > 0;
        ImprovementPower(improvement).AfterCombatEnd(CombatRoom{});
        int after = 0;
        for (CardModel item : f.player.Deck().Cards()) after += item.CurrentUpgradeLevel() > 0;
        require(after == before + 1, "Improvement must upgrade exactly one deck card after combat");
    }
}

void test_necrobinder_potions() {
    {
        Fixture f(CharacterId::NECROBINDER);
        const auto result = PotionCmd::TryToProcure(PotionId::POTION_OF_DOOM, f.player);
        require(result.success, "Potion of Doom could not be procured");
        PlayerChoiceContext ctx;
        f.player.Potions().front().OnUseWrapper(ctx, f.enemy);
        PowerModel doom = f.enemy.GetPower(PowerId::DOOM_POWER);
        require(doom && doom.Amount() == data(PotionId::POTION_OF_DOOM).vars[0].base,
                "Potion of Doom did not apply its configured Doom amount");
        f.enemy.SetCurrentHpInternal(doom.Amount());
        DoomPower(doom).BeforeSideTurnEnd(ctx, CombatSide::Enemy, CombatState(&f.sim).Enemies());
        require(f.enemy.IsDead(), "Doom must kill the doomed target at its side-turn boundary");
    }
    {
        Fixture f(CharacterId::NECROBINDER);
        require(PotionCmd::TryToProcure(PotionId::POT_OF_GHOULS, f.player).success,
                "Pot of Ghouls could not be procured");
        PlayerChoiceContext ctx;
        f.player.Potions().front().OnUseWrapper(ctx, f.player.Creature());
        int souls = 0;
        for (CardModel card : f.player.PlayerCombatState().Hand().Cards()) souls += card.CardId_() == CardId::SOUL;
        require(souls == 2, "Pot of Ghouls must generate two Soul cards in hand");
    }
    {
        Fixture f(CharacterId::NECROBINDER);
        Creature osty = f.player.PlayerCombatState().GetPet(MonsterId::OSTY);
        require(static_cast<bool>(osty), "Necrobinder combat fixture has no Osty");
        const int hp = osty.CurrentHp();
        require(PotionCmd::TryToProcure(PotionId::BONE_BREW, f.player).success,
                "Bone Brew could not be procured");
        PlayerChoiceContext ctx;
        f.player.Potions().front().OnUseWrapper(ctx, f.player.Creature());
        require(osty.CurrentHp() > hp, "Bone Brew must summon/heal Osty");
    }
}

void test_toric_toughness_power() {
    Fixture f;
    CardModel card = f.add(CardId::TORIC_TOUGHNESS);
    const int initialBlock = f.player.Creature().Block();
    f.play(card, f.player.Creature());
    require(f.player.Creature().Block() == initialBlock + 5,
            "Toric Toughness must grant its immediate block");
    PowerModel power = f.player.Creature().GetPower(PowerId::TORIC_TOUGHNESS_POWER);
    require(power && power.Amount() == 2 && power.DynamicVars().Block().BaseValue() == Dec(5),
            "Toric Toughness must store turns and the block dynamic var");

    Hook::AfterBlockCleared(CombatState(&f.sim), f.player.Creature());
    require(f.player.Creature().Block() == initialBlock + 10,
            "Toric Toughness must reapply block when block clears");
    power = f.player.Creature().GetPower(PowerId::TORIC_TOUGHNESS_POWER);
    require(power && power.Amount() == 1, "Toric Toughness must decrement after a block clear");

    Hook::AfterBlockCleared(CombatState(&f.sim), f.player.Creature());
    require(f.player.Creature().Block() == initialBlock + 15 &&
            !f.player.Creature().HasPower(PowerId::TORIC_TOUGHNESS_POWER),
            "Toric Toughness must remove itself after its final trigger");
}

void test_mad_science_clone_snapshot_failure() {
    Env env;
    env.ResetCombat(NewRunOptions{.seed = "MADSCIENCESNAPSHOT", .character = CharacterId::IRONCLAD},
                   EncounterId::CULTISTS_NORMAL);
    Player player(&env.sim(), 1);
    CardModel card = CombatState(&env.sim()).CreateCard(CardId::MAD_SCIENCE, player);
    CardPileCmd::Add(card, PileType::Hand);
    MadScience mad(card);
    mad.SetTinkerTimeType(CardType::Power);
    mad.SetTinkerTimeRider(MadScienceRider::Curious);
    CardModel clone = card.CreateClone();
    MadScience cloned(clone);
    require(cloned.TinkerTimeType() == CardType::Power && cloned.TinkerTimeRider() == MadScienceRider::Curious &&
            clone.CloneOf() == card, "Mad Science clone lost its event configuration");

    const uint64_t expectedHash = env.Hash();
    std::string snapshot = env.ExportSnapshotJson();
    auto copy = env.Clone();
    auto restored = Env::ImportSnapshotJson(snapshot);
    require(copy->Hash() == expectedHash && restored->Hash() == expectedHash &&
            copy->ExportSnapshotJson() == snapshot && restored->ExportSnapshotJson() == snapshot,
            "Mad Science clone or snapshot restore changed the simulation");
    MadScience restoredCard(CardModel(&restored->sim(), card.Index()));
    require(restoredCard.TinkerTimeType() == CardType::Power &&
            restoredCard.TinkerTimeRider() == MadScienceRider::Curious,
            "Mad Science snapshot lost custom card state");

    CardModel bad = CombatState(&env.sim()).CreateCard(CardId::MAD_SCIENCE, player);
    MadScience(bad).SetTinkerTimeType(CardType::Attack);
    MadScience(bad).SetTinkerTimeRider(MadScienceRider::Sapping);
    const uint64_t beforeFailure = hash_state(env.state());
    bool failed = false;
    try {
        PlayerChoiceContext ctx;
        MadScience(bad).OnPlay(ctx, nullptr);
    } catch (const GameError&) {
        failed = true;
    }
    require(failed && hash_state(env.state()) == beforeFailure,
            "invalid Mad Science target did not fail without state changes");
}

}  // namespace

int main() {
    try {
        test_mad_science_variants();
        test_necrobinder_potions();
        test_toric_toughness_power();
        test_mad_science_clone_snapshot_failure();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "shared models regression: " << error.what() << '\n';
        return 1;
    }
}
