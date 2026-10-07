#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "env/env.h"
#include "env/run_queries.h"
#include "game/All.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Rooms/TreasureRoom.h"
#include "game/Runs/RunManager.h"

using namespace sts2;

static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

static std::vector<uint8_t> row_points(const ActMap& map, int row) {
    std::vector<uint8_t> points;
    for (int col = 0; col < map.cols; ++col) {
        int point = map.point_at(col, row);
        if (point >= 0) points.push_back(static_cast<uint8_t>(point));
    }
    return points;
}

static int treasure_point(const ActMap& map) {
    for (int id = 0; id < static_cast<int>(map.points.size()); ++id) {
        const MapPoint& point = map.points[static_cast<size_t>(id)];
        if (point.type == MapPointType::Treasure && map.point_at(point.coord.col, point.coord.row) == id) return id;
    }
    return -1;
}

static void validate_hourglass(const ActMap& map, bool expected_quest_marker = true) {
    const int treasure_row = map.rows - 7;
    const int treasure = treasure_point(map);
    require(treasure >= 0, "Spoils map has no live treasure point");
    const MapPoint& center = map.points[static_cast<size_t>(treasure)];
    require(center.coord.col == map.cols / 2 && center.coord.row == treasure_row,
            "Spoils treasure is not at the centered hourglass waist");
    require(center.spoils_map_quest == expected_quest_marker, "Spoils treasure quest marker state differs");
    require(row_points(map, treasure_row).size() == 1, "Spoils treasure row did not converge to one point");
    for (uint8_t id : row_points(map, 1))
        require(map.points[id].type == MapPointType::Monster, "Spoils first row is not all monsters");
    for (uint8_t id : row_points(map, map.rows - 1))
        require(map.points[id].type == MapPointType::RestSite, "Spoils final row is not all rest sites");

    bool path_to_boss_without_treasure = false;
    auto walk = [&](auto&& self, int point, bool passed_treasure) -> void {
        passed_treasure = passed_treasure || point == treasure;
        if (point == map.boss) {
            path_to_boss_without_treasure |= !passed_treasure;
            return;
        }
        map.points[static_cast<size_t>(point)].children.for_each(
            [&](uint8_t child) { self(self, child, passed_treasure); });
    };
    walk(walk, map.start, false);
    require(!path_to_boss_without_treasure, "Spoils map has a path that bypasses its treasure");

    for (int col = 0; col < map.cols; ++col) {
        for (int row = 0; row < map.rows; ++row) {
            const int id = map.point_at(col, row);
            if (id < 0) continue;
            const MapPoint& point = map.points[static_cast<size_t>(id)];
            point.children.for_each([&](uint8_t child) {
                const MapPoint& next = map.points[child];
                require(next.coord.row == point.coord.row + 1, "Spoils map edge skipped a row");
                require(std::abs(next.coord.col - point.coord.col) <= 1 || child == map.boss,
                        "Spoils map edge exceeded adjacent columns");
            });
        }
    }
}

static std::unique_ptr<Env> run_with_spoils(const std::string& seed, int card_count = 1, int ascension = 0) {
    auto env = std::make_unique<Env>();
    new_run(env->state(), NewRunOptions{.seed = seed, .character = CharacterId::IRONCLAD, .ascension = ascension});
    Player player(&env->sim(), 1);
    RunState run(&env->sim());
    for (int i = 0; i < card_count; ++i)
        CardPileCmd::Add(run.CreateCard(CardId::SPOILS_MAP, player), PileType::Deck);
    RunManager::GenerateMap(&env->sim());
    for (const MapPoint& point : env->state().run.map.points)
        require(!point.spoils_map_quest, "Spoils quest appeared in Act 1");
    RunManager::EnterAct(&env->sim(), 1);
    validate_hourglass(env->state().run.map);
    return env;
}

