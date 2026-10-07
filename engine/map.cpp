#include "engine/map.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include "core/dotnet_sort.h"

namespace sts2 {

int ActMap::get_point(int col, int row) const {
    const MapPoint& b = points[boss];
    if (col == b.coord.col && row == b.coord.row) return boss;
    if (second_boss >= 0) {
        const MapPoint& sb = points[static_cast<size_t>(second_boss)];
        if (col == sb.coord.col && row == sb.coord.row) return second_boss;
    }
    const MapPoint& s = points[start];
    if (col == s.coord.col && row == s.coord.row) return start;
    return point_at(col, row);
}

int act_number_of_rooms(ActId act, bool is_multiplayer) {
    int n;
    switch (act) {
        case ActId::OVERGROWTH:
        case ActId::UNDERDOCKS: n = 15; break;
        case ActId::HIVE: n = 14; break;
        case ActId::GLORY: n = 13; break;
        default: throw std::invalid_argument("act_number_of_rooms: unsupported act");
    }
    return is_multiplayer ? n - 1 : n;
}

// MapPointTypeCounts.StandardRandomUnknownCount
static int standard_random_unknown_count(Rng& rng) { return rng.next_gaussian_int(12, 1, 10, 14); }

MapPointTypeCounts act_map_point_types(ActId act, Rng& rng, bool swarming_elites) {
    MapPointTypeCounts c;
    switch (act) {
        case ActId::OVERGROWTH:
        case ActId::UNDERDOCKS:
            c.num_rests = rng.next_gaussian_int(7, 1, 6, 7);
            c.num_unknowns = standard_random_unknown_count(rng);
            break;
        case ActId::HIVE:
            c.num_rests = rng.next_gaussian_int(6, 1, 6, 7);
            c.num_unknowns = standard_random_unknown_count(rng) - 1;
            break;
        case ActId::GLORY:
            c.num_rests = rng.next_int(5, 7);
            c.num_unknowns = standard_random_unknown_count(rng) - 1;
            break;
        default: throw std::invalid_argument("act_map_point_types: unsupported act");
    }
    // (int)Math.Round(5f * (swarming ? 1.6f : 1f))
    float elites = 5.0f * (swarming_elites ? 1.6f : 1.0f);
    c.num_elites = static_cast<int>(std::nearbyint(static_cast<double>(elites)));
    return c;
}

namespace {

// Mutable view over an ActMap while StandardActMap's constructor runs.
class Generator {
public:
    Generator(Rng& rng, ActMap& map, MapPointTypeCounts counts, bool replace_treasure)
        : rng_(rng), m_(map), counts_(counts), replace_treasure_(replace_treasure) {}

    void run(bool has_second_boss) {
        clear_grid();
        m_.boss = new_point(m_.cols / 2, m_.rows);
        m_.start = new_point(m_.cols / 2, 0);
        if (has_second_boss) m_.second_boss = new_point(m_.cols / 2, m_.rows + 1);
        generate_map();
        assign_point_types();
        prune_and_repair();
        center_grid();
        spread_adjacent_points();
        straighten_paths();
    }

    void run_spoils() {
        clear_grid();
        m_.boss = new_point(m_.cols / 2, m_.rows);
        m_.start = new_point(m_.cols / 2, 0);
        const int treasure_row = m_.rows - 7;
        if (treasure_row <= 0 || treasure_row >= m_.rows)
            throw std::logic_error("Spoils Map treasure row is out of bounds");
        generate_hourglass_map(treasure_row);
        assign_spoils_point_types();
        prune_and_repair();
    }

private:
    Rng& rng_;
    ActMap& m_;
    MapPointTypeCounts counts_;
    bool replace_treasure_;

    void clear_grid() {
        for (auto& col : m_.grid) {
            for (auto& cell : col) cell = -1;
        }
    }

    MapPoint& P(int id) { return m_.points[static_cast<size_t>(id)]; }

    int new_point(int col, int row) {
        MapPoint p;
        p.coord = {static_cast<int8_t>(col), static_cast<int8_t>(row)};
        m_.points.push_back(p);
        return static_cast<int>(m_.points.size() - 1);
    }

    int get_or_create(int col, int row) {
        int id = m_.grid[col][row];
        // ActMap.GetPoint also matches the boss/start points by coordinate;
        // generation only ever asks for rows 1..rows-1, which they never occupy.
        if (id >= 0) return id;
        id = new_point(col, row);
        m_.grid[col][row] = static_cast<int8_t>(id);
        return id;
    }

