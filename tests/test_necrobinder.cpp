#include <algorithm>
#include <array>
#include <stdexcept>

#include "engine/run_setup.h"
#include "engine/sim/alloc.h"
#include "env/env.h"
#include "game/All.h"
#include "game/Commands/CardCmd.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/DamageCmd.h"
#include "game/Commands/OstyCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Factories/CardFactory.h"
#include "game/Hooks/Hook.h"
#include "game/Models/Enchantments/SoulsPower.h"
#include "game/Models/Enchantments/Corrupted.h"
#include "game/Models/Powers/DoomPower.h"
#include "game/Models/Relics/NecrobinderRelics.h"
#include "game/Runs/RunManager.h"

using namespace sts2;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

constexpr uint64_t kAllEpochs = 0x1ffffffffffffffULL;

struct CombatFixture {
    Env env;
    Sim* sim = nullptr;
    Player player;
    CombatState combat;

    explicit CombatFixture(std::string_view seed = "NECROBINDERTEST") {
        env.ResetCombat(NewRunOptions{.seed = seed, .character = CharacterId::NECROBINDER}, EncounterId::CULTISTS_NORMAL);
        sim = &env.sim();
        player = Player(sim, 1);
        combat = CombatState(sim);
    }
};

void test_necrobinder_roster_start_and_epoch_filters() {
    GameState state;
    Sim sim(state);
    new_run(state, NewRunOptions{.seed = "NECROSTARTCONFIG", .character = CharacterId::NECROBINDER});
    Player player(&sim, 1);

    require(player.Character() == CharacterId::NECROBINDER, "new run did not preserve Necrobinder selection");
    require(player.Creature().CurrentHp() == 66 && player.Creature().MaxHp() == 66,
            "Necrobinder starting HP is not 66");
    require(player.Gold() == 99 && player.MaxEnergy() == 3, "Necrobinder starting gold or energy changed");
    require(player.Deck().Count() == 10, "Necrobinder starting deck is not ten cards");
    std::array<int32_t, 4> starterCounts{};
    for (CardModel card : player.Deck().Cards()) {
        switch (card.CardId_()) {
        case CardId::STRIKE_NECROBINDER: ++starterCounts[0]; break;
        case CardId::DEFEND_NECROBINDER: ++starterCounts[1]; break;
        case CardId::BODYGUARD: ++starterCounts[2]; break;
        case CardId::UNLEASH: ++starterCounts[3]; break;
        default: throw std::runtime_error("Necrobinder starting deck contains an unexpected card");
        }
    }
    require(starterCounts == std::array<int32_t, 4>{4, 4, 1, 1},
            "Necrobinder starting deck does not contain four Strikes, four Defends, Bodyguard and Unleash");
    require(static_cast<bool>(player.GetRelic(RelicId::BOUND_PHYLACTERY)), "Bound Phylactery is not the starting relic");
    require(player.Potions().empty(), "Necrobinder starts with a potion");
    require(EventModel::Canonical(&sim, EventId::BYRDONIS_NEST).IsAllowed(RunState(&sim)),
            "Bound Phylactery's Osty summon was incorrectly treated as an event pet by Byrdonis Nest");

    const auto cards = CardFactory::GetUnlockedCards(CardPoolId::NECROBINDER_CARD_POOL, player);
    const auto sourcePool = data(CardPoolId::NECROBINDER_CARD_POOL).cards;
    size_t expectedSinglePlayerCards = 0;
    require(sourcePool.size() == 91, "Necrobinder source pool is not 91 cards");
    for (CardId id : sourcePool) {
        if (data(id).multiplayer != CardMultiplayerConstraint::MultiplayerOnly) {
            ++expectedSinglePlayerCards;
            require(cards.contains(id), "a single-player Necrobinder pool card was filtered out");
        } else {
            require(!cards.contains(id), "a multiplayer-only Necrobinder card entered the single-player pool");
        }
        require(IsUsable(ModelKind::Card, static_cast<uint16_t>(id)), "Necrobinder pool card is not dispatched");
    }
    require(cards.size() == expectedSinglePlayerCards,
            "Necrobinder's fully unlocked single-player card pool has the wrong size");
    require(IsUsable(ModelKind::Card, static_cast<uint16_t>(CardId::SOUL)) &&
                IsUsable(ModelKind::Card, static_cast<uint16_t>(CardId::SWEEPING_GAZE)),
            "Necrobinder's generated Soul and Sweeping Gaze models are not registered");
    CardModel egg = player.RunState().CreateCard(CardId::BYRDONIS_EGG, player);
    require(IsUsable(ModelKind::Card, static_cast<uint16_t>(CardId::BYRDONIS_EGG)) &&
                egg.Keywords().Contains(CardKeyword::Unplayable),
            "Byrdonis Egg is not a usable unplayable quest-card model");
    CardPileCmd::Add(egg, PileType::Deck);
    require(!EventModel::Canonical(&sim, EventId::BYRDONIS_NEST).IsAllowed(RunState(&sim)),
            "Byrdonis Egg did not satisfy the Byrdonis Nest pet predicate");

    constexpr std::array<PowerId, 34> singlePlayerPowerTargets = {
        PowerId::BORROWED_TIME_POWER, PowerId::CACOPHONY_POWER, PowerId::CALCIFY_POWER,
        PowerId::CALL_OF_THE_VOID_POWER, PowerId::COUNTDOWN_POWER, PowerId::DANSE_MACABRE_POWER,
        PowerId::DEBILITATE_POWER, PowerId::DEMESNE_POWER, PowerId::DEVOUR_LIFE_POWER,
        PowerId::ENERGY_NEXT_TURN_POWER, PowerId::ENFEEBLING_TOUCH_POWER, PowerId::FORBIDDEN_GRIMOIRE_POWER,
        PowerId::FRIENDSHIP_POWER, PowerId::HANG_POWER, PowerId::HAUNT_POWER, PowerId::LETHALITY_POWER,
        PowerId::NECRO_MASTERY_POWER, PowerId::NEUROSURGE_POWER, PowerId::OBLIVION_POWER,
        PowerId::PAGESTORM_POWER, PowerId::REAPER_FORM_POWER, PowerId::SENTRY_MODE_POWER,
        PowerId::SHROUD_POWER, PowerId::SIC_EM_POWER, PowerId::SLEIGHT_OF_FLESH_POWER,
        PowerId::SOULBOUND_POWER, PowerId::SPIRIT_OF_ASH_POWER, PowerId::STRENGTH_POWER,
        PowerId::SUMMON_NEXT_TURN_POWER, PowerId::UNDERWORLD_POWER, PowerId::VEILPIERCER_POWER,
        PowerId::VULNERABLE_POWER, PowerId::WEAK_POWER, PowerId::DOOM_POWER};
    for (PowerId id : singlePlayerPowerTargets)
        require(IsUsable(ModelKind::Power, static_cast<uint16_t>(id)),
                "a single-player Necrobinder power target is not registered");

    const auto relicPool = data(RelicPoolId::NECROBINDER_RELIC_POOL).relics;
    require(relicPool.size() == 8, "Necrobinder relic pool is not eight relics");
    for (RelicId id : relicPool)
        require(IsUsable(ModelKind::Relic, static_cast<uint16_t>(id)), "Necrobinder relic is not dispatched");

    const struct CardGate { uint8_t epoch; std::array<CardId, 3> cards; } cardGates[] = {
        {27, {CardId::SCOURGE, CardId::OBLIVION, CardId::COUNTDOWN}},
        {30, {CardId::AFTERLIFE, CardId::SACRIFICE, CardId::CALCIFY}},
        {32, {CardId::SCULPTING_STRIKE, CardId::VEILPIERCER, CardId::BANSHEES_CRY}},
    };
    for (const CardGate& gate : cardGates) {
        GameState gateState;
        Sim gateSim(gateState);
        const uint64_t mask = kAllEpochs & ~(uint64_t{1} << gate.epoch);
        new_run(gateState, NewRunOptions{.seed = "NECROCARDGATE", .character = CharacterId::NECROBINDER,
                                        .profile = ProfileConfig{.all_unlocked = true, .unlocked_epoch_bits = mask}});
        const auto gated = CardFactory::GetUnlockedCards(CardPoolId::NECROBINDER_CARD_POOL, Player(&gateSim, 1));
        for (CardId id : gate.cards) require(!gated.contains(id), "an unrevealed Necrobinder epoch card remained in the pool");
        for (const CardGate& other : cardGates) {
            if (other.epoch == gate.epoch) continue;
            for (CardId id : other.cards) require(gated.contains(id), "one epoch gate incorrectly hid another epoch's cards");
        }
    }
}

