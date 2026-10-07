// The `combat` command of sts2sim_cli (see combat_cmd.cpp).
#pragma once

#include <string>

#include "content/generated/ids.h"

namespace sts2 {

// `encounter`: an encounter entry (EnterMapCoordDebug), or empty for the map's.
int run_combat_replay(const std::string& seed, CharacterId character, int ascension, int col, int row,
                      const std::string& encounter, const std::string& actions,
                      const std::string& deck = {}, const std::string& relics = {}, bool takeSpecialRewards = false,
                      const std::string& potion = {}, int32_t startingEnergy = -1, int32_t startingStars = -1,
                      const std::string& potionTarget = "player", bool potionAutoDeath = false,
                      const std::string& madScienceConfig = {}, bool snapshots = false);

}  // namespace sts2
