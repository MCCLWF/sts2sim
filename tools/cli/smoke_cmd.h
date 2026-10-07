// The `smoke` command of sts2sim_cli (see smoke_cmd.cpp).
#pragma once

#include <map>
#include <string>

#include "content/generated/ids.h"

namespace sts2 {

int run_smoke(std::map<std::string, std::string>& opts, CharacterId (*parse_character)(const std::string&));

}  // namespace sts2