void test_necrobinder_relic_epoch_gates_and_acquisition() {
    const struct RelicGate { uint8_t epoch; std::array<RelicId, 3> relics; } gates[] = {
        {28, {RelicId::BONE_FLUTE, RelicId::FUNERARY_MASK, RelicId::BOOK_REPAIR_KNIFE}},
        {31, {RelicId::IVORY_TILE, RelicId::BIG_HAT, RelicId::BOOKMARK}},
    };
    for (const RelicGate& gate : gates) {
        GameState state;
        Sim sim(state);
        const uint64_t mask = kAllEpochs & ~(uint64_t{1} << gate.epoch);
        new_run(state, NewRunOptions{.seed = "NECRORELICGATE", .character = CharacterId::NECROBINDER,
                                    .profile = ProfileConfig{.all_unlocked = true, .unlocked_epoch_bits = mask}});
        const auto& offered = state.run.player.relic_grab_bag.original;
        for (RelicId id : gate.relics)
            require(!offered.contains(id), "an unrevealed Necrobinder relic remained in the character grab bag");
        for (const RelicGate& other : gates) {
            if (other.epoch == gate.epoch) continue;
            for (RelicId id : other.relics)
                require(offered.contains(id), "one relic epoch gate incorrectly hid another epoch's relics");
        }
    }

    CombatFixture f("NECRORELICACQUIRE");
    constexpr std::array<RelicId, 8> relics = {
        RelicId::BIG_HAT, RelicId::BONE_FLUTE, RelicId::BOOK_REPAIR_KNIFE, RelicId::BOOKMARK,
        RelicId::BOUND_PHYLACTERY, RelicId::FUNERARY_MASK, RelicId::IVORY_TILE, RelicId::UNDYING_SIGIL};
    for (RelicId id : relics) {
        if (!f.player.GetRelic(id)) (void)RelicCmd::Obtain(id, f.player);
        require(static_cast<bool>(f.player.GetRelic(id)), "Necrobinder relic could not be obtained");
    }
}