    void add_child(int parent, int child) {
        P(parent).children.add(static_cast<uint8_t>(child));
        P(child).parents.add(static_cast<uint8_t>(parent));
    }

    void remove_child(int parent, int child) {
        P(parent).children.remove(static_cast<uint8_t>(child));
        P(child).parents.remove(static_cast<uint8_t>(parent));
    }

    // ActMap.GetAllMapPoints: column-major over the grid.
    StaticVec<uint8_t, kMaxMapPoints> all_points() const {
        StaticVec<uint8_t, kMaxMapPoints> out;
        for (int c = 0; c < m_.cols; ++c) {
            for (int r = 0; r < m_.rows; ++r) {
                if (m_.grid[c][r] >= 0) out.push_back(static_cast<uint8_t>(m_.grid[c][r]));
            }
        }
        return out;
    }

    // MapPoint.CompareTo -> MapCoord.CompareTo: (col, row).
    int compare_points(uint8_t a, uint8_t b) {
        const MapCoord ca = P(a).coord, cb = P(b).coord;
        if (ca.col != cb.col) return ca.col < cb.col ? -1 : 1;
        if (ca.row != cb.row) return ca.row < cb.row ? -1 : 1;
        return 0;
    }

    template <size_t N>
    void stable_shuffle_points(StaticVec<uint8_t, N>& list) {
        dotnet_sort(list.span(), [this](uint8_t a, uint8_t b) { return compare_points(a, b); });
        rng_.shuffle(list.span());
    }

    // ---------------------------------------------------------------- GenerateMap

    void generate_map() {
        for (int i = 0; i < 7; ++i) {
            int p = get_or_create(rng_.next_int(0, 7), 1);
            if (i == 1) {
                while (m_.start_points.contains(static_cast<uint8_t>(p))) p = get_or_create(rng_.next_int(0, 7), 1);
            }
            m_.start_points.add(static_cast<uint8_t>(p));
            path_generate(p);
        }
        for (int c = 0; c < m_.cols; ++c) {
            int id = m_.grid[c][m_.rows - 1];
            if (id >= 0) add_child(id, m_.boss);
        }
        if (m_.second_boss >= 0) add_child(m_.boss, m_.second_boss);
        for (int c = 0; c < m_.cols; ++c) {
            int id = m_.grid[c][1];
            if (id >= 0) add_child(m_.start, id);
        }
    }

    // SpoilsActMap.GenerateHourglassMap. The rows narrow toward one fixed
    // center treasure and widen again below it; unlike StandardActMap this
    // topology intentionally skips map post-processing after pruning.
    void generate_hourglass_map(int treasure_row) {
        for (int i = 0; i < 7; ++i) {
            int p = get_or_create(rng_.next_int(0, 7), 1);
            if (i == 1) {
                while (m_.start_points.contains(static_cast<uint8_t>(p)))
                    p = get_or_create(rng_.next_int(0, 7), 1);
            }
            m_.start_points.add(static_cast<uint8_t>(p));
            path_generate_spoils(p, treasure_row);
        }

        const int center = m_.cols / 2;
        int treasure = get_or_create(center, treasure_row);
        P(treasure).type = MapPointType::Treasure;
        P(treasure).can_be_modified = false;
        StaticVec<uint8_t, kMapWidth> row_points;
        for (int col = 0; col < m_.cols; ++col) {
            int id = m_.grid[col][treasure_row];
            if (id >= 0) row_points.push_back(static_cast<uint8_t>(id));
        }
        for (uint8_t point : row_points) {
            if (point != treasure) redirect_to_treasure(point, treasure);
        }

        connect_row_to_boss();
        connect_row_to_start();
    }

    void path_generate_spoils(int start, int treasure_row) {
        int cur = start;
        while (P(cur).coord.row < m_.rows - 1) {
            MapCoord next = generate_next_coord_spoils(cur, treasure_row);
            int n = get_or_create(next.col, next.row);
            add_child(cur, n);
            cur = n;
        }
    }

