#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "env/env.h"
#include "env/snapshot_json.h"
#include "engine/sim/state_fields.generated.h"
#include "game/All.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Runs/RunFlow.h"
#include "game/Runs/RunManager.h"

using namespace sts2;

static void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

static void reject_snapshot(const Env& source, const char* reason) {
    try {
        Env::ImportSnapshotJson(source.ExportSnapshotJson());
        throw std::runtime_error("invalid runtime state accepted");
    } catch (const GameError& error) {
        require(std::string(error.what()).find(reason) != std::string::npos,
            "runtime state rejected for an unrelated reason");
    }
}

static void veteran_profile_facts() {
    Env env;
    env.ResetCombat({.seed = "SNAPPROFILE", .character = CharacterId::IRONCLAD},
        EncounterId::CULTISTS_NORMAL);
    UnlockProfileState profile;
    profile.number_of_runs = 46;
    profile.encounters_seen_low = profile.encounters_seen_high = 0;
    // Game ActModel.ApplyDiscoveryOrderModifications reads only these bosses.
    for (ActId act : {ActId::OVERGROWTH, ActId::UNDERDOCKS, ActId::HIVE, ActId::GLORY})
        for (EncounterId boss : data(act).boss) {
            const auto id = static_cast<unsigned>(boss);
            if (id < 64) profile.encounters_seen_low |= uint64_t{1} << id;
            else profile.encounters_seen_high |= uint64_t{1} << (id - 64);
        }
    env.state().run.profile = env.state().run.player.profile = profile;
    auto restored = Env::ImportSnapshotJson(env.ExportSnapshotJson());
    require(restored->Hash() == env.Hash(), "real veteran profile facts not preserved");
    require(restored->Predict({.kind = Action::EndTurn})->Hash()
        == env.Predict({.kind = Action::EndTurn})->Hash(), "veteran profile transition restore differs");

    auto invalid = env.Clone();
    invalid->state().run.profile.number_of_runs = 0;
    reject_snapshot(*invalid, "full veteran profile facts inconsistent");
    invalid->state().run.profile = profile;
    invalid->state().run.profile.unlocked_epoch_bits &= ~uint64_t{1};
    reject_snapshot(*invalid, "full veteran profile facts inconsistent");
    invalid->state().run.profile = profile;
    const auto boss = static_cast<unsigned>(EncounterId::VANTOM_BOSS);
    if (boss < 64) invalid->state().run.profile.encounters_seen_low &= ~(uint64_t{1} << (boss & 63));
    else invalid->state().run.profile.encounters_seen_high &= ~(uint64_t{1} << (boss & 63));
    reject_snapshot(*invalid, "full veteran profile discovery boss missing");
    invalid->state().run.profile = profile;
    invalid->state().run.player.profile.full_veteran_compatible = false;
    reject_snapshot(*invalid, "only fully unlocked veteran profiles");
}

