// Models/Cards/SpoilsMap.cs. The quest map is generated in Act 2; M2 runs
// only use the card's creation and removal behavior in Act 1.
#pragma once

#include "engine/map.h"
#include "game/Models/CardModel.h"
#include "game/Entities/Cards/CardPile.h"
#include "game/Runs/RunState.h"

namespace sts2 {

struct SpoilsMap : CardModel {
    SIM_MODEL(SpoilsMap, CardModel)

    int32_t MaxUpgradeLevel() const { return 0; }
    void AfterCreated() const { extra(0) = 1; }
    bool HasSpoilsCoord() const { return extra(1) != 0; }
    int32_t SpoilsColumn() const { return extra(2); }
    int32_t SpoilsRow() const { return extra(3); }

    ActMap ModifyGeneratedMap(RunState runState, ActMap map, int32_t actIndex) const {
        if (actIndex != extra(0) || Pile().Type() != PileType::Deck) return map;
        const ActState& act = runState.st().acts[static_cast<size_t>(actIndex)];
        return generate_spoils_map(runState.st().run_seed, act.id, /*is_multiplayer=*/false,
                                   runState.st().has_ascension(AscensionLevel::SwarmingElites));
    }

    ActMap ModifyGeneratedMapLate(RunState runState, ActMap map, int32_t actIndex) const {
        if (actIndex != extra(0) || Pile().Type() != PileType::Deck) return map;
        extra(1) = 0;
        for (int col = 0; col < map.cols && !extra(1); ++col) {
            for (int row = 0; row < map.rows; ++row) {
                const int point = map.point_at(col, row);
                if (point < 0 || map.points[static_cast<size_t>(point)].type != MapPointType::Treasure) continue;
                extra(1) = 1;
                extra(2) = col;
                extra(3) = row;
                break;
            }
        }
        (void)runState;
        return map;
    }

    void AfterMapGenerated(ActMap& map, int32_t actIndex) const {
        if (actIndex != extra(0) || Pile().Type() != PileType::Deck || !extra(1)) return;
        const int point = map.get_point(extra(2), extra(3));
        if (point >= 0) map.points[static_cast<size_t>(point)].spoils_map_quest = true;
    }

    void BeforeCardRemoved(CardModel card) const {
        RunState run = Owner().RunState();
        if (card != *this || run.CurrentActIndex() != extra(0) || !extra(1)) return;
        for (CardRef ref : run.st().player.deck) {
            if (ref == Index() || !sim().g->cards.live(ref)) continue;
            CardModel other(&sim(), ref);
            if (other.CardId_() != CardId::SPOILS_MAP) continue;
            const SpoilsMap other_spoils(other);
            if (other_spoils.HasSpoilsCoord() && other_spoils.SpoilsColumn() == extra(2) &&
                other_spoils.SpoilsRow() == extra(3)) return;
        }
        const int point = run.st().map.get_point(extra(2), extra(3));
        if (point >= 0) run.st().map.points[static_cast<size_t>(point)].spoils_map_quest = false;
    }
};

}  // namespace sts2