    MapCoord generate_next_coord_spoils(int cur, int treasure_row) {
        const int current_col = P(cur).coord.col;
        const int row = P(cur).coord.row + 1;
        const int center = m_.cols / 2;
        const int dist_to_treasure = std::abs(row - treasure_row);
        const int rows_to_bottom = m_.rows - 1 - row;
        const int max_spread = std::min(center, std::max(0, rows_to_bottom) + 1);
        const int spread = std::min(center, std::min(dist_to_treasure, max_spread));
        const int min_col = std::max(0, center - spread);
        const int max_col = std::min(static_cast<int>(m_.cols) - 1, center + spread);

        StaticVec<int, 3> directions{-1, 0, 1};
        if (treasure_row - P(cur).coord.row > 3) {
            dotnet_sort(directions.span(), [](int a, int b) { return (a > b) - (a < b); });
            rng_.shuffle(directions.span());
        } else if (treasure_row - P(cur).coord.row > 0) {
            directions.clear();
            const int toward_center = (center > current_col) - (center < current_col);
            if (toward_center != 0) directions.push_back(toward_center);
            directions.push_back(0);
            if (toward_center != 0) directions.push_back(-toward_center);
            if (!directions.contains(-1)) directions.push_back(-1);
            if (!directions.contains(1)) directions.push_back(1);
        } else {
            dotnet_sort(directions.span(), [](int a, int b) { return (a > b) - (a < b); });
            rng_.shuffle(directions.span());
        }

        for (int direction : directions) {
            const int next_col = direction == -1 ? std::max(0, current_col - 1)
                                : direction == 1 ? std::min(static_cast<int>(m_.cols) - 1, current_col + 1)
                                                 : current_col;
            if (next_col < min_col || next_col > max_col || has_invalid_crossover(cur, next_col)) continue;
            const int point = m_.grid[next_col][row];
            const bool parent_room = point < 0 || P(point).parents.contains(static_cast<uint8_t>(cur)) ||
                                     P(point).parents.size() < 3;
            const bool child_room = cur == m_.start || P(cur).children.size() < 3 ||
                                    (point >= 0 && P(cur).children.contains(static_cast<uint8_t>(point)));
            if (parent_room && child_room) return {static_cast<int8_t>(next_col), static_cast<int8_t>(row)};
        }

        int next_col = std::clamp(center, min_col, max_col);
        if (std::abs(next_col - current_col) > 1)
            next_col = std::clamp(current_col + ((next_col > current_col) - (next_col < current_col)), min_col, max_col);
        if (has_invalid_crossover(cur, next_col)) next_col = std::clamp(current_col, min_col, max_col);
        if (std::abs(next_col - current_col) > 1)
            throw std::logic_error("Spoils Map fallback step exceeds adjacency");
        return {static_cast<int8_t>(next_col), static_cast<int8_t>(row)};
    }

    void redirect_to_treasure(int stray, int treasure) {
        for (uint8_t parent : P(stray).parents.to_vec()) {
            remove_child(parent, stray);
            add_child(parent, treasure);
        }
        for (uint8_t child : P(stray).children.to_vec()) {
            remove_child(stray, child);
            add_child(treasure, child);
        }
        m_.grid[P(stray).coord.col][P(stray).coord.row] = -1;
    }

    void connect_row_to_boss() {
        for (int col = 0; col < m_.cols; ++col) {
            int id = m_.grid[col][m_.rows - 1];
            if (id >= 0 && !P(id).children.contains(m_.boss)) add_child(id, m_.boss);
        }
    }

    void connect_row_to_start() {
        for (int col = 0; col < m_.cols; ++col) {
            int id = m_.grid[col][1];
            if (id >= 0 && !P(m_.start).children.contains(static_cast<uint8_t>(id))) add_child(m_.start, id);
        }
    }

    void path_generate(int start) {
        int cur = start;
        while (P(cur).coord.row < m_.rows - 1) {
            MapCoord next = generate_next_coord(cur);
            int n = get_or_create(next.col, next.row);
            add_child(cur, n);
            cur = n;
        }
    }

    MapCoord generate_next_coord(int cur) {
        int col = P(cur).coord.col;
        int left = std::max(0, col - 1);
        int right = std::min(col + 1, 6);
        StaticVec<int, 3> dirs{-1, 0, 1};
        dotnet_sort(dirs.span(), [](int a, int b) { return (a > b) - (a < b); });
        rng_.shuffle(dirs.span());
        for (int d : dirs) {
            int target = d == -1 ? left : (d == 0 ? col : right);
            if (!has_invalid_crossover(cur, target)) {
                return {static_cast<int8_t>(target), static_cast<int8_t>(P(cur).coord.row + 1)};
            }
        }
        throw std::logic_error("Cannot find next node");
    }

    bool has_invalid_crossover(int cur, int target_x) {
        int delta = target_x - P(cur).coord.col;
        if (delta == 0 || delta == 7) return false;
        int other = m_.grid[target_x][P(cur).coord.row];
        if (other < 0) return false;
        return P(other).children.any(
            [&](uint8_t c) { return P(c).coord.col - P(other).coord.col == -delta; });
    }