static void check_quest_flow() {
    auto env = run_with_spoils("SPOIL00001", 2);
    auto& run = env->state().run;
    Player player(&env->sim(), 1);
    const int treasure = treasure_point(run.map);
    require(treasure >= 0, "quest flow has no treasure coordinate");
    const MapCoord coord = run.map.points[static_cast<size_t>(treasure)].coord;
    const std::string snapshot = env->ExportSnapshotJson();
    auto restored = Env::ImportSnapshotJson(snapshot);
    require(restored->Hash() == env->Hash(), "Spoils quest failed snapshot roundtrip");
    require(ExportRunQueries(*env).find("\"spoils_map_quest\":true") != std::string::npos,
            "run query omitted the Spoils quest marker");

    run.visited_map_coords.push_back(coord);
    const int32_t gold_before = player.Gold();
    const int32_t earned = TreasureRoom::DoNormalRewards(&env->sim());
    require(earned >= 1200, "Spoils quest did not grant 600 gold per card");
    require(player.Gold() - gold_before == earned, "treasure gold result disagrees with player gold");
    int remaining = 0;
    for (CardModel card : player.Deck().Cards()) remaining += card.CardId_() == CardId::SPOILS_MAP;
    require(remaining == 0, "completed Spoils Map cards remain in the deck");
    require(!run.map.points[static_cast<size_t>(treasure)].spoils_map_quest,
            "completed Spoils quest marker remains on the map");
    require(TreasureRoom::DoNormalRewards(&env->sim()) < 600,
            "completed Spoils quest granted gold more than once");
}

static std::string dump_map(const ActMap& map) {
    auto coord = [&](MapCoord c) { return "[" + std::to_string(c.col) + "," + std::to_string(c.row) + "]"; };
    std::ostringstream out;
    out << "{\"cols\":" << static_cast<int>(map.cols) << ",\"rows\":" << static_cast<int>(map.rows)
        << ",\"start\":" << coord(map.points[map.start].coord) << ",\"boss\":"
        << coord(map.points[map.boss].coord) << ",\"second_boss\":null,\"starts\":[";
    std::vector<MapCoord> starts;
    map.start_points.for_each([&](uint8_t id) { starts.push_back(map.points[id].coord); });
    std::sort(starts.begin(), starts.end(), [](MapCoord a, MapCoord b) { return a.col < b.col; });
    for (size_t i = 0; i < starts.size(); ++i) { if (i) out << ','; out << coord(starts[i]); }
    out << "],\"points\":[";
    std::vector<const MapPoint*> points;
    for (int row = 0; row < map.rows; ++row)
        for (int col = 0; col < map.cols; ++col)
            if (int id = map.point_at(col, row); id >= 0) points.push_back(&map.points[static_cast<size_t>(id)]);
    bool first_point = true;
    for (const MapPoint* point : points) {
        if (!first_point) out << ',';
        first_point = false;
        out << "{\"coord\":" << coord(point->coord) << ",\"type\":\"" << name(point->type)
            << "\",\"spoils_map_quest\":" << (point->spoils_map_quest ? "true" : "false") << ",\"children\":[";
        std::vector<MapCoord> children;
        point->children.for_each([&](uint8_t id) { children.push_back(map.points[id].coord); });
        std::sort(children.begin(), children.end(), [](MapCoord a, MapCoord b) {
            return a.row != b.row ? a.row < b.row : a.col < b.col;
        });
        for (size_t i = 0; i < children.size(); ++i) { if (i) out << ','; out << coord(children[i]); }
        out << "]}";
    }
    out << "]}";
    return out.str();
}

int main(int argc, char** argv) {
    try {
        if ((argc == 3 || argc == 4) && std::string(argv[1]) == "--dump-map") {
            const int ascension = argc == 4 ? std::atoi(argv[3]) : 0;
            auto env = run_with_spoils(argv[2], 1, ascension);
            std::cout << dump_map(env->state().run.map) << '\n';
            return 0;
        }

        const ActId act_ids[] = {ActId::OVERGROWTH, ActId::UNDERDOCKS, ActId::HIVE};
        for (size_t act = 0; act < std::size(act_ids); ++act) {
            for (uint64_t seed = 1; seed <= 96; ++seed) {
                ActMap map = generate_spoils_map(0x9e3779b97f4a7c15ULL * (seed + 131 * act), act_ids[act],
                                                 /*is_multiplayer=*/false, /*swarming_elites=*/seed % 10 == 0);
                validate_hourglass(map, /*expected_quest_marker=*/false);
            }
        }
        check_quest_flow();
        std::cout << "Spoils Map generation and quest passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
