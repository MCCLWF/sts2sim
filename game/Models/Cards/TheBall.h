#pragma once

// Original v0.111.0 MultiplayerOnly model. This behavior is not reachable in single-player.
#include "game/Models/CardModel.h"

namespace sts2 {
struct TheBall : CardModel {
    SIM_MODEL(TheBall, CardModel)
    SIM_NOT_PORTED(AfterDowngraded);  // MultiplayerOnly; excluded from single-player behavior.
    SIM_NOT_PORTED(GetResultLocationForCardPlay);  // MultiplayerOnly; excluded from single-player behavior.
    SIM_NOT_PORTED(OnPlay);  // MultiplayerOnly; excluded from single-player behavior.
    SIM_NOT_PORTED(OnUpgrade);  // MultiplayerOnly; excluded from single-player behavior.
    SIM_NOT_PORTED(CanonicalVars);  // MultiplayerOnly; excluded from single-player behavior.
    SIM_NOT_PORTED(MultiplayerConstraint);  // MultiplayerOnly; excluded from single-player behavior.
};
}  // namespace sts2