    // ---------------------------------------------------------------- AssignPointTypes

    void set_row(int row, MapPointType type) {
        for (int c = 0; c < m_.cols; ++c) {
            int id = m_.grid[c][row];
            if (id >= 0) {
                P(id).type = type;
                P(id).can_be_modified = false;
            }
        }
    }

    void assign_point_types() {
        set_row(m_.rows - 1, MapPointType::RestSite);
        set_row(m_.rows - 7, replace_treasure_ ? MapPointType::Elite : MapPointType::Treasure);
        set_row(1, MapPointType::Monster);

        // Queue<MapPointType>: rests, shops, elites, unknowns.
        StaticVec<MapPointType, 64> queue;
        for (int i = 0; i < counts_.num_rests; ++i) queue.push_back(MapPointType::RestSite);
        for (int i = 0; i < counts_.num_shops; ++i) queue.push_back(MapPointType::Shop);
        for (int i = 0; i < counts_.num_elites; ++i) queue.push_back(MapPointType::Elite);
        for (int i = 0; i < counts_.num_unknowns; ++i) queue.push_back(MapPointType::Unknown);

        for (int pass = 0; pass < 3; ++pass) {
            if (queue.empty()) break;
            StaticVec<uint8_t, kMaxMapPoints> list;
            for (uint8_t id : all_points()) {
                if (P(id).type == MapPointType::Unassigned) list.push_back(id);
            }
            stable_shuffle_points(list);
            for (uint8_t id : list) {
                if (queue.empty()) break;
                P(id).type = next_valid_point_type(queue, id);
            }
        }
        for (uint8_t id : all_points()) {
            if (P(id).type == MapPointType::Unassigned) P(id).type = MapPointType::Monster;
        }
        P(m_.boss).type = MapPointType::Boss;
        P(m_.start).type = MapPointType::Ancient;
        if (m_.second_boss >= 0) P(m_.second_boss).type = MapPointType::Boss;
    }

    void assign_spoils_point_types() {
        set_row(m_.rows - 1, MapPointType::RestSite);
        set_row(1, MapPointType::Monster);

        StaticVec<MapPointType, 64> queue;
        for (int i = 0; i < counts_.num_rests; ++i) queue.push_back(MapPointType::RestSite);
        for (int i = 0; i < counts_.num_shops; ++i) queue.push_back(MapPointType::Shop);
        for (int i = 0; i < counts_.num_elites; ++i) queue.push_back(MapPointType::Elite);
        for (int i = 0; i < counts_.num_unknowns; ++i) queue.push_back(MapPointType::Unknown);

        for (int pass = 0; pass < 3; ++pass) {
            if (queue.empty()) break;
            StaticVec<uint8_t, kMaxMapPoints> list;
            for (uint8_t id : all_points()) {
                if (P(id).type == MapPointType::Unassigned) list.push_back(id);
            }
            stable_shuffle_points(list);
            for (uint8_t id : list) {
                if (queue.empty()) break;
                P(id).type = next_valid_point_type(queue, id);
            }
        }
        for (uint8_t id : all_points()) {
            if (P(id).type == MapPointType::Unassigned) P(id).type = MapPointType::Monster;
        }
        P(m_.boss).type = MapPointType::Boss;
        P(m_.start).type = MapPointType::Ancient;
    }

    template <size_t N>
    MapPointType next_valid_point_type(StaticVec<MapPointType, N>& queue, int point) {
        for (size_t i = 0; i < queue.size(); ++i) {
            MapPointType t = queue.front();
            queue.erase_at(0);
            if (is_valid_point_type(t, point)) return t;
            queue.push_back(t);
        }
        return MapPointType::Unassigned;
    }

    static bool in(MapPointType t, std::initializer_list<MapPointType> set) {
        for (MapPointType s : set) {
            if (s == t) return true;
        }
        return false;
    }

public:
    bool is_valid_point_type(MapPointType t, int point) {
        const MapPoint& p = P(point);
        // IsValidForUpper
        if (p.coord.row >= m_.rows - 3 && t == MapPointType::RestSite) return false;
        // IsValidForLower
        if (p.coord.row < 6 && in(t, {MapPointType::RestSite, MapPointType::Elite})) return false;
        // IsValidWithParents (parents and children)
        const auto restricted = {MapPointType::Elite, MapPointType::RestSite, MapPointType::Treasure,
                                 MapPointType::Shop};
        if (in(t, restricted)) {
            if (p.parents.any([&](uint8_t q) { return P(q).type == t; })) return false;
            if (p.children.any([&](uint8_t q) { return P(q).type == t; })) return false;
        }
        // IsValidWithChildren is subsumed by the check above.
        // IsValidWithSiblings
        if (in(t, {MapPointType::RestSite, MapPointType::Monster, MapPointType::Unknown, MapPointType::Elite,
                   MapPointType::Shop})) {
            bool clash = p.parents.any([&](uint8_t par) {
                return P(par).children.any([&](uint8_t sib) { return sib != point && P(sib).type == t; });
            });
            if (clash) return false;
        }
        return true;
    }

private:
    // ---------------------------------------------------------------- MapPathPruning

