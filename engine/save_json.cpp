#include "engine/save_json.h"

#include <algorithm>

namespace sts2 {

std::string model_id(CardId id) { return "CARD." + std::string(name(id)); }
std::string model_id(RelicId id) { return "RELIC." + std::string(name(id)); }
std::string model_id(PotionId id) { return "POTION." + std::string(name(id)); }
std::string model_id(EncounterId id) { return "ENCOUNTER." + std::string(name(id)); }
std::string model_id(EventId id) { return "EVENT." + std::string(name(id)); }
std::string model_id(ActId id) { return "ACT." + std::string(name(id)); }
std::string model_id(CharacterId id) { return "CHARACTER." + std::string(name(id)); }

namespace {

void write_rng(JsonWriter& w, const Rng& r) {
    w.begin_object();
    w.field("counter", r.counter);
    w.field("s0", r.random.s0);
    w.field("s1", r.random.s1);
    w.field("s2", r.random.s2);
    w.field("s3", r.random.s3);
    w.end_object();
}

template <class Id>
void write_id_or_null(JsonWriter& w, std::string_view key, Id id) {
    w.key(key);
    if (id == Id::NONE) w.null();
    else w.value(model_id(id));
}

template <class Span>
void write_ids(JsonWriter& w, std::string_view key, const Span& ids) {
    w.key(key).begin_array();
    for (auto id : ids) w.value(model_id(id));
    w.end_array();
}

void write_grab_bag(JsonWriter& w, const RelicGrabBag& bag) {
    w.begin_object().key("relic_id_lists").begin_object();
    for (const RelicGrabBag::Deque& d : bag.deques) write_ids(w, snake_case(name(d.rarity)), d.relics);
    w.end_object().end_object();
}

void write_player(JsonWriter& w, const GameState& g, const PlayerData& p) {
    w.begin_object();
    w.field("base_orb_slot_count", p.base_orb_slots);
    w.field("character_id", model_id(p.character));
    w.field("current_hp", g.creatures[p.creature].current_hp);
    w.key("deck").begin_array();
    for (CardRef ref : p.deck) {
        const CardState& c = g.cards[ref];
        w.begin_object();
        if (c.upgrade != 0) w.field("current_upgrade_level", static_cast<int32_t>(c.upgrade));
        if (c.floor_added >= 0) w.field("floor_added_to_deck", static_cast<int32_t>(c.floor_added));
        w.field("id", model_id(c.id));
        w.end_object();
    }
    w.end_array();
    w.key("extra_fields").begin_object();
    if (p.card_shop_removals_used != 0) w.field("card_shop_removals_used", p.card_shop_removals_used);
    if (p.wongo_points_earned != 0) w.field("wongo_points", p.wongo_points_earned);
    w.end_object();
    w.field("gold", p.gold);
    w.field("max_energy", p.max_energy);
    w.field("max_hp", g.creatures[p.creature].max_hp);
    w.field("max_potion_slot_count", p.max_potion_slots);
    w.field("net_id", p.net_id);
    w.key("odds").begin_object();
    w.field("card_rarity_odds_value", p.odds.card_rarity);
    w.field("potion_reward_odds_value", p.odds.potion_reward);
    w.end_object();
    bool any_potion = false;
    for (int i = 0; i < p.max_potion_slots; ++i) any_potion |= p.potions[i] != 0;
    if (any_potion) {  // the game omits an empty potion list
        w.key("potions").begin_array();
        for (int i = 0; i < p.max_potion_slots; ++i) {
            if (p.potions[i] == 0) continue;
            w.begin_object().field("id", model_id(g.potions[p.potions[i]].id)).field("slot_index", i).end_object();
        }
        w.end_array();
    }
    w.key("relic_grab_bag");
    write_grab_bag(w, p.relic_grab_bag);
    w.key("relics").begin_array();
    for (RelicRef ref : p.relics) {
        const RelicState& r = g.relics[ref];
        w.begin_object();
        if (r.floor_added >= 0) w.field("floor_added_to_deck", static_cast<int32_t>(r.floor_added));
        w.field("id", model_id(r.id));
        w.end_object();
    }
    w.end_array();
    w.key("rng").begin_object().key("rngs").begin_object();
    for (size_t i = 0; i < p.rng.rngs.size(); ++i) {
        w.key(kPlayerRngNames[i]);
        write_rng(w, p.rng.rngs[i]);
    }
    w.end_object().field("seed", p.rng.seed).end_object();
    w.end_object();
}

}  // namespace

void write_run_save(JsonWriter& w, const GameState& g) {
    const RunData& run = g.run;
    w.begin_object();
    w.key("acts").begin_array();
    for (const ActState& act : run.acts) {
        const RoomSet& r = act.rooms;
        w.begin_object();
        w.field("id", model_id(act.id));
        w.key("rooms").begin_object();
        write_id_or_null(w, "ancient_id", r.ancient);
        w.field("boss_encounters_visited", r.boss_encounters_visited);
        write_id_or_null(w, "boss_id", r.boss);
        write_ids(w, "elite_encounter_ids", r.elite_encounters);
        w.field("elite_encounters_visited", r.elite_encounters_visited);
        write_ids(w, "event_ids", r.events);
        w.field("events_visited", r.events_visited);
        write_ids(w, "normal_encounter_ids", r.normal_encounters);
        w.field("normal_encounters_visited", r.normal_encounters_visited);
        write_id_or_null(w, "second_boss_id", r.second_boss);
        w.end_object();
        w.end_object();
    }
    w.end_array();
    w.field("ascension", run.ascension);
    w.field("current_act_index", run.current_act_index);
    w.key("extra_fields").begin_object();
    if (run.started_with_neow) w.field("started_with_neow", true);
    w.end_object();
    w.field("game_mode", "standard");
    w.key("modifiers").begin_array().end_array();
    w.key("odds").begin_object();
    w.field("unknown_map_point_elite_odds_value", run.odds.elite);
    w.field("unknown_map_point_monster_odds_value", run.odds.monster);
    w.field("unknown_map_point_shop_odds_value", run.odds.shop);
    w.field("unknown_map_point_treasure_odds_value", run.odds.treasure);
    w.end_object();
    w.key("players").begin_array();
    write_player(w, g, run.player);
    w.end_array();
    w.key("rng").begin_object().key("rngs").begin_object();
    for (size_t i = 0; i < run.rng.rngs.size(); ++i) {
        w.key(kRunRngNames[i]);
        write_rng(w, run.rng.rngs[i]);
    }
    w.end_object().field("seed", run.seed_string()).end_object();
    w.key("shared_relic_grab_bag");
    write_grab_bag(w, run.shared_relic_grab_bag);
    w.end_object();
}

void write_map(JsonWriter& w, const ActMap& m) {
    auto coord = [&](const MapPoint& p) {
        w.begin_array().value(static_cast<int32_t>(p.coord.col)).value(static_cast<int32_t>(p.coord.row)).end_array();
    };
    auto coord_of = [&](int id) { coord(m.points[static_cast<size_t>(id)]); };
    w.begin_object();
    w.field("cols", static_cast<int32_t>(m.cols));
    w.field("rows", static_cast<int32_t>(m.rows));
    w.key("start");
    coord_of(m.start);
    w.key("boss");
    coord_of(m.boss);
    w.key("second_boss");
    if (m.second_boss >= 0) coord_of(m.second_boss);
    else w.null();
    // Sorted by column, as the oracle prints them.
    std::vector<int> starts;
    m.start_points.for_each([&](uint8_t id) { starts.push_back(id); });
    std::sort(starts.begin(), starts.end(),
              [&](int a, int b) { return m.points[static_cast<size_t>(a)].coord.col < m.points[static_cast<size_t>(b)].coord.col; });
    w.key("starts").begin_array();
    for (int id : starts) coord_of(id);
    w.end_array();
    w.key("points").begin_array();
    for (int r = 0; r < m.rows; ++r) {
        for (int c = 0; c < m.cols; ++c) {
            int id = m.grid[c][r];
            if (id < 0) continue;
            const MapPoint& p = m.points[static_cast<size_t>(id)];
            w.begin_object();
            w.key("coord");
            coord(p);
            w.field("type", name(p.type));
            w.field("spoils_map_quest", p.spoils_map_quest);
            std::vector<int> kids;
            p.children.for_each([&](uint8_t k) { kids.push_back(k); });
            std::sort(kids.begin(), kids.end(), [&](int a, int b) {
                const MapCoord ca = m.points[static_cast<size_t>(a)].coord, cb = m.points[static_cast<size_t>(b)].coord;
                return ca.row != cb.row ? ca.row < cb.row : ca.col < cb.col;
            });
            w.key("children").begin_array();
            for (int k : kids) coord_of(k);
            w.end_array();
            w.end_object();
        }
    }
    w.end_array();
    w.end_object();
}

}  // namespace sts2