void test_necrobinder_osty_generation_and_relic_hooks() {
    CombatFixture f("NECROOSTYRELICS");
    PlayerChoiceContext ctx;
    Creature startingOsty = f.player.PlayerCombatState().GetPet(MonsterId::OSTY);
    require(startingOsty && startingOsty.IsAlive(), "Bound Phylactery did not summon Osty at combat start");
    const int32_t oldOstyMaxHp = startingOsty.MaxHp();
    const int32_t oldOstyCurrentHp = startingOsty.CurrentHp();
    Creature osty = OstyCmd::Summon(ctx, f.player, Dec(15), CardModel{});
    require(osty && osty.IsAlive() && osty.MaxHp() == oldOstyMaxHp + 15 &&
                osty.CurrentHp() == oldOstyCurrentHp + 15,
            "Osty summon did not add the exact max HP and healing amount to the existing pet");

    CardModel source = f.combat.CreateCard(CardId::POKE, f.player);
    CardPlay play{source, f.player};
    Creature enemy = f.combat.Enemies().front();
    CardModel soul = f.combat.CreateCard(CardId::SOUL, f.player);
    const int handBefore = f.player.PlayerCombatState().Hand().Count();
    CardPileCmd::AddGeneratedCardToCombat(soul, PileType::Hand, f.player);
    require(soul.Pile().Type() == PileType::Hand && f.player.PlayerCombatState().Hand().Count() == handBefore + 1,
            "generated Soul did not enter the requested combat pile");

    (void)RelicCmd::Obtain(RelicId::BONE_FLUTE, f.player);
    const int blockBefore = f.player.Creature().Block();
    DamageCmd::Attack(Dec(1)).FromOsty(osty, source, &play).Targeting(enemy).Execute(ctx);
    require(f.player.Creature().Block() == blockBefore + 2, "Bone Flute did not block after Osty's attack");

    const int petHpBeforeAttack = osty.CurrentHp();
    const int ownerHpBeforeRedirect = f.player.Creature().CurrentHp();
    CreatureCmd::Damage(ctx, f.player.Creature(), Dec(f.player.Creature().Block() + 5), ValueProp::Move, enemy);
    require(osty.IsAlive() && osty.CurrentHp() == petHpBeforeAttack - 5 &&
                f.player.Creature().CurrentHp() == ownerHpBeforeRedirect,
            "Die For You did not redirect powered damage from Necrobinder to Osty");

    CreatureCmd::Kill(osty);
    require(!osty.IsAlive(), "test could not put Osty in the dead/revivable combat state");
    osty = OstyCmd::Summon(ctx, f.player, Dec(7), source);
    require(osty && osty.IsAlive() && osty.MaxHp() == 7 && osty.CurrentHp() == 7,
            "Osty summon did not revive the existing dead pet at the requested max HP");

    RelicModel mask = RelicCmd::Obtain(RelicId::FUNERARY_MASK, f.player);
    auto soul_count = [&] {
        int count = 0;
        for (CardModel card : f.player.PlayerCombatState().DrawPile().Cards()) count += card.CardId_() == CardId::SOUL;
        return count;
    };
    const int soulsBefore = soul_count();
    mask.BeforeHandDraw(f.player, ctx, f.combat);
    require(soul_count() == soulsBefore + 3,
            "Funerary Mask did not add three Souls before the first hand draw");

    RelicModel bigHat = RelicCmd::Obtain(RelicId::BIG_HAT, f.player);
    const CardList priorHand = f.player.PlayerCombatState().Hand().Cards();
    const int handCount = static_cast<int>(priorHand.size());
    bigHat.AfterSideTurnStart(CombatSide::Player, f.combat.PlayerCreatures(), f.combat);
    require(f.player.PlayerCombatState().Hand().Count() == handCount + 2, "Big Hat did not create two hand cards");
    CardList hatCards;
    for (CardModel card : f.player.PlayerCombatState().Hand().Cards()) {
        if (std::find(priorHand.begin(), priorHand.end(), card) == priorHand.end()) hatCards.push_back(card);
    }
    require(hatCards.size() == 2, "Big Hat did not add exactly two generated cards");
    require(hatCards[0].CardId_() != hatCards[1].CardId_(),
            "Big Hat did not generate distinct cards");

    RelicModel repair = RelicCmd::Obtain(RelicId::BOOK_REPAIR_KNIFE, f.player);
    f.player.Creature().SetCurrentHpInternal(f.player.Creature().CurrentHp() - 10);
    const int injured = f.player.Creature().CurrentHp();
    repair.AfterDiedToDoom(ctx, CreatureList{enemy});
    require(f.player.Creature().CurrentHp() == injured + 3, "Book Repair Knife did not heal for a Doom fatality");

    RelicModel bookmark = RelicCmd::Obtain(RelicId::BOOKMARK, f.player);
    CardModel retained = f.combat.CreateCard(CardId::BANSHEES_CRY, f.player);
    CardPileCmd::Add(retained, PileType::Hand);
    const int oldCost = retained.EnergyCost().GetWithModifiers(CostModifiers::Local);
    bookmark.AfterFlush(ctx, f.player, CardList{}, CardList{retained});
    require(retained.EnergyCost().GetWithModifiers(CostModifiers::Local) == oldCost - 1,
            "Bookmark did not reduce a positive-cost retained card until played");

    RelicModel ivory = RelicCmd::Obtain(RelicId::IVORY_TILE, f.player);
    const int energyBefore = f.player.PlayerCombatState().Energy();
    CardPlay expensivePlay{source, f.player};
    expensivePlay.Resources.EnergyValue = 3;
    ivory.AfterCardPlayed(ctx, &expensivePlay);
    require(f.player.PlayerCombatState().Energy() == energyBefore + 1, "Ivory Tile did not grant energy at its threshold");

    RelicModel sigil = RelicCmd::Obtain(RelicId::UNDYING_SIGIL, f.player);
    PowerRef doomRef = alloc_power(*f.sim->g, PowerId::DOOM_POWER);
    PowerModel doom(f.sim, doomRef);
    doom.st().owner = enemy.Index();
    doom.st().amount = enemy.CurrentHp();
    enemy.ApplyPowerInternal(doom);
    const Dec sigilFactor = sigil.ModifyDamageMultiplicative(f.player.Creature(), Dec(10), ValueProp::Move,
                                                             enemy, source, nullptr);
    require(sigilFactor == Dec::lit(5, 1), "Undying Sigil did not halve damage from an enemy at or below its Doom threshold");

    // Exercise the existing Enchantment model used by Necrobinder's event path.
    CardModel exhaustCard = f.combat.CreateCard(CardId::GRAVEBLAST, f.player);
    require(SoulsPower::CanEnchant(exhaustCard), "SoulsPower rejected a locally Exhausting card");
    (void)CardCmd::Enchant(EnchantmentId::SOULS_POWER, exhaustCard, Dec(1));
    require(!exhaustCard.Keywords().Contains(CardKeyword::Exhaust), "SoulsPower did not remove Exhaust");
}