    using Path = std::vector<uint8_t>;

    void prune_and_repair() {
        for (int i = 0; i < 3; ++i) {
            prune_duplicate_segments();
            if (!repair_pruned_point_types()) break;
        }
    }

    bool repair_pruned_point_types() {
        bool flag = false;
        flag |= repair_point_type(MapPointType::Shop, counts_.num_shops);
        flag |= repair_point_type(MapPointType::Elite, counts_.num_elites);
        flag |= repair_point_type(MapPointType::RestSite, counts_.num_rests);
        flag |= repair_point_type(MapPointType::Unknown, counts_.num_unknowns);
        return flag;
    }

    bool repair_point_type(MapPointType type, int target) {
        int have = 0;
        for (uint8_t id : all_points()) have += P(id).type == type;
        int missing = target - have;
        if (missing <= 0) return false;
        bool result = false;
        StaticVec<uint8_t, kMaxMapPoints> list;
        for (uint8_t id : all_points()) {
            if (P(id).type == MapPointType::Monster && P(id).can_be_modified) list.push_back(id);
        }
        stable_shuffle_points(list);
        for (uint8_t id : list) {
            if (missing == 0) break;
            if (is_valid_point_type(type, id)) {
                P(id).type = type;
                --missing;
                result = true;
            }
        }
        return result;
    }

    void prune_duplicate_segments() {
        int n = 0;
        auto matching = find_matching_segments();
        while (prune_paths(matching)) {
            if (++n > 50) throw std::logic_error("Unable to prune matching segments");
            matching = find_matching_segments();
        }
    }

    std::vector<std::vector<Path>> find_matching_segments() {
        std::map<std::string, std::vector<Path>> segments;  // SortedDictionary, ordinal keys
        SeenSegments seen;
        Path path;
        size_t shared = 0;
        walk_paths(m_.start, path, shared, segments, seen);
        std::vector<std::vector<Path>> out;
        for (auto& [key, list] : segments) {
            if (list.size() > 1) out.push_back(list);
        }
        return out;
    }

    // The point sequence of a segment, packed (point id + 1 per byte).
    struct SegmentId {
        std::array<uint64_t, 3> words{};
        friend bool operator==(const SegmentId&, const SegmentId&) = default;
        void set(size_t position, uint8_t point) {
            if (position >= 24) throw std::logic_error("map segment longer than 24 points");
            words[position / 8] |= (static_cast<uint64_t>(point) + 1) << (8 * (position % 8));
        }
    };
    struct SegmentIdHash {
        size_t operator()(const SegmentId& s) const {
            uint64_t h = 0x9E3779B97F4A7C15ull;
            for (uint64_t w : s.words) { h ^= w; h *= 0xFF51AFD7ED558CCDull; h ^= h >> 33; }
            return static_cast<size_t>(h);
        }
    };
    using SeenSegments = std::unordered_set<SegmentId, SegmentIdHash>;

    // FindAllPaths in the same order (children first to last, depth first),
    // without building the list: each complete path goes straight to
    // add_segments. `shared` is how many leading points the path has in
    // common with the previous complete path.
    void walk_paths(int cur, Path& path, size_t& shared, std::map<std::string, std::vector<Path>>& segments,
                    SeenSegments& seen) {
        path.push_back(static_cast<uint8_t>(cur));
        if (P(cur).type == MapPointType::Boss) {
            add_segments(path, shared, segments, seen);
            shared = path.size();
        } else {
            P(cur).children.for_each([&](uint8_t child) { walk_paths(child, path, shared, segments, seen); });
        }
        path.pop_back();
        shared = std::min(shared, path.size());
    }

    bool valid_segment_start(int p) {
        if (P(p).children.size() <= 1) return P(p).coord.row == 0;
        return true;
    }

    bool valid_segment_end(int p) { return P(p).parents.size() >= 2; }

