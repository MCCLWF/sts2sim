// The `replay` command of sts2sim_cli (see replay_cmd.cpp).
#pragma once

#include <string>

#include "content/generated/ids.h"

namespace sts2 {

int run_replay(const std::string& seed, CharacterId character, int ascension, bool noNeow, int startHp,
               const std::string& deck = {}, const std::string& relics = {}, bool run_queries = false,
               const std::string& stop = {}, bool verify_snapshots = false, bool snapshots = false);

}  // namespace sts2