static void runtime_stun_and_shared_pages() {
    Env combat;
    combat.ResetCombat({.seed = "SNAPSTUN", .character = CharacterId::IRONCLAD},
        EncounterId::CULTISTS_NORMAL);
    Creature enemy = CombatState(&combat.sim()).Enemies().front();
    CreatureCmd::Stun(enemy);
    require(enemy.Monster().NextMove().IsStunned(), "fixture failed to stun a monster");
    auto restored = Env::ImportSnapshotJson(combat.ExportSnapshotJson());
    require(restored->Hash() == combat.Hash() && restored->LegalActions() == combat.LegalActions(),
        "synthetic stun state restore differs");
    require(restored->Predict({.kind = Action::EndTurn})->Hash()
        == combat.Predict({.kind = Action::EndTurn})->Hash(), "restored stun transition differs");

    auto invalid = combat.Clone();
    auto& monster = invalid->state().creatures[enemy.Index()].monster;
    monster.next_move.stunned = false;
    reject_snapshot(*invalid, "inconsistent stunned monster state");
    monster.next_move.stunned = true;
    monster.machine.current = -3;
    reject_snapshot(*invalid, "inconsistent stunned monster state");
    monster.machine.current = MonsterModel::stunnedStateIndex;
    monster.machine.log.last = MonsterModel::stunnedStateIndex;
    reject_snapshot(*invalid, "monster state index");

    invalid = combat.Clone();
    auto& history = invalid->state().creatures[enemy.Index()].monster.machine.log;
    history.seen |= uint32_t{1} << kMaxMonsterStates;
    reject_snapshot(*invalid, "monster history summary");
    history = combat.state().creatures[enemy.Index()].monster.machine.log;
    history.consecutive = history.count + 1;
    reject_snapshot(*invalid, "monster history summary");
    history = combat.state().creatures[enemy.Index()].monster.machine.log;
    history.move_count = history.count + 1;
    reject_snapshot(*invalid, "monster history summary");
    history = combat.state().creatures[enemy.Index()].monster.machine.log;
    history.move_age[history.last] = history.move_count;
    reject_snapshot(*invalid, "monster history summary");
    history = combat.state().creatures[enemy.Index()].monster.machine.log;
    const auto& machine_def = enemy.Monster().MachineDef();
    int omitted_move = -1;
    for (size_t i = 0; i < machine_def.states.size(); ++i)
        if (machine_def.states[i].kind == MonsterStateKind::Move && static_cast<int>(i) != history.last) {
            omitted_move = static_cast<int>(i);
            break;
        }
    require(omitted_move >= 0, "history fixture needs two Move states");
    const int8_t original_last = history.last;
    history.Append(static_cast<int8_t>(omitted_move), true);
    history.Append(original_last, true);
    // Omitting a seen Move would change its cooldown after restoration, even
    // though the count, last state and remaining ages still look consistent.
    history.seen_moves &= ~(uint32_t{1} << omitted_move);
    history.move_age[omitted_move] = 0;
    reject_snapshot(*invalid, "monster history summary");

    Env shared;
    new_run(shared.state(), {.seed = "SNAPSHARED", .character = CharacterId::IRONCLAD});
    shared.state().run.started_with_neow = false;
    shared.Step({.kind = Action::StartRun});
    shared.state().run.total_floor = 6;
    // Select a deterministic event through the ordinary map/action path.
    // The fixture substitutes its event model at the ancient map point;
    // the behavior under test is shared option progression and restoration.
    shared.state().run.acts[0].rooms.ancient = EventId::PUNCH_OFF;
    const Action move = shared.LegalActions().front();
    const int point = shared.state().run.map.get_point(move.coord.col, move.coord.row);
    require(point >= 0, "shared event fixture has no map point");
    shared.state().run.map.points[point].type = MapPointType::Ancient;
    shared.Step(move);
    auto restored_layout = Env::ImportSnapshotJson(shared.ExportSnapshotJson());
    require(restored_layout->Hash() == shared.Hash()
        && restored_layout->LegalActions() == shared.LegalActions(),
        "combat-layout event before Fight failed to restore");
    auto bad_layout = shared.Clone();
    bad_layout->state().combat.precreated_for_event_layout = false;
    reject_snapshot(*bad_layout, "inactive monster machine attached to combat");
    auto page = shared.Predict({.kind = Action::EventOption, .index = 1});
    require(page->state().run.event_synchronizer.page_index == 1,
        "shared event option did not advance its page");
    auto restored_page = Env::ImportSnapshotJson(page->ExportSnapshotJson());
    require(restored_page->Hash() == page->Hash() && restored_page->LegalActions() == page->LegalActions(),
        "shared event page restore differs");
    require(restored_page->Predict({.kind = Action::EventOption, .index = 0})->Hash()
        == page->Predict({.kind = Action::EventOption, .index = 0})->Hash(),
        "restored shared event option differs");

    auto bad_page = page->Clone();
    bad_page->state().run.event_synchronizer.canonical_event = EventId::HUNGRY_FOR_MUSHROOMS;
    reject_snapshot(*bad_page, "live event synchronizer state");
    bad_page->state().run.event_synchronizer.canonical_event = EventId::PUNCH_OFF;
    bad_page->state().run.event_synchronizer.event_count = 0;
    reject_snapshot(*bad_page, "empty event synchronizer state");

    // TheLanternKey.ReturnTheKey ends the event without starting its staged
    // encounter. EventRoom.Exit is deferred until the following map move.
    Env lantern;
    new_run(lantern.state(), {.seed = "SNAPLANTERN", .character = CharacterId::IRONCLAD});
    lantern.state().run.started_with_neow = false;
    lantern.Step({.kind = Action::StartRun});
    lantern.state().run.acts[0].rooms.ancient = EventId::THE_LANTERN_KEY;
    const Action lantern_move = lantern.LegalActions().front();
    const int lantern_point = lantern.state().run.map.get_point(lantern_move.coord.col, lantern_move.coord.row);
    lantern.state().run.map.points[lantern_point].type = MapPointType::Ancient;
    lantern.Step(lantern_move);
    lantern.Step({.kind = Action::EventOption, .index = 0});
    require(lantern.state().run.phase == RunPhase::Map && lantern.state().combat.precreated_for_event_layout,
        "ReturnTheKey fixture did not retain the staged event roster at the map decision");
    auto restored_lantern = Env::ImportSnapshotJson(lantern.ExportSnapshotJson());
    require(restored_lantern->Hash() == lantern.Hash() && restored_lantern->LegalActions() == lantern.LegalActions(),
        "finished non-fighting event roster failed to restore");
    const Action following_move = lantern.LegalActions().front();
    require(restored_lantern->Predict(following_move)->Hash() == lantern.Predict(following_move)->Hash(),
        "restored LanternKey room exit differs");
    auto unfinished_map = lantern.Clone();
    unfinished_map->state().run.events[0].is_finished = false;
    reject_snapshot(*unfinished_map, "inactive monster machine attached to combat");
}