    std::string segment_key(const Path& seg) {
        const MapPoint& a = P(seg.front());
        const MapPoint& b = P(seg.back());
        std::string key;
        if (a.coord.row == 0) {
            key = std::to_string(a.coord.row) + "-" + std::to_string(b.coord.col) + "," + std::to_string(b.coord.row) + "-";
        } else {
            key = std::to_string(a.coord.col) + "," + std::to_string(a.coord.row) + "-" + std::to_string(b.coord.col) +
                  "," + std::to_string(b.coord.row) + "-";
        }
        for (size_t i = 0; i < seg.size(); ++i) {
            if (i) key += ",";
            key += std::to_string(static_cast<int>(P(seg[i]).type));
        }
        return key;
    }

    static bool overlapping(const Path& a, const Path& b) {
        if (a.size() < 3 || b.size() < 3) return false;
        for (size_t i = 1; i + 2 <= a.size(); ++i) {
            if (a[i] == b[i]) return true;
        }
        return false;
    }

    // FindMatchingSegments offers every segment of every path, and most paths
    // share most of their segments. Offering the same point sequence again
    // never changes a list: its key exists by then, and it overlaps either
    // its own first copy or whatever kept that copy out. `seen` skips those
    // repeats before their key is built; every list and its order are the
    // same as without it. A segment that ends inside the first `shared`
    // points was offered by the previous path, so it is not even looked up.
    void add_segments(const Path& path, size_t shared, std::map<std::string, std::vector<Path>>& segments,
                      SeenSegments& seen) {
        const size_t n = path.size();
        for (size_t i = 0; i + 1 < n; ++i) {
            if (!valid_segment_start(path[i])) continue;
            SegmentId id;
            id.set(0, path[i]);
            if (i + 1 < n) id.set(1, path[i + 1]);
            for (size_t j = 2; j < n - i; ++j) {
                id.set(j, path[i + j]);
                if (i + j < shared || !valid_segment_end(path[i + j])) continue;
                if (!seen.insert(id).second) continue;
                Path seg(path.begin() + static_cast<ptrdiff_t>(i), path.begin() + static_cast<ptrdiff_t>(i + j + 1));
                std::string key = segment_key(seg);
                auto it = segments.find(key);
                if (it == segments.end()) {
                    segments.emplace(std::move(key), std::vector<Path>{std::move(seg)});
                } else {
                    bool any = false;
                    for (const Path& existing : it->second) any |= overlapping(existing, seg);
                    if (!any) it->second.push_back(std::move(seg));
                }
            }
        }
    }

    bool prune_paths(std::vector<std::vector<Path>>& matching) {
        for (auto& list : matching) {
            rng_.shuffle(std::span<Path>(list));
            if (prune_all_but_last(list) != 0) return true;
            for (const Path& seg : list) {
                if (break_parent_child(seg)) return true;
            }
        }
        return false;
    }

    int prune_all_but_last(const std::vector<Path>& matches) {
        int n = 0;
        for (const Path& seg : matches) {
            if (n == static_cast<int>(matches.size()) - 1) return n;
            if (prune_segment(seg)) ++n;
        }
        return n;
    }

    bool removed(int p) { return m_.grid[P(p).coord.col][P(p).coord.row] != p; }

    bool in_map(int p) {
        if (removed(p) && P(p).type != MapPointType::Ancient) return P(p).type == MapPointType::Boss;
        return true;
    }

    bool prune_segment(const Path& seg) {
        bool result = false;
        for (size_t i = 0; i + 1 < seg.size(); ++i) {
            int p = seg[i];
            if (!in_map(p)) return true;
            if (P(p).children.size() > 1 || P(p).parents.size() > 1 ||
                P(p).parents.any([&](uint8_t n) { return P(n).children.size() == 1 && !removed(n); })) {
                continue;
            }
            bool branching = false;
            for (size_t k = i; k < seg.size(); ++k) {
                if (P(seg[k]).children.size() > 1 && P(seg[k]).parents.size() == 1) branching = true;
            }
            if (branching) continue;
            if (P(seg.back()).parents.size() == 1) return false;
            bool orphan_outside = P(p).children.any([&](uint8_t c) {
                bool in_seg = std::find(seg.begin(), seg.end(), c) != seg.end();
                return !in_seg && P(c).parents.size() == 1;
            });
            if (!orphan_outside) {
                remove_point(p);
                result = true;
            }
        }
        return result;
    }

