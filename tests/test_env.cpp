#include <cassert>
#include <cstdint>
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <utility>

#include "env/env.h"
#include "game/Commands/PotionCmd.h"
#include "engine/sim/errors.h"
#include "engine/sim/gc.h"
#include "engine/sim/state_hash.h"
#include "game/All.h"

using namespace sts2;

int main() {
    // An ordinary clone can discard obsolete replay storage. Its public
    // identity and subsequent legal execution must remain exactly equivalent,
    // including an EndTurn that creates a new stationary replay snapshot.
    for (auto character : {CharacterId::IRONCLAD, CharacterId::SILENT, CharacterId::REGENT,
                           CharacterId::NECROBINDER, CharacterId::DEFECT}) {
        Env root;
        root.ResetCombat({.seed = "CLONECOMPLETE", .character = character}, EncounterId::CULTISTS_NORMAL);
        auto current = root.Predict({.kind = Action::EndTurn});
        auto branch = current->Clone();
        if (current->Hash() != branch->Hash()
            || current->ExportSnapshotJson() != branch->ExportSnapshotJson()) return 40;
        for (int i = 0; i < 6; ++i) {
            const auto legal = current->LegalActions();
            if (legal != branch->LegalActions() || legal.empty()) return 41;
            auto action = legal.front();
            if (current->decision().kind == DecisionKind::Combat && i % 2)
                action = {.kind = Action::EndTurn};
            current = current->Predict(action);
            branch = branch->Predict(action);
            if (current->Hash() != branch->Hash()
                || current->ExportSnapshotJson() != branch->ExportSnapshotJson()) return 42;
        }
    }
    // Hook snapshots preserve source order after the inline 512-listener
    // budget is exceeded by generated statuses in a long combat.
    {
        HookListenerList listeners;
        for (uint16_t i = 1; i <= 520; ++i)
            listeners.push_back(AbstractModel(nullptr, ModelKind::Card, i));
        if (listeners.size() != 520 || listeners[511].Index() != 512 ||
            listeners[512].Index() != 513 || listeners[519].Index() != 520) return 30;
        HookListenerList moved = std::move(listeners);
        if (moved.size() != 520 || moved[512].Index() != 513) return 31;
    }
    // Long summoning combats retain active enemies, escaped creatures and
    // references from powers/cards while recycling detached monster slots.
    {
        auto game = std::make_unique<GameState>();
        game->combat.exists = true;
        for (int i = 0; i < 20; ++i) game->creatures.alloc();
        game->run.player.creature = 1;
        game->combat.enemies.push_back(2);
        game->combat.escaped.push_back(4);
        game->creatures[2].attached = true;
        game->creatures[2].monster.id = MonsterId::QUEEN;
        game->creatures[2].monster.extra[1] = 7;  // Queen._amalgam
        PowerRef retained = game->powers.alloc();
        game->creatures[2].powers.push_back(retained);
        game->powers[retained].applier = 3;
        CardRef targeting = game->cards.alloc();
        game->cards[targeting].current_target = 5;
        PowerRef target_power = game->powers.alloc();
        game->creatures[5].powers.push_back(target_power);
        game->powers[target_power].applier = 8;
        PowerRef detached_power = game->powers.alloc();
        game->creatures[6].powers.push_back(detached_power);
        // The collector reports all freed slots: 13 creatures and one power.
        if (CollectDetachedCreatures(*game) != 14 || game->creatures.size() != 7 ||
            !game->creatures.live(1) || !game->creatures.live(2) ||
            !game->creatures.live(3) || !game->creatures.live(4) ||
            !game->creatures.live(5) || !game->creatures.live(7) || !game->creatures.live(8) ||
            game->creatures.live(6) || !game->powers.live(retained) ||
            !game->powers.live(target_power) || game->powers.live(detached_power)) return 28;
        if (game->creatures.alloc() != 6) return 29;
    }
    // Captured from vanilla CombatState.CreateCreature by EnvLiveCardChecks,
    // DUAL001, A0, first enemy combat id 1. Null and negative map coordinates
    // are different roots; do not clamp a present negative column or row.
    struct MonsterRngVector { bool present; MapCoord coord; std::array<uint64_t, 4> words; };
    const MonsterRngVector monster_rng_vectors[] = {
        {false, {}, {6180656293325301155ULL, 14759289082765171192ULL, 2619033770157508384ULL, 10139190090342948186ULL}},
        {true, {0, -1}, {6468053039050415726ULL, 6340883114703917213ULL, 15760746111170511652ULL, 13713001174597005119ULL}},
        {true, {-3, 5}, {11478976370664498579ULL, 11927765981865739065ULL, 4925541940656317373ULL, 6829653272351862408ULL}},
    };
    for (const auto& vector : monster_rng_vectors) {
        auto game = std::make_unique<GameState>();
        new_run(*game, NewRunOptions{.seed = "DUAL001", .character = CharacterId::IRONCLAD});
        if (vector.present) game->run.visited_map_coords.push_back(vector.coord);
        Sim sim(*game);
        CombatState state(&sim);
        state.AddPlayer(Player(&sim, 1));
        MonsterModel monster = MonsterModel::ToMutable(&sim, MonsterId::CALCIFIED_CULTIST);
        Creature creature = state.CreateCreature(monster, CombatSide::Enemy, -1);
        const auto& rng = monster.Rng();
        if (creature.CombatId() != 1 || rng.counter != 0
            || std::array{rng.random.s0, rng.random.s1, rng.random.s2, rng.random.s3} != vector.words) return 27;
    }
    Env env;
    env.Reset(NewRunOptions{.seed = "M3ENVTEST", .character = CharacterId::IRONCLAD, .ascension = 0});
    if (env.decision().kind == DecisionKind::Ended) return 1;
    const auto first = env.LegalActions();
    if (first.empty()) return 2;
    const auto mask = env.ActionMask();
    if (mask.size() != Env::kActionMaskSize) return 9;
    size_t active = 0;
    for (uint8_t value : mask) active += value != 0;
    if (active != first.size()) return 10;
    const auto observation = env.Observe();
    if (observation.size() != Env::kObservationSize) return 3;
    const uint64_t before = env.Hash();

    auto copy = env.Clone();
    if (copy->Hash() != before || copy->Observe() != observation) return 4;
    copy->Step(first.front());
    if (copy->Hash() == before && copy->decision().kind == env.decision().kind) return 5;
    if (env.Hash() != before) return 6;  // clone mutation does not alias the source

    // AnyTime potions are exposed outside combat and consume their slot through
    // the same wrapper used by direct model tests.
    if (!PotionCmd::TryToProcure(PotionId::BLOOD_POTION, Player(&env.sim(), 1)).success) return 11;
    auto potion_actions = env.LegalActions();
    auto potion_it = std::find_if(potion_actions.begin(), potion_actions.end(), [](const Action& action) {
        return action.kind == Action::UsePotion;
    });
    if (potion_it == potion_actions.end()) return 12;
    env.Step(*potion_it);
    if (!Player(&env.sim(), 1).Potions().empty()) return 13;

    auto det = env.Clone();
    const auto visible = det->Observe();
    det->Determinize(123456789ULL);
    if (det->Observe() != visible || det->LegalActions() != env.LegalActions()) return 7;
    auto same = env.Clone();
    same->Determinize(123456789ULL);
    if (same->Hash() != det->Hash()) return 14;
    auto different = env.Clone();
    different->Determinize(987654321ULL);
    if (different->Observe() != visible || different->LegalActions() != env.LegalActions() || different->Hash() == det->Hash()) return 15;

    // The Env can enter a deterministic encounter directly while keeping the
    // normal Env::Reset run-level contract unchanged.
    Env combat;
    combat.ResetCombat(NewRunOptions{.seed = "M3COMBATTEST", .character = CharacterId::IRONCLAD, .ascension = 0},
                       EncounterId::CULTISTS_NORMAL);
    if (combat.decision().kind != DecisionKind::Combat) return 16;
    if (combat.state().run.phase != RunPhase::Combat) return 17;
    if (combat.LegalActions().empty()) return 18;
    combat.ValidateCombatRoot();
    // Rng has tail padding after its counter. Allocation/stack noise in that
    // padding cannot change a logical state hash or poison a transposition key.
    auto padded = combat.Clone();
    Rng& padded_rng = padded->state().run.rng.rngs[0];
    constexpr size_t rng_fields_end = offsetof(Rng, counter) + sizeof(int32_t);
    static_assert(rng_fields_end < sizeof(Rng));
    const uint64_t logical_hash = combat.Hash();
    std::memset(reinterpret_cast<unsigned char*>(&padded_rng) + rng_fields_end,
                0xa5, sizeof(Rng) - rng_fields_end);
    if (padded->Hash() != logical_hash) return 22;
    ++padded_rng.counter;
    if (padded->Hash() == logical_hash) return 23;

    auto inactive = combat.Clone();
    CardRef unused = inactive->state().cards.alloc();
    inactive->state().cards.free(unused);
    const uint64_t inactive_hash = inactive->Hash();
    inactive->state().cards[unused].id = CardId::SPLASH;
    auto& hand_storage = inactive->state().player_combat.piles[0];
    hand_storage.push_back(unused);
    hand_storage.erase_at(hand_storage.size() - 1);
    if (inactive->Hash() != inactive_hash) return 25;

    // Equal enumerated sets can have different future insertion orders. The
    // free-list order must survive hashing even though removed values do not.
    DotnetSet<uint8_t, 8> set_a, set_b;
    for (uint8_t i : {1, 2, 3}) { set_a.add(i); set_b.add(i); }
    set_a.remove(1); set_a.remove(2);
    set_b.remove(2); set_b.remove(1);
    StateHasher hash_a, hash_b;
    hash_a(set_a); hash_b(set_b);
    if (hash_a.finish() == hash_b.finish()) return 26;
    // MultiplayerOnly cards are outside the single-player model contract.
    // A root containing one is rejected before any search branch.
    const CardRef first_card = combat.state().player_combat.piles[0][0];
    combat.state().cards[first_card].id = CardId::BEACON_OF_HOPE;
    try {
        combat.ValidateCombatRoot();
        return 19;
    } catch (const Unported&) {
        // Expected; the source state can be discarded without searching.
    }
    // Prediction itself must reject an unported root, even when the selected
    // action would not immediately invoke that card. The source remains intact.
    const uint64_t unported_hash = combat.Hash();
    try {
        auto branch = combat.Predict(Action{.kind = Action::EndTurn});
        return 20;
    } catch (const Unported&) {
        if (combat.Hash() != unported_hash) return 21;
    }

    {
        // CreatureCmd.Kill ends the run when the player dies anywhere:
        // RunManager.OnEnded(isVictory: false). Room Full of Cheese's Search
        // deals its damage outside combat; at low HP the run must end there
        // instead of returning to the map with a dead player.
        Env env;
        GameState& state = env.state();
        new_run(state, NewRunOptions{.seed = "CHEESEDEATH", .character = CharacterId::SILENT, .ascension = 10});
        state.run.rooms.push_back(RoomType::Event);
        state.run.room_event = EventId::ROOM_FULL_OF_CHEESE;
        state.run.phase = RunPhase::Event;
        Player player(&env.sim(), 1);
        EventModel::ToMutable(&env.sim(), EventId::ROOM_FULL_OF_CHEESE).BeginEvent(player, false);
        state.creatures[state.run.player.creature].current_hp = 1;
        env.Step({.kind = Action::EventOption, .index = 1});
        if (!player.Creature().IsDead() || env.decision().kind != DecisionKind::Ended
            || state.run.is_victory || !env.LegalActions().empty()) return 41;
    }

    VectorEnv batch(2);
    const NewRunOptions batch_options[] = {
        NewRunOptions{.seed = "M3BATCHA", .character = CharacterId::IRONCLAD, .ascension = 0},
        NewRunOptions{.seed = "M3BATCHB", .character = CharacterId::IRONCLAD, .ascension = 10},
    };
    batch.Reset(batch_options);
    if (batch.size() != 2 || batch.Observe()[0].size() != Env::kObservationSize || batch.Hashes()[0] == batch.Hashes()[1]) return 8;
    return 0;
}