void test_necrobinder_upgraded_drain_power_corrupted_multiplier() {
    CombatFixture f("NECRODRAINPOWERUPGRADE");
    CardModel drainPower = f.combat.CreateCard(CardId::DRAIN_POWER, f.player);
    drainPower.UpgradeInternal();
    require(drainPower.DynamicVars().Damage().BaseValue() == Dec(12) &&
                drainPower.DynamicVars().Cards().BaseValue() == Dec(3),
            "Drain Power+ did not carry its two source upgrade deltas");
    require(Corrupted::CanEnchant(drainPower), "Corrupted rejected the Necrobinder-pool Drain Power attack");
    (void)CardCmd::Enchant(EnchantmentId::CORRUPTED, drainPower, Dec(1));
    require(drainPower.Enchantment().EnchantDamageMultiplicative(Dec(12), ValueProp::Move) == Dec::lit(15, 1),
            "Corrupted did not apply its source 1.5x powered-attack multiplier to Drain Power+");
    require(drainPower.Enchantment().EnchantDamageMultiplicative(Dec(12), ValueProp::Move | ValueProp::Unpowered) == Dec(1),
            "Corrupted modified unpowered Drain Power damage");
}

void test_necrobinder_doom_batch_and_end_turn_timing() {
    CombatFixture f("NECRODOOMBATCH");
    PlayerChoiceContext ctx;
    (void)RelicCmd::Obtain(RelicId::BOOK_REPAIR_KNIFE, f.player);
    f.player.Creature().SetCurrentHpInternal(f.player.Creature().CurrentHp() - 10);
    const int hpBeforeDoomBatch = f.player.Creature().CurrentHp();

    CreatureList enemies = f.combat.Enemies();
    require(enemies.size() == 2, "Doom batch test requires the two-enemy Cultists fight");
    for (Creature enemy : enemies) {
        PowerRef ref = alloc_power(*f.sim->g, PowerId::DOOM_POWER);
        PowerModel doom(f.sim, ref);
        doom.st().owner = enemy.Index();
        doom.st().amount = enemy.CurrentHp();
        enemy.ApplyPowerInternal(doom);
    }

    Hook::BeforeSideTurnEnd(f.combat, CombatSide::Enemy, enemies);
    require(enemies[0].IsDead() && enemies[1].IsDead(),
            "Doom did not kill all threshold-matching creatures in one enemy-side batch");
    require(f.player.Creature().CurrentHp() == hpBeforeDoomBatch + 6,
            "AfterDiedToDoom did not run once for the complete two-creature batch");

    Hook::AfterSideTurnEnd(f.combat, CombatSide::Enemy, enemies);
    require(f.player.Creature().CurrentHp() == hpBeforeDoomBatch + 6,
            "Doom retriggered for an already-resolved batch after enemy-side turn end");
}