    void remove_point(int p) {
        m_.grid[P(p).coord.col][P(p).coord.row] = -1;
        m_.start_points.remove(static_cast<uint8_t>(p));
        for (uint8_t c : P(p).children.to_vec()) remove_child(p, c);
        for (uint8_t par : P(p).parents.to_vec()) remove_child(par, p);
    }

    bool break_parent_child(const Path& seg) {
        bool result = false;
        for (size_t i = 0; i + 1 < seg.size(); ++i) {
            int p = seg[i];
            if (P(p).children.size() >= 2) {
                int next = seg[i + 1];
                if (P(next).parents.size() != 1) {
                    remove_child(p, next);
                    result = true;
                }
            }
        }
        return result;
    }

    // ---------------------------------------------------------------- MapPostProcessing

    bool column_empty(int col) const {
        for (int r = 0; r < m_.rows; ++r) {
            if (m_.grid[col][r] >= 0) return false;
        }
        return true;
    }

    void center_grid() {
        const int w = m_.cols, h = m_.rows;
        bool left = column_empty(0) && column_empty(1);
        bool right = column_empty(w - 1) && column_empty(w - 2);
        int shift = 0;
        if (left && !right) shift = -1;
        else if (!left && right) shift = 1;
        if (shift == 0) return;
        if (shift > 0) {
            for (int r = 0; r < h; ++r) {
                for (int c = w - 1; c >= 0; --c) {
                    int id = m_.grid[c][r];
                    m_.grid[c][r] = -1;
                    int nc = c + shift;
                    if (nc < w) {
                        m_.grid[nc][r] = static_cast<int8_t>(id);
                        if (id >= 0) P(id).coord.col = static_cast<int8_t>(nc);
                    }
                }
            }
        } else {
            for (int r = 0; r < h; ++r) {
                for (int c = 0; c < w; ++c) {
                    int id = m_.grid[c][r];
                    m_.grid[c][r] = -1;
                    int nc = c + shift;
                    if (nc >= 0) {
                        m_.grid[nc][r] = static_cast<int8_t>(id);
                        if (id >= 0) P(id).coord.col = static_cast<int8_t>(nc);
                    }
                }
            }
        }
    }

    // GetAllowedPositions: columns within one of every parent and child, ascending.
    uint32_t allowed_positions(int p, int total) {
        uint32_t mask = (1u << total) - 1;
        auto neighbor = [&](int col) {
            uint32_t m = 0;
            for (int d = -1; d <= 1; ++d) {
                int n = col + d;
                if (n >= 0 && n < total) m |= 1u << n;
            }
            return m;
        };
        P(p).parents.for_each([&](uint8_t q) { mask &= neighbor(P(q).coord.col); });
        P(p).children.for_each([&](uint8_t q) { mask &= neighbor(P(q).coord.col); });
        return mask;
    }

    template <size_t N>
    int compute_gap(int candidate, const StaticVec<uint8_t, N>& row_nodes, int cur) {
        int best = INT_MAX;
        for (uint8_t n : row_nodes) {
            if (n != cur) best = std::min(best, std::abs(candidate - P(n).coord.col));
        }
        return best;
    }

    void spread_adjacent_points() {
        const int w = m_.cols, h = m_.rows;
        for (int r = 0; r < h; ++r) {
            StaticVec<uint8_t, kMapWidth> list;
            for (int c = 0; c < w; ++c) {
                if (m_.grid[c][r] >= 0) list.push_back(static_cast<uint8_t>(m_.grid[c][r]));
            }
            bool moved;
            do {
                moved = false;
                for (uint8_t item : list) {
                    int col = P(item).coord.col;
                    uint32_t allowed = allowed_positions(item, w);
                    int best = col;
                    int best_gap = compute_gap(col, list, item);
                    for (int pos = 0; pos < w; ++pos) {
                        if (!(allowed >> pos & 1) || pos == col) continue;
                        if (m_.grid[pos][r] >= 0 && m_.grid[pos][r] != item) continue;
                        int gap = compute_gap(pos, list, item);
                        if (gap > best_gap) {
                            best = pos;
                            best_gap = gap;
                        }
                    }
                    if (best != col) {
                        m_.grid[col][r] = -1;
                        m_.grid[best][r] = static_cast<int8_t>(item);
                        P(item).coord.col = static_cast<int8_t>(best);
                        moved = true;
                    }
                }
            } while (moved);
        }
    }

