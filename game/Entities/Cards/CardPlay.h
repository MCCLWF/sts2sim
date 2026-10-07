// Entities/Cards/CardPlay.cs and Entities/Cards/ResourceInfo.cs
#pragma once

#include <cstdint>
#include "core/csharp_int.h"

#include "game/Entities/Creatures/Creature.h"
#include "game/Entities/Players/Player.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct ResourceInfo {
    int32_t EnergySpent = 0;
    int32_t EnergyValue = 0;
    int32_t StarsSpent = 0;
    int32_t StarValue = 0;
};

struct CardPlay {
    CardModel Card;
    sts2::Player Player;
    Creature Target;
    PileType ResultPile = PileType::None;
    ResourceInfo Resources;
    bool IsAutoPlay = false;
    int32_t PlayIndex = 0;
    int32_t PlayCount = 1;

    // The source history retains this CardPlay object's identity. While it is
    // executing, remember only its own entries so Unmovable can exclude them
    // from the POD turn total. Prefix replay reconstructs this stack object.
    mutable int32_t BlockHistoryCount = 0;
    mutable int32_t BlockHistoryRound = -1;
    mutable int32_t BlockHistoryPlayerTurn = -1;
    mutable CombatSide BlockHistorySide = CombatSide::Player;
    mutable uint32_t BlockHistoryEpoch = 0;

    int32_t BlockEntriesAt(int32_t round, CombatSide side, int32_t playerTurn, uint32_t epoch) const {
        return BlockHistoryRound == round && BlockHistorySide == side && BlockHistoryPlayerTurn == playerTurn && BlockHistoryEpoch == epoch
            ? BlockHistoryCount : 0;
    }
    void RecordBlockEntry(int32_t round, CombatSide side, int32_t playerTurn, uint32_t epoch) const {
        BlockHistoryCount = list_count_add(BlockEntriesAt(round, side, playerTurn, epoch));
        BlockHistoryRound = round;
        BlockHistorySide = side;
        BlockHistoryPlayerTurn = playerTurn;
        BlockHistoryEpoch = epoch;
    }

    bool IsFirstInSeries() const { return PlayIndex == 0; }
    bool IsLastInSeries() const { return PlayIndex == PlayCount - 1; }
};

}  // namespace sts2