void test_necrobinder_choice_prefix_snapshot_restore() {
    CombatFixture f("NECROCHOICESNAP");
    PlayerCombatState pcs = f.player.PlayerCombatState();
    pcs.Hand().Clear();
    pcs.SetEnergy(3);
    CardModel selectedA = f.combat.CreateCard(CardId::STRIKE_NECROBINDER, f.player);
    CardModel selectedB = f.combat.CreateCard(CardId::DEFEND_NECROBINDER, f.player);
    CardModel transfigure = f.combat.CreateCard(CardId::TRANSFIGURE, f.player);
    CardPileCmd::Add(CardList{selectedA, selectedB, transfigure}, PileType::Hand);

    int32_t handIndex = pcs.Hand().Cards().index_of(transfigure);
    require(handIndex >= 0, "Transfigure is not in the source hand");
    auto playActions = f.env.LegalActions();
    auto play = std::find_if(playActions.begin(), playActions.end(), [handIndex](const Action& action) {
        return action.kind == Action::PlayCard && action.index == handIndex;
    });
    require(play != playActions.end(), "Transfigure is not a legal action");
    auto pending = f.env.Predict(*play);
    require(pending->decision().kind == DecisionKind::Choice, "Transfigure did not expose its one-card selection");
    const ChoiceRequest& countRequest = pending->decision().request;
    require(countRequest.min_select == 1 && countRequest.max_select == 1 && countRequest.option_count == 2,
            "Transfigure did not preserve the original fixed-one selection prefix and two-card candidate list");
    const std::string snapshot = pending->ExportSnapshotJson();
    const auto answers = pending->LegalActions();
    require(!answers.empty(), "Transfigure selection exposed no legal card choice");
    auto indexPending = pending->Predict(answers.front());
    require(indexPending->decision().kind == DecisionKind::Choice &&
                indexPending->decision().request.prompt == "index",
            "Transfigure did not request the card index after the fixed selection count");
    const std::string indexSnapshot = indexPending->ExportSnapshotJson();
    const auto indexAnswers = indexPending->LegalActions();
    require(indexAnswers.size() == 2, "Transfigure index choice did not expose both eligible cards");
    auto expected = indexPending->Predict(indexAnswers.front());
    auto restored = Env::ImportSnapshotJson(snapshot);
    require(restored->ExportSnapshotJson() == snapshot, "pending Necrobinder choice snapshot did not round-trip");
    auto restoredIndex = restored->Predict(answers.front());
    require(restoredIndex->ExportSnapshotJson() == indexSnapshot, "the restored count prefix changed the next choice");
    auto restoredResult = restoredIndex->Predict(indexAnswers.front());
    require(restoredResult->Hash() == expected->Hash(),
            "restored Necrobinder selection prefix produced a different successor");
    require(pending->ExportSnapshotJson() == snapshot, "answering a Necrobinder choice mutated its source snapshot");
    require(indexPending->ExportSnapshotJson() == indexSnapshot, "answering the card index mutated its source snapshot");
}