    void straighten_paths() {
        const int w = m_.cols, h = m_.rows;
        for (int r = 0; r < h; ++r) {
            for (int c = 0; c < w; ++c) {
                int id = m_.grid[c][r];
                if (id < 0 || P(id).parents.size() != 1 || P(id).children.size() != 1) continue;
                int parent = P(id).parents.first();
                int child = P(id).children.first();
                int col = P(id).coord.col;
                bool go_right = col < P(child).coord.col && col < P(parent).coord.col;
                bool go_left = col > P(child).coord.col && col > P(parent).coord.col;
                if (go_right && c < w - 1) {
                    if (m_.grid[c + 1][r] >= 0) continue;
                    P(id).coord.col = static_cast<int8_t>(c + 1);
                    m_.grid[c][r] = -1;
                    m_.grid[c + 1][r] = static_cast<int8_t>(id);
                }
                if (go_left && c > 0 && m_.grid[c - 1][r] < 0) {
                    P(id).coord.col = static_cast<int8_t>(c - 1);
                    m_.grid[c][r] = -1;
                    m_.grid[c - 1][r] = static_cast<int8_t>(id);
                }
            }
        }
    }
};

}  // namespace

ActMap generate_standard_map(Rng map_rng, ActId act, bool is_multiplayer, bool replace_treasure_with_elites,
                             bool has_second_boss, bool swarming_elites) {
    ActMap map;
    map.rows = static_cast<int8_t>(act_number_of_rooms(act, is_multiplayer) + 1);
    MapPointTypeCounts counts = act_map_point_types(act, map_rng, swarming_elites);
    Generator gen(map_rng, map, counts, replace_treasure_with_elites);
    gen.run(has_second_boss);
    return map;
}

ActMap generate_spoils_map(uint64_t run_seed, ActId act, bool is_multiplayer, bool swarming_elites) {
    Rng map_rng(run_seed, "spoils_map");
    ActMap map;
    map.rows = static_cast<int8_t>(act_number_of_rooms(act, is_multiplayer) + 1);
    MapPointTypeCounts counts = act_map_point_types(act, map_rng, swarming_elites);
    Generator gen(map_rng, map, counts, /*replace_treasure=*/false);
    gen.run_spoils();
    return map;
}

ActMap generate_golden_path_map() {
    constexpr MapPointType point_types[16] = {
        MapPointType::Monster, MapPointType::Unknown, MapPointType::Monster, MapPointType::RestSite,
        MapPointType::Monster, MapPointType::RestSite, MapPointType::Unknown, MapPointType::Treasure,
        MapPointType::Unknown, MapPointType::Treasure, MapPointType::Unknown, MapPointType::Shop,
        MapPointType::Elite, MapPointType::RestSite, MapPointType::Elite, MapPointType::RestSite,
    };

    ActMap map{};
    map.rows = 17;  // GoldenPathActMap grid is [7, 16 + 1]; boss is outside the grid.
    for (int col = 0; col < kMapWidth; ++col)
        for (int row = 0; row < kMaxMapRows; ++row) map.grid[col][row] = -1;

    MapPoint start;
    start.coord = MapCoord{3, 0};
    start.type = MapPointType::Ancient;
    map.points.push_back(start);
    map.start = 0;

    for (int i = 0; i < 16; ++i) {
        MapPoint point;
        point.coord = MapCoord{3, static_cast<int8_t>(i + 1)};
        point.type = point_types[i];
        uint8_t index = static_cast<uint8_t>(map.points.size());
        map.points.push_back(point);
        map.grid[3][i + 1] = static_cast<int8_t>(index);
        if (i == 0) map.start_points.add(index);
        if (i > 0) {
            map.points[index - 1].children.add(index);
            map.points[index].parents.add(static_cast<uint8_t>(index - 1));
        }
    }

    MapPoint boss;
    boss.coord = MapCoord{3, 17};
    boss.type = MapPointType::Boss;
    map.boss = static_cast<uint8_t>(map.points.size());
    map.points.push_back(boss);
    map.points[map.start].children.add(1);
    map.points[1].parents.add(map.start);
    map.points[map.boss - 1].children.add(map.boss);
    map.points[map.boss].parents.add(static_cast<uint8_t>(map.boss - 1));
    return map;
}

ActMap generate_act_map(uint64_t run_seed, int act_index, ActId act, bool is_multiplayer,
                        bool replace_treasure_with_elites, bool has_second_boss, bool swarming_elites) {
    std::string name = "act_" + std::to_string(act_index + 1) + "_map";
    return generate_standard_map(Rng(run_seed, name), act, is_multiplayer, replace_treasure_with_elites,
                                 has_second_boss, swarming_elites);
}

}  // namespace sts2
