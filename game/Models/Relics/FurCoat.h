#pragma once

#include <algorithm>

#include "game/Commands/CreatureCmd.h"
#include "game/Models/RelicModel.h"

namespace sts2 {

// Relics/FurCoat.cs.  The game stores the quest object in each MapPoint; the
// Env represents that one quest identity with MapPoint::fur_coat_quest.
struct FurCoat : RelicModel {
    SIM_MODEL(FurCoat, RelicModel)

    static constexpr int kMarkedCombats = 8;

    static int PointAt(const RunState& run, MapCoord coord) {
        return run.st().map.get_point(coord.col, coord.row);
    }

    static bool IsCurrentPointMarked(const RunState& run) {
        if (run.CurrentMapCol() < 0 || run.CurrentMapRow() < 0) return false;
        int point = PointAt(run, MapCoord{static_cast<int8_t>(run.CurrentMapCol()),
                                         static_cast<int8_t>(run.CurrentMapRow())});
        return point >= 0 && run.st().map.points[static_cast<size_t>(point)].fur_coat_quest;
    }

    void AddMarkedRooms(ActMap& map) const {
        RunState run = Owner().RunState();
        if (run.CurrentActIndex() != run.st().fur_coat_act_index) return;

        bool savedCoordsValid = run.st().fur_coat_coords_set &&
                                run.st().fur_coat_coord_count <= kMarkedCombats;
        for (uint8_t i = 0; savedCoordsValid && i < run.st().fur_coat_coord_count; ++i) {
            MapCoord coord{run.st().fur_coat_coord_cols[i], run.st().fur_coat_coord_rows[i]};
            int point = map.get_point(coord.col, coord.row);
            savedCoordsValid = point >= 0 &&
                               (map.points[static_cast<size_t>(point)].type == MapPointType::Monster ||
                                map.points[static_cast<size_t>(point)].type == MapPointType::Elite);
        }

        if (!savedCoordsValid) {
            StaticVec<MapCoord, kMaxMapPoints> candidates;
            // ActMap.GetAllMapPoints is column-major in the source game.  The
            // order is part of the Fur Coat RNG stream, so do not sort it.
            for (int col = 0; col < map.cols; ++col) {
                for (int row = 0; row < map.rows; ++row) {
                    int point = map.point_at(col, row);
                    if (point < 0) continue;
                    const MapPoint& item = map.points[static_cast<size_t>(point)];
                    if ((item.type == MapPointType::Monster || item.type == MapPointType::Elite) &&
                        !item.fur_coat_quest)
                        candidates.push_back(item.coord);
                }
            }
            Rng rng = Rng::for_content(run.st().run_seed, 0, "FUR_COAT", 0);
            rng.shuffle(candidates.span());
            const int32_t count = std::min<int32_t>(kMarkedCombats, static_cast<int32_t>(candidates.size()));
            run.st().fur_coat_coord_count = static_cast<uint8_t>(count);
            run.st().fur_coat_coords_set = true;
            for (int32_t i = 0; i < count; ++i) {
                run.st().fur_coat_coord_cols[i] = candidates[static_cast<size_t>(i)].col;
                run.st().fur_coat_coord_rows[i] = candidates[static_cast<size_t>(i)].row;
            }
        }

        for (uint8_t i = 0; i < run.st().fur_coat_coord_count; ++i) {
            MapCoord coord{run.st().fur_coat_coord_cols[i], run.st().fur_coat_coord_rows[i]};
            int point = map.get_point(coord.col, coord.row);
            if (point < 0) throw GameError("Fur Coat saved coordinate is missing from the map");
            map.points[static_cast<size_t>(point)].fur_coat_quest = true;
        }
    }

    void AfterObtained() const {
        RunState run = Owner().RunState();
        run.st().fur_coat_act_index = run.CurrentActIndex();
        AddMarkedRooms(run.st().map);
    }

    ActMap ModifyGeneratedMapLate(RunState runState, ActMap map, int32_t actIndex) const {
        (void)actIndex;
        AddMarkedRooms(map);
        return map;
    }

    void BeforeCombatStart() const {
        RunState run = Owner().RunState();
        if (!IsCurrentPointMarked(run)) return;
        run.st().current_map_affected_by_fur_coat = true;
        CombatState combat = Owner().Creature().CombatState();
        for (Creature enemy : combat.HittableEnemies()) CreatureCmd::SetCurrentHp(enemy, Dec(1));
    }

    void AfterCreatureAddedToCombat(Creature creature) const {
        if (!creature.IsEnemy() || !IsCurrentPointMarked(Owner().RunState())) return;
        CreatureCmd::SetCurrentHp(creature, Dec(1));
    }
};

}  // namespace sts2
