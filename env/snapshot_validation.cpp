#include "env/snapshot_internal.h"
#include "content/generated/tables.h"
#include "game/Models/MonsterModel.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"
#include <array>
#include <bit>
#include <set>
#include <sstream>
#include <utility>
#include <cstring>

namespace sts2::snapshot_detail {
namespace {
void require(bool condition, const char* reason) {
    if (!condition) throw GameError(std::string("snapshot state: ") + reason);
}
template<class T, uint16_t N> void reference(const Pool<T, N>& pool, int64_t ref, bool live = false) {
    require(ref >= 0 && ref <= pool.capacity(), "object reference outside pool");
    if (live) require(pool.live(static_cast<uint32_t>(ref)), "object reference is not live");
}
template<class T, uint16_t N, class F> void each(const Pool<T, N>& pool, F&& visit) {
    pool.visit_snapshot([](auto, auto) {}, [&](uint32_t ref, const T& value) { visit(ref, value); });
}
void rng(const Rng& value) {
    require((value.random.s0 | value.random.s1 | value.random.s2 | value.random.s3) != 0, "all-zero RNG state");
}
void map(const ActMap& value) {
    require(value.cols >= 0 && value.cols <= kMapWidth && value.rows >= 0 && value.rows <= kMaxMapRows, "map dimensions");
    if (value.points.empty()) return;
    auto ref = [&](int index) { require(index >= 0 && static_cast<size_t>(index) < value.points.size(), "map point reference"); };
    ref(value.start); ref(value.boss); if (value.second_boss != -1) ref(value.second_boss);
    for (int col = 0; col < kMapWidth; ++col) for (int row = 0; row < kMaxMapRows; ++row)
        if (value.grid[col][row] != -1) ref(value.grid[col][row]);
    value.start_points.for_each(ref);
    int spoils_quests = 0;
    for (const MapPoint& point : value.points) {
        point.children.for_each(ref); point.parents.for_each(ref);
        if (point.fur_coat_quest)
            require(point.type == MapPointType::Monster || point.type == MapPointType::Elite,
                    "Fur Coat marker on a non-combat map point");
        if (point.spoils_map_quest) {
            require(point.type == MapPointType::Treasure, "Spoils Map marker on a non-treasure map point");
            ++spoils_quests;
        }
    }
    require(spoils_quests <= 1, "multiple Spoils Map quest markers");
}
void profile(const UnlockProfileState& value) {
    require(value.number_of_runs >= 0, "negative profile run count");
    require((value.unlocked_epoch_bits & ~uint64_t{0x1ffffffffffffffULL}) == 0,
        "profile epoch bit outside catalog");
    require((value.encounters_seen_low & uint64_t{1}) == 0
        && (value.encounters_seen_high & ~uint64_t{0x7ffffffULL}) == 0,
        "profile encounter bit outside catalog");
    if (value.full_veteran_compatible) {
        require(value.number_of_runs > 0
            && value.unlocked_epoch_bits == 0x1ffffffffffffffULL,
            "full veteran profile facts inconsistent");
        // ActModel.ApplyDiscoveryOrderModifications only consults each act's
        // BossDiscoveryOrder, which is its three AllBossEncounters in v0.111.0.
        // Ordinary/event encounters need not have been seen to unlock content.
        for (ActId act : {ActId::OVERGROWTH, ActId::UNDERDOCKS, ActId::HIVE, ActId::GLORY})
            for (EncounterId boss : data(act).boss) {
                const auto id = static_cast<unsigned>(boss);
                const bool seen = id < 64 ? (value.encounters_seen_low & (uint64_t{1} << id)) != 0
                    : (value.encounters_seen_high & (uint64_t{1} << (id - 64))) != 0;
                require(seen, "full veteran profile discovery boss missing");
            }
    }
}
void ancient_progress(const AncientEventProgressState& value) {
    for (size_t event = 1; event < AncientEventProgressState::kEventIdCount; ++event)
        for (size_t character = 1; character < AncientEventProgressState::kCharacterIdCount; ++character)
            require(value.visits[event][character] >= 0, "negative ancient visit count");
}
void event_synchronizer(const EventSynchronizerState& value) {
    require(value.event_count <= 1, "event synchronizer event count");
    require(!value.has_player_vote || value.has_player_vote_slot,
        "event synchronizer vote without player slot");
    if (!value.has_player_vote) require(value.player_vote == 0,
        "event synchronizer stale player vote");
    if (value.event_count == 0) {
        require(value.canonical_event == EventId::NONE
            && !value.has_player_vote_slot && !value.has_player_vote
            && value.page_index == 0,
            "empty event synchronizer state");
    } else {
        require(value.canonical_event != EventId::NONE
            && value.has_player_vote_slot && !value.has_player_vote
            // ChooseOptionForSharedEvent advances the page before invoking
            // the option; BeforeExitingRoom preserves this synchronizer.
            && (value.page_index == 0 || data(value.canonical_event).is_shared),
            "live event synchronizer state");
    }
}
}  // namespace

void validate_game_state(const GameState& g) {
    require(g.next_affliction_uid != 0, "affliction UID counter exhausted");
    require(g.card_selector_mode == CardSelectorMode::Default ||
                g.card_selector_mode == CardSelectorMode::FirstN, "card selector mode");
    if (g.run.phase == RunPhase::Combat)
        require(g.combat.exists && g.player_combat.exists, "combat phase without combat state");
    require(!g.combat.is_in_progress || (g.combat.exists && g.player_combat.exists), "active combat without combat state");
    require(std::memchr(g.run.seed, 0, sizeof(g.run.seed)) != nullptr, "unterminated seed");
    require(g.run.current_act_index >= 0 && static_cast<size_t>(g.run.current_act_index) < g.run.acts.size(), "act index");
    require(g.run.ascension >= 0 && g.run.ascension <= 10, "ascension range");
    require(g.run.progress_character_wins >= 0, "negative character win count");
    require(!g.run.is_victory || g.run.phase == RunPhase::Ended, "victory without ended run");
    require(g.run.progress_wongo_points >= 0 && g.run.player.wongo_points_earned >= 0, "Wongo progress points");
    require(!g.combat.exists || g.combat.encounter != EncounterId::NONE, "missing encounter");
    const auto& history = g.combat;
    for (int32_t count : {history.cards_played_this_combat, history.player_attack_cards_this_turn,
            history.player_attack_cards_started_this_turn, history.player_attack_skill_cards_started_this_turn,
            history.player_cards_started_this_turn, history.player_all_cards_started_this_turn,
            history.player_card_block_entries_this_turn, history.player_cards_finished_this_turn,
            history.player_skill_cards_finished_this_turn, history.player_shiv_cards_finished_this_turn,
            history.player_cards_discarded_this_turn, history.player_cards_drawn_this_combat,
            history.player_cards_drawn_this_turn, history.player_status_cards_drawn_this_turn,
            history.player_zero_cost_attacks_started_this_turn, history.player_lightning_orbs_channeled_this_combat,
            history.player_cards_generated_this_combat, history.player_ethereal_cards_played_this_combat,
            history.player_damage_received_entries_this_combat, history.cards_exhausted_this_combat,
            history.player_bound_cards_afflicted_this_turn, history.player_cards_exhausted_this_turn})
        require(count >= 0, "negative summarized history count");
    // Actual SpendEnergy records only positive amounts; StarsModified's
    // Radiate filter also retains only positive entries. List.Count is int32.
    constexpr int64_t max_history_sum = int64_t{INT32_MAX} * INT32_MAX;
    require(history.player_energy_spent_this_turn >= 0 && history.player_energy_spent_this_turn <= max_history_sum
        && history.player_stars_gained_this_turn >= 0 && history.player_stars_gained_this_turn <= max_history_sum,
        "summarized resource history sum outside original range");
    require(g.combat.player_card_block_history_round >= -1 && g.combat.player_card_block_history_turn >= -1,
            "invalid card block history phase");
    require(g.combat.player_card_block_history_side == CombatSide::Player ||
            g.combat.player_card_block_history_side == CombatSide::Enemy, "invalid card block history side");
    require(g.run.player.character != CharacterId::NONE, "missing player character");
    profile(g.run.profile);
    profile(g.run.player.profile);
    ancient_progress(g.run.progress_ancient_stats);
    event_synchronizer(g.run.event_synchronizer);
    require(g.run.run_seed == g.run.rng.seed, "run RNG seed identity");
    require(g.run.fur_coat_act_index == -1 ||
                (g.run.fur_coat_act_index >= 0 &&
                 static_cast<size_t>(g.run.fur_coat_act_index) < g.run.acts.size()),
            "Fur Coat act index");
    require(g.run.fur_coat_coord_count <= 8, "Fur Coat coordinate count");
    if (!g.run.fur_coat_coords_set) require(g.run.fur_coat_coord_count == 0, "Fur Coat unset coordinates");
    require(g.run.player.max_potion_slots >= 0 && static_cast<size_t>(g.run.player.max_potion_slots) == g.run.player.potions.size(), "potion slot count");
    require(g.player_combat.orb_capacity >= 0 && g.player_combat.orb_capacity <= 10, "orb capacity");
    map(g.run.map);
    for (const ActState& act : g.run.acts) {
        require(act.id != ActId::NONE, "missing act model");
        require(act.rooms.events_visited >= 0 && static_cast<size_t>(act.rooms.events_visited) <= act.rooms.events.size(), "event cursor");
        require(act.rooms.normal_encounters_visited >= 0 && static_cast<size_t>(act.rooms.normal_encounters_visited) <= act.rooms.normal_encounters.size(), "encounter cursor");
        require(act.rooms.elite_encounters_visited >= 0 && static_cast<size_t>(act.rooms.elite_encounters_visited) <= act.rooms.elite_encounters.size(), "elite cursor");
    }
    for (const Rng& stream : g.run.rng.rngs) rng(stream);
    for (const Rng& stream : g.run.player.rng.rngs) rng(stream);
    if (g.combat.exists) rng(g.combat.encounter_rng);
    require((g.combat.player_last_nondupe_attack_card == 0) ==
                (g.combat.player_last_nondupe_attack_turn == -1),
            "last player Attack history pair");
    if (g.combat.player_last_nondupe_attack_card)
        reference(g.cards, g.combat.player_last_nondupe_attack_card, true);
    require((g.combat.player_previous_nondupe_attack_card == 0) ==
                (g.combat.player_previous_nondupe_attack_turn == -1),
            "previous player Attack history pair");
    if (g.combat.player_previous_nondupe_attack_card) {
        reference(g.cards, g.combat.player_previous_nondupe_attack_card, true);
        require(g.combat.player_previous_nondupe_attack_turn < g.combat.player_last_nondupe_attack_turn,
                "player Attack history turn ordering");
    }
    // These are the original per-power Dictionary<CardModel,int> entries
    // retained across an unfinished card play. They are strong references,
    // and each dictionary admits only one entry per card object.
    std::set<std::pair<PowerRef, CardRef>> capture_pairs;
    for (const auto& capture : g.combat.active_card_play_power_captures) {
        require(capture.power != 0 && capture.card != 0, "null CardPlay Power capture reference");
        reference(g.powers, capture.power, true);
        reference(g.cards, capture.card, true);
        require(capture_pairs.emplace(capture.power, capture.card).second,
                "duplicate CardPlay Power capture pair");
    }
    std::set<AfflictionRef> attached_afflictions;
    std::set<uint64_t> affliction_uids;
    for (uint32_t ref = 1; ref <= g.cards.capacity(); ++ref) {
        if (!g.cards.live(ref)) continue;
        const AfflictionRef affliction = g.cards[ref].affliction_instance;
        if (!affliction) continue;
        reference(g.afflictions, affliction, true);
        const auto& value = g.afflictions[affliction];
        require(!value.pending && value.card == ref && value.id == g.cards[ref].affliction
                    && value.amount == g.cards[ref].affliction_amount,
                "card/affliction identity");
        require(attached_afflictions.insert(affliction).second, "affliction attached to multiple cards");
    }
    for (uint32_t ref = 1; ref <= g.afflictions.capacity(); ++ref) {
        if (!g.afflictions.live(ref)) continue;
        const auto& value = g.afflictions[ref];
        require(value.id != AfflictionId::NONE, "affliction object has no identity");
        require(value.uid != 0 && value.uid < g.next_affliction_uid && affliction_uids.insert(value.uid).second,
                "affliction object UID is null or duplicated");
        if (value.pending) require(value.card == 0, "pending affliction has a card");
        else if (value.card != 0 && !attached_afflictions.contains(ref)) {
            std::ostringstream detail;
            detail << "snapshot state: attached affliction has no card root (affliction_ref=" << ref
                   << ", card_ref=" << value.card << ", uid=" << value.uid;
            if (value.card <= g.cards.capacity() && g.cards.live(value.card)) {
                const auto& card = g.cards[value.card];
                detail << ", card_affliction_instance=" << card.affliction_instance
                       << ", card_affliction_id=" << static_cast<uint16_t>(card.affliction)
                       << ", card_removed=" << card.removed;
            }
            detail << ')';
            throw GameError(detail.str());
        }
    }
    reference(g.creatures, g.run.player.creature, true);
    require(g.creatures[g.run.player.creature].player == 1, "player creature identity");

    std::set<CardRef> piles;
    const PileType types[] = {PileType::Hand, PileType::Draw, PileType::Discard, PileType::Exhaust, PileType::Play};
    for (size_t i = 0; i < 5; ++i) for (CardRef ref : g.player_combat.piles[i]) {
        reference(g.cards, ref, true);
        require(piles.insert(ref).second && g.cards[ref].pile == types[i] && g.cards[ref].owner == 1, "combat pile membership");
    }
    std::set<CardRef> deck;
    for (CardRef ref : g.run.player.deck) {
        reference(g.cards, ref, true);
        require(deck.insert(ref).second && !piles.contains(ref) && g.cards[ref].pile == PileType::Deck && g.cards[ref].owner == 1, "deck membership");
    }
    for (RelicRef ref : g.run.player.relics) reference(g.relics, ref, true);
    for (PotionRef ref : g.run.player.potions) if (ref) reference(g.potions, ref, true);
    for (CreatureRef ref : g.player_combat.pets) reference(g.creatures, ref, true);
    for (OrbRef ref : g.player_combat.orbs) reference(g.orbs, ref, true);
    std::set<CreatureRef> roster;
    std::set<int32_t> combat_ids;
    auto roster_refs = [&](const auto& list) {
        for (CreatureRef ref : list) {
            reference(g.creatures, ref, true);
            require(roster.insert(ref).second, "duplicate creature roster reference");
            const auto& creature = g.creatures[ref];
            require(creature.combat_id >= 0 && creature.combat_id < g.combat.next_creature_id && combat_ids.insert(creature.combat_id).second,
                "invalid or duplicate combat ID");
        }
    };
    roster_refs(g.combat.allies); roster_refs(g.combat.enemies); roster_refs(g.combat.escaped);
    each(g.cards, [&](auto, const CardState& card) {
        require(card.id != CardId::NONE && card.owner <= 1, "card identity or owner");
        for (const auto& cost : card.temporary_star_costs) require(cost.cost >= 0, "temporary star cost");
        reference(g.cards, card.clone_of); reference(g.cards, card.deck_version); reference(g.creatures, card.current_target);
        reference(g.relics, card.bing_bong_skip_relic);
        require(!card.bing_bong_skip_relic || g.relics[card.bing_bong_skip_relic].id == RelicId::BING_BONG,
                "card BingBong membership points to a different relic model");
        for (const VarOverride& value : card.var_overrides) require(value.index < data(card.id).vars.size(), "card dynamic variable index");
    });
    each(g.creatures, [&](auto ref, const CreatureState& creature) {
        require(creature.player <= 1 && creature.pet_owner <= 1, "creature player owner");
        require(creature.attacks_this_turn >= 0, "creature attack history count");
        std::set<CreatureRef> damage_dealers;
        for (const auto& entry : creature.powered_damage_this_turn) {
            // History can refer to a retired dealer; it must still name a slot.
            reference(g.creatures, entry.dealer);
            require(entry.dealer && entry.count > 0 && damage_dealers.insert(entry.dealer).second,
                    "creature powered damage history");
        }
        require(creature.hp_display == HpDisplay::Normal || creature.hp_display == HpDisplay::InfiniteWithNumbers
            || creature.hp_display == HpDisplay::InfiniteWithoutNumbers, "creature HP display");
        for (PowerRef power : creature.powers) {
            reference(g.powers, power, true); require(g.powers[power].owner == ref, "power owner mismatch");
        }
        if (creature.monster.id == MonsterId::NONE) return;
        rng(creature.monster.rng);
        require(creature.slot >= -1 && (creature.slot == -1 || static_cast<size_t>(creature.slot) < data(g.combat.encounter).slots.size()), "monster slot");
        const auto& machine = creature.monster.machine;
        const auto& next = creature.monster.next_move;
        if (!machine.exists) {
            // ResetStateMachine nulls the native machine but preserves NextMove.
            // The pool therefore retains its registered layout after removal;
            // an active roster monster normally owns an actual machine. A
            // combat-layout event first creates its roster for presentation;
            // SetUpCombat builds the machines only after the Fight option.
            const bool staged_event_layout = g.combat.precreated_for_event_layout
                && !g.combat.is_in_progress
                && (g.run.phase == RunPhase::Event || (g.run.is_victory && g.run.phase == RunPhase::Ended)
                    // A finished non-fighting combat-layout event retains its
                    // presentation roster until the next map move exits the
                    // EventRoom (for example TheLanternKey.ReturnTheKey).
                    || (g.run.phase == RunPhase::Map && g.run.events[0].is_finished
                        && g.run.events[0].cleanup_called && g.run.events[0].options.empty()))
                && !g.run.rooms.empty() && g.run.rooms.back() == RoomType::Event
                && g.run.room_event == g.run.event_synchronizer.canonical_event
                && data(g.run.room_event).layout == EventLayoutType::Combat
                && machine.definition == 0 && next.state == -1 && !next.stunned
                && next.stun_move == 0 && next.stun_follow_up == -1 && !next.stun_performed;
            require(staged_event_layout || (!creature.attached && !g.combat.allies.contains(ref)
                && !g.combat.enemies.contains(ref)), "inactive monster machine attached to combat");
            if (machine.definition == 0) {
                require(next.state == -1, "missing monster machine");
                return;
            }
        }
        require(move_machine_matches_model(machine.definition, creature.monster.id), "monster machine registry binding");
        const MoveMachineDef& def = move_machine_def(machine.definition);
        require(!def.states.empty() && def.states.size() <= kMaxMonsterStates, "monster machine size");
        auto state = [&](int index) { require(index >= -1 && (index == -1 || static_cast<size_t>(index) < def.states.size()), "monster state index"); };
        if (machine.current == MonsterModel::stunnedStateIndex || next.stunned) {
            // Creature.StunInternal constructs a separate synthetic MoveState,
            // never a registry index. Its log/follow-up still use normal states.
            require(machine.current == MonsterModel::stunnedStateIndex
                && next.stunned && next.state == -1, "inconsistent stunned monster state");
        } else {
            state(machine.current);
        }
        state(next.state); state(next.stun_follow_up);
        const auto& history = machine.log;
        uint32_t move_mask = 0;
        for (size_t i = 0; i < def.states.size(); ++i)
            if (def.states[i].kind == MonsterStateKind::Move) move_mask |= uint32_t{1} << i;
        require(history.seen_moves == (history.seen & move_mask), "monster history summary");
        require(history.count <= 0x7fffffffu && history.move_count <= history.count,
            "monster history summary");
        require((history.seen >> def.states.size()) == 0
            && (history.seen_moves & ~history.seen) == 0
            && static_cast<uint32_t>(std::popcount(history.seen)) <= history.count
            && static_cast<uint32_t>(std::popcount(history.seen_moves)) <= history.move_count,
            "monster history summary");
        require((history.seen == 0) == (history.count == 0)
            && (history.seen_moves == 0) == (history.move_count == 0), "monster history summary");
        if (history.empty()) {
            require(history.last == -1 && history.consecutive == 0, "monster history summary");
        } else {
            state(history.last);
            require(history.last >= 0 && history.Contains(history.last)
                && history.consecutive > 0 && history.consecutive <= history.count,
                "monster history summary");
            if (def.states[history.last].kind == MonsterStateKind::Move) {
                require((history.seen_moves & (uint32_t{1} << history.last)) != 0
                    && history.move_age[history.last] == 0
                    && history.consecutive <= history.move_count, "monster history summary");
            } else {
                require(history.consecutive <= history.count - history.move_count,
                    "monster history summary");
            }
        }
        bool has_last_move = false;
        for (int i = 0; i < kMaxMonsterStates; ++i) {
            if ((history.seen_moves & (uint32_t{1} << i)) == 0) {
                require(history.move_age[i] == 0, "monster history summary");
                continue;
            }
            require(def.states[i].kind == MonsterStateKind::Move
                && history.move_age[i] < history.move_count, "monster history summary");
            has_last_move |= history.move_age[i] == 0;
            for (int j = 0; j < i; ++j)
                if ((history.seen_moves & (uint32_t{1} << j)) != 0)
                    require(history.move_age[i] != history.move_age[j], "monster history summary");
            if (history.last >= 0 && def.states[history.last].kind == MonsterStateKind::Move
                && i != history.last)
                require(history.move_age[i] >= history.consecutive, "monster history summary");
        }
        require(has_last_move == (history.move_count > 0), "monster history summary");
        for (size_t i = 0; i < def.states.size(); ++i) state(machine.follow_up[i]);
        require((machine.performed_at_least_once >> def.states.size()) == 0, "monster performed-state mask");
        if (next.state >= 0) require(def.states[next.state].kind == MonsterStateKind::Move, "next monster state is not a move");
        require(next.stun_move == 0 || registered_move_fn(next.stun_move) != nullptr, "unknown stun callback");
    });
    each(g.powers, [&](auto, const PowerState& power) {
        reference(g.creatures, power.owner); reference(g.creatures, power.applier); reference(g.creatures, power.target);
        require(power.id == PowerId::DAMPEN_POWER || (power.dampen_casters.empty() && power.dampen_downgraded_cards.empty()),
                "non-Dampen power contains Dampen state");
        std::set<CreatureRef> casters;
        for (CreatureRef caster : power.dampen_casters) {
            reference(g.creatures, caster, true);
            require(caster && casters.insert(caster).second, "Dampen caster identity");
        }
        std::set<CardRef> downgraded;
        for (const auto& entry : power.dampen_downgraded_cards) {
            reference(g.cards, entry.card, true);
            require(entry.card && entry.level > 0 && downgraded.insert(entry.card).second, "Dampen card identity");
        }
        if (power.id == PowerId::CURL_UP_POWER || power.id == PowerId::SWIPE_POWER) reference(g.cards, power.extra[0]);
        if (power.id == PowerId::GIGANTIFICATION_POWER) reference(g.cards, power.extra[1]);
    });
    each(g.relics, [&](auto, const RelicState& relic) {
        require(relic.owner <= 1, "relic owner");
        for (CardRef card : relic.paels_tooth_cards) reference(g.cards, card, true);
        require(relic.id == RelicId::PAELS_TOOTH || relic.paels_tooth_cards.empty(),
                "non-Tooth relic contains stored cards");
        if (relic.id == RelicId::UNSETTLING_LAMP) { reference(g.cards, relic.extra[0]); reference(g.powers, relic.extra[1]); }
        if (relic.id == RelicId::VAMBRACE) reference(g.cards, relic.extra[0]);
        if (relic.id == RelicId::PEN_NIB) reference(g.cards, relic.extra[1]);
        if (relic.id == RelicId::ARCHAIC_TOOTH) {
            require((relic.extra[0] == 0) == (relic.extra[1] == 0), "Archaic Tooth saved card pair");
            reference(g.cards, relic.extra[0]);
            reference(g.cards, relic.extra[1]);
        }
        if (relic.id == RelicId::TOUCH_OF_OROBAS) {
            for (int index : {0, 1})
                require(relic.extra[index] >= 0 && relic.extra[index] < static_cast<int32_t>(RelicId::Count),
                        "Touch of Orobas saved relic id");
        }
        if (relic.id == RelicId::SEA_GLASS)
            require(relic.extra[0] >= 0 && relic.extra[0] < static_cast<int32_t>(CharacterId::Count),
                    "Sea Glass saved character id");
        if (relic.id == RelicId::WONGOS_MYSTERY_TICKET)
            require(relic.extra[0] >= 0 && (relic.extra[1] == 0 || relic.extra[1] == 1),
                    "Wongo mystery ticket saved state");
        if (relic.id == RelicId::PAELS_WING)
            require(relic.extra[0] >= 0, "Pael's Wing sacrifice counter");
    });
    each(g.potions, [&](auto, const PotionState& potion) { require(potion.owner <= 1, "potion owner"); });
    each(g.orbs, [&](auto, const OrbState& orb) { require(orb.owner <= 1, "orb owner"); });
    each(g.rewards, [&](auto, const RewardState& reward) {
        reference(g.potions, reward.potion); reference(g.relics, reward.relic);
        for (CardRef ref : reward.cards) reference(g.cards, ref);
    });
    for (RewardRef ref : g.run.rewards.rewards) reference(g.rewards, ref);
    for (RewardRef ref : g.run.combat_extra_rewards) reference(g.rewards, ref);
    for (const EventState& event : g.run.events) {
        for (CardRef ref : event.slippery_skipped_removals) reference(g.cards, ref);
        for (const EventOptionState& option : event.options) reference(g.relics, option.relic);
        if (event.id == EventId::CRYSTAL_SPHERE) {
            require(event.extra[0] == 0 || event.extra[0] == 1, "Crystal Sphere active flag");
            if (event.extra[0] != 0) {
                require(event.crystal_sphere_cells.size() == 121, "Crystal Sphere board size");
                require((event.extra[1] == 1 || event.extra[1] == 2)
                    && event.extra[2] > 0 && event.extra[2] <= 6, "Crystal Sphere tool or divinations");
            }
            require(event.extra[3] >= 0 && event.extra[3] < (1 << 15), "Crystal Sphere revealed mask");
            int revealed_mask = 0;
            for (uint8_t index : event.crystal_sphere_revealed_order) {
                require(index < 15 && (revealed_mask & (1 << index)) == 0,
                    "Crystal Sphere revealed item order");
                revealed_mask |= 1 << index;
            }
            require(revealed_mask == event.extra[3], "Crystal Sphere revealed mask mismatch");
            for (uint8_t value : event.crystal_sphere_cells)
                require(value == 0 || value == 1 || (value >= 2 && value <= 16)
                    || (value >= 18 && value <= 32), "Crystal Sphere cell value");
        }
        if (event.id == EventId::WELCOME_TO_WONGOS)
            require(event.extra[0] >= 0 && event.extra[0] < static_cast<int32_t>(RelicId::Count),
                    "Wongo featured relic id");
        if (event.id == EventId::RANWID_THE_ELDER && event.options.size() == 3) {
            const auto& potion = event.options[0];
            const auto& relic = event.options[2];
            reference(g.potions, potion.arg);
            reference(g.relics, relic.arg);
            require((potion.arg == 0) == potion.is_locked && (relic.arg == 0) == relic.is_locked,
                    "Ranwid offer reference or lock state");
        }
    }
    for (const auto& entry : g.run.merchant.character_cards) reference(g.cards, entry.card);
    for (const auto& entry : g.run.merchant.colorless_cards) reference(g.cards, entry.card);
    for (const auto& entry : g.run.merchant.relics) reference(g.relics, entry.relic);
    for (const auto& entry : g.run.merchant.potions) reference(g.potions, entry.potion);
}
}  // namespace sts2::snapshot_detail