int main(int argc, char** argv) {
    veteran_profile_facts();
    runtime_stun_and_shared_pages();
    Env env;
    env.ResetCombat(NewRunOptions{.seed = "DUAL001", .character = CharacterId::IRONCLAD},
                    EncounterId::CULTISTS_NORMAL);
    const OrbId snapshot_orb_ids[] = {
        OrbId::LIGHTNING_ORB, OrbId::FROST_ORB, OrbId::DARK_ORB,
        OrbId::PLASMA_ORB, OrbId::GLASS_ORB,
    };
    env.state().player_combat.orb_capacity = 5;
    for (size_t i = 0; i < sizeof(snapshot_orb_ids) / sizeof(snapshot_orb_ids[0]); ++i) {
        OrbRef ref = env.state().orbs.alloc();
        OrbState& orb = env.state().orbs[ref];
        orb.id = snapshot_orb_ids[i];
        orb.owner = 1;
        orb.dark_evoke_val = Dec(static_cast<int32_t>(10 + i));
        orb.glass_passive_val = Dec(static_cast<int32_t>(20 + i));
        env.state().player_combat.orbs.push_back(ref);
    }
    // Exact scalar encodings must survive managed JSON readers. In particular
    // ulong cannot round-trip through double, and -0/decimal scale are observable.
    env.state().run.rng.rngs[0].random.s0 = std::numeric_limits<uint64_t>::max();
    env.state().combat.gold_proportion = -0.0f;
    env.state().combat.player_cards_finished_this_turn = 7;
    env.state().combat.player_cards_started_this_turn = 6;
    env.state().combat.player_skill_cards_finished_this_turn = 8;
    env.state().combat.player_shiv_cards_finished_this_turn = 3;
    env.state().combat.player_cards_discarded_this_turn = 9;
    env.state().combat.player_cards_drawn_this_combat = 10;
    env.state().combat.player_energy_spent_this_turn = 4;
    env.state().combat.player_status_cards_drawn_this_turn = 11;
    env.state().combat.player_zero_cost_attacks_started_this_turn = 12;
    env.state().combat.player_lightning_orbs_channeled_this_combat = 5;
    env.state().combat.player_cards_generated_this_combat = 13;
    env.state().creatures[2].RecordPoweredDamage(1, 14);
    env.state().combat.player_lost_hp_previous_turn = true;
    env.state().combat.player_nonfully_blocked_damage_previous_turn = true;
    CardRef card = env.state().player_combat.piles[0][0];
    env.state().cards[card].id = CardId::RAMPAGE;
    env.state().cards[card].growth_damage = Dec::lit(-12500, 4);
    env.state().cards[card].var_overrides.push_back(VarOverride{.value = Dec::lit(-123, 2), .index = 0});
    const auto before = env.Hash();
    const auto snapshot = env.ExportSnapshotJson();
    require(env.Hash() == before, "snapshot changed the source");
    require(snapshot == env.Clone()->ExportSnapshotJson(), "clone snapshot differs");
    require(snapshot.find(kStateSchemaSha256) != std::string::npos, "schema missing");
    // The last-list representation in f63ae51 cannot be imported as a
    // summary; reject its exact schema before reading any state fields.
    std::string previous_schema = snapshot;
    previous_schema.replace(previous_schema.find(kStateSchemaSha256), 64,
        "0a04cf9a4c3c980c9eedb47cb7f3fe8c8064987d348658d17ee9f32ff5cbf078");
    bool old_schema_rejected = false;
    try { Env::ImportSnapshotJson(previous_schema); }
    catch (const GameError& error) {
        old_schema_rejected = std::string(error.what()).find("incompatible schema_sha256") != std::string::npos;
    }
    require(old_schema_rejected, "previous StateLog list schema was accepted");
    require(snapshot.find("\"s0\":18446744073709551615") != std::string::npos, "ulong precision lost");
    require(snapshot.find("\"f32_bits\":2147483648") != std::string::npos, "float sign bit lost");
    require(snapshot.find("\"decimal_bits\":[123,0,0,2147614720]") != std::string::npos, "decimal bits lost");
    require(snapshot.find("\"growth_damage\":{\"decimal_bits\":[12500,0,0,2147745792]}") != std::string::npos,
        "card growth decimal sign/scale lost");
    auto imported = Env::ImportSnapshotJson(snapshot);
    require(imported->ExportSnapshotJson() == snapshot, "complete snapshot restore differs");
    require(imported->state().player_combat.orb_capacity == 5 &&
        imported->state().player_combat.orbs.size() == sizeof(snapshot_orb_ids) / sizeof(snapshot_orb_ids[0]),
        "orb capacity or queue length did not survive snapshot restore");
    for (size_t i = 0; i < sizeof(snapshot_orb_ids) / sizeof(snapshot_orb_ids[0]); ++i) {
        const OrbRef ref = imported->state().player_combat.orbs[i];
        const OrbState& orb = imported->state().orbs[ref];
        require(orb.id == snapshot_orb_ids[i] && orb.owner == 1 &&
            orb.dark_evoke_val == Dec(static_cast<int32_t>(10 + i)) &&
            orb.glass_passive_val == Dec(static_cast<int32_t>(20 + i)),
            "orb model state or queue order did not survive snapshot restore");
    }
    require(imported->state().combat.player_cards_finished_this_turn == 7 &&
        imported->state().combat.player_cards_started_this_turn == 6 &&
        imported->state().combat.player_skill_cards_finished_this_turn == 8 &&
        imported->state().combat.player_shiv_cards_finished_this_turn == 3 &&
        imported->state().combat.player_cards_discarded_this_turn == 9 &&
        imported->state().combat.player_cards_drawn_this_combat == 10 &&
        imported->state().combat.player_energy_spent_this_turn == 4 &&
        imported->state().combat.player_status_cards_drawn_this_turn == 11 &&
        imported->state().combat.player_zero_cost_attacks_started_this_turn == 12 &&
        imported->state().combat.player_lightning_orbs_channeled_this_combat == 5 &&
        imported->state().combat.player_cards_generated_this_combat == 13 &&
        imported->state().creatures[2].PoweredDamageFrom(1) == 14 &&
        imported->state().combat.player_lost_hp_previous_turn &&
        imported->state().combat.player_nonfully_blocked_damage_previous_turn,
        "Defect/shared history counters did not survive snapshot restore");
    require(imported->Predict(Action{.kind = Action::EndTurn})->Hash()
        == env.Predict(Action{.kind = Action::EndTurn})->Hash(), "restored EndTurn diverged");
    auto turn_two = env.Predict(Action{.kind = Action::EndTurn});
    auto restored_turn_two = Env::ImportSnapshotJson(turn_two->ExportSnapshotJson());
    require(restored_turn_two->Predict(Action{.kind = Action::EndTurn})->Hash()
        == turn_two->Predict(Action{.kind = Action::EndTurn})->Hash(), "later-turn restore diverged");

    // Fragmented storage must preserve the next allocation, not only currently
    // enumerated values. Free an unreferenced created card between two allocations.
    auto fragmented = env.Clone();
    CardModel temporary = CombatState(&fragmented->sim()).CreateCard(CardId::STRIKE_IRONCLAD, Player(&fragmented->sim(), 1));
    CombatState(&fragmented->sim()).CreateCard(CardId::STRIKE_IRONCLAD, Player(&fragmented->sim(), 1));
    fragmented->state().cards.free(temporary.Index());
    auto restored_fragmented = Env::ImportSnapshotJson(fragmented->ExportSnapshotJson());
    require(restored_fragmented->state().cards.alloc() == fragmented->state().cards.alloc(), "pool allocation order lost");

    DotnetSet<int8_t, 8> original_set, restored_set;
    for (int8_t value : {0, 1, 2}) original_set.add(value);
    original_set.remove(0); original_set.remove(2);
    original_set.visit_snapshot([&](int count, int free_list, int free_count) {
        restored_set.load_snapshot(count, free_list, free_count, [&](auto add) {
            original_set.visit_snapshot([](auto, auto, auto) {}, add);
        });
    }, [](auto, auto, auto, auto) {});
    original_set.add(7); restored_set.add(7);
    require(original_set.to_vec() == restored_set.to_vec(), "set future insertion order lost");
    try { restored_set.load_snapshot(1, 0, 1, [&](auto add) { add(0, false, 0, 0); }); throw std::runtime_error("cyclic free chain accepted"); }
    catch (const GameError&) { }
    auto unported = env.Clone(); unported->state().cards[card].id = CardId::BEACON_OF_HOPE;
    // Beacon of Hope is multiplayer-only; clear the unrelated fixture override.
    unported->state().cards[card].var_overrides.clear();
    try { Env::ImportSnapshotJson(unported->ExportSnapshotJson()); throw std::runtime_error("unported import accepted"); }
    catch (const Unported&) { require(env.Hash() == before, "unported import changed source"); }

    // The pending-choice current state is only partially executed; answering
    // needs the untouched base plus the pending action and answer prefix.
    Player player(&env.sim(), 1);
    CardModel discovery = CombatState(&env.sim()).CreateCard(CardId::DISCOVERY, player);
    CardPileCmd::Add(discovery, PileType::Hand);
    const int index = static_cast<int>(player.PlayerCombatState().Hand().Cards().size()) - 1;
    auto choice = env.Predict(Action{.kind = Action::PlayCard, .index = index});
    require(choice->decision().kind == DecisionKind::Choice, "fixture did not reach a choice");
    const auto choice_snapshot = choice->ExportSnapshotJson();
    require(choice_snapshot.find("\"replay_base\":{") != std::string::npos, "replay base missing");
    require(choice_snapshot.find("\"pending\":{\"kind\":2") != std::string::npos, "pending action missing");
    auto resolved = choice->Predict(choice->LegalActions().front());
    auto restored_choice = Env::ImportSnapshotJson(choice_snapshot);
    require(restored_choice->Hash() == choice->Hash(), "choice restore differs");
    require(restored_choice->Predict(choice->LegalActions().front())->Hash() == resolved->Hash(), "restored answer diverged");
    require(resolved->ExportSnapshotJson().find("\"replay_base\":null") != std::string::npos,
            "inactive replay base exported");
    require(choice_snapshot == choice->ExportSnapshotJson(), "answer changed source snapshot");

    // Optional fixtures are consumed by the managed checks to validate real
    // C++ choice documents and extreme numeric values, rather than hand-written JSON.
    if (argc == 2) {
        std::filesystem::create_directories(argv[1]);
        std::ofstream(std::filesystem::path(argv[1]) / "scalars.json") << snapshot;
        std::ofstream(std::filesystem::path(argv[1]) / "choice.json") << choice_snapshot;
    }
    std::cout << "Complete snapshot checks passed: exact scalars, owned restore, later turns, choice replay, storage order, unported rejection.\n";
}
