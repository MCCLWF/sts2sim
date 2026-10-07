// Entities/Cards/CardLocation.cs
#pragma once

#include "game/Entities/Players/Player.h"

namespace sts2 {

struct CardLocation {
    Player player;
    PileType pileType{};
    CardPilePosition position{};
    friend bool operator==(const CardLocation&, const CardLocation&) = default;
};

}  // namespace sts2