void test_necrobinder_snap_excludes_retained_hand_cards() {
    CombatFixture f("NECROSNAPFILTER");
    PlayerCombatState pcs = f.player.PlayerCombatState();
    pcs.Hand().Clear();
    pcs.SetEnergy(3);
    CardModel retained = f.combat.CreateCard(CardId::STRIKE_NECROBINDER, f.player);
    CardCmd::ApplyKeyword(retained, {CardKeyword::Retain});
    CardModel selectable = f.combat.CreateCard(CardId::DEFEND_NECROBINDER, f.player);
    CardModel selectableStrike = f.combat.CreateCard(CardId::STRIKE_NECROBINDER, f.player);
    CardModel snap = f.combat.CreateCard(CardId::SNAP, f.player);
    CardPileCmd::Add(CardList{retained, selectable, selectableStrike, snap}, PileType::Hand);

    auto actions = f.env.LegalActions();
    auto play = std::find_if(actions.begin(), actions.end(), [&](const Action& action) {
        return action.kind == Action::PlayCard && action.index == 3 &&
               action.target == f.combat.Enemies()[0].CombatId();
    });
    require(play != actions.end(), "Snap was not a legal attack action");
    auto pending = f.env.Predict(*play);
    require(pending->decision().kind == DecisionKind::Choice,
            "Snap did not expose its Retain selection");
    const ChoiceRequest& request = pending->decision().request;
    require(request.min_select == 1 && request.max_select == 1 && request.option_count == 2 &&
                request.cards.size() == 2 && request.cards[0] == selectable.Index() &&
                request.cards[1] == selectableStrike.Index(),
            "Snap offered a Retain card instead of filtering it from the hand candidates");
}

}  // namespace

int main() {
    test_necrobinder_roster_start_and_epoch_filters();
    test_necrobinder_relic_epoch_gates_and_acquisition();
    test_necrobinder_osty_generation_and_relic_hooks();
    test_necrobinder_upgraded_drain_power_corrupted_multiplier();
    test_necrobinder_doom_batch_and_end_turn_timing();
    test_necrobinder_choice_prefix_snapshot_restore();
    test_necrobinder_snap_excludes_retained_hand_cards();
}
