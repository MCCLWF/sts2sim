#pragma once

#include "game/All.h"

namespace sts2::OstyCmd {

Creature Summon(PlayerChoiceContext& choiceContext, Player summoner, Dec amount, AbstractModel source);

}  // namespace sts2::OstyCmd
