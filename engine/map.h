// Act map generation: port of Map/StandardActMap.cs, MapPathPruning.cs and
// MapPostProcessing.cs. HashSet iteration order is reproduced with DotnetSet
// because it feeds the RNG (segment pruning, point-type assignment).
#pragma once

#include <cstdint>

#include "content/generated/enums.h"
#include "content/generated/ids.h"
#include "core/rng.h"
#include "engine/containers.h"

namespace sts2 {

inline constexpr int kMapWidth = 7;
inline constexpr int kMaxMapRows = 18;  // rooms + ancient row + boss rows
inline constexpr int kMaxMapPoints = kMapWidth * kMaxMapRows + 3;

struct MapCoord {
    int8_t col = 0;
    int8_t row = 0;
    friend constexpr bool operator==(MapCoord, MapCoord) = default;
};

struct MapPoint {
    MapCoord coord;
    MapPointType type = MapPointType::Unassigned;
    bool can_be_modified = true;
    // Fur Coat's map quest marker.  The C# game stores this in the point's
    // quest list; the Env keeps the identity as POD so it survives clones and
    // snapshots without introducing heap-backed model references.
    bool fur_coat_quest = false;
    // Spoils Map's Act 2 treasure quest marker.
    bool spoils_map_quest = false;
    DotnetSet<uint8_t, 8> children;  // point ids
    DotnetSet<uint8_t, 8> parents;
};

// An act's map. Points live in `points` (ids are indices); the grid holds the
// regular rows 0..rows-1 (the ancient start point and bosses sit outside it,
// as in the game).
struct ActMap {
    int8_t cols = kMapWidth;
    int8_t rows = 0;  // StandardActMap._mapLength
    StaticVec<MapPoint, kMaxMapPoints> points;
    int8_t grid[kMapWidth][kMaxMapRows];  // point id or -1
    uint8_t start = 0;                    // StartingMapPoint (MapPointType::Ancient)
    uint8_t boss = 0;
    int16_t second_boss = -1;
    DotnetSet<uint8_t, 8> start_points;  // ActMap.startMapPoints

    int point_at(int col, int row) const {
        if (col < 0 || col >= cols || row < 0 || row >= rows) return -1;
        return grid[col][row];
    }
    // ActMap.GetPoint(col, row), including start and boss points.
    int get_point(int col, int row) const;
};

struct MapPointTypeCounts {
    int num_elites = 5;
    int num_shops = 3;
    int num_unknowns = 0;
    int num_rests = 0;
};

// ActModel.GetNumberOfRooms / GetMapPointTypes, per act.
int act_number_of_rooms(ActId act, bool is_multiplayer);
MapPointTypeCounts act_map_point_types(ActId act, Rng& map_rng, bool swarming_elites);

// StandardActMap(new Rng(seed, "act_{i+1}_map"), act, isMultiplayer, replaceTreasureWithElites, hasSecondBoss)
ActMap generate_standard_map(Rng map_rng, ActId act, bool is_multiplayer, bool replace_treasure_with_elites,
                             bool has_second_boss, bool swarming_elites);

// Map/SpoilsActMap.cs, used when Spoils Map is in the deck in Act 2.
ActMap generate_spoils_map(uint64_t run_seed, ActId act, bool is_multiplayer, bool swarming_elites);

// Map/GoldenPathActMap.cs, single-player constructor.
ActMap generate_golden_path_map();

// StandardActMap.CreateFor(runState, ...) for act index `act_index`.
ActMap generate_act_map(uint64_t run_seed, int act_index, ActId act, bool is_multiplayer,
                        bool replace_treasure_with_elites, bool has_second_boss, bool swarming_elites);

}  // namespace sts2
