#pragma once

// Original v0.111.0 MultiplayerOnly model. This behavior is not reachable in single-player.
#include "game/Models/CardModel.h"

namespace sts2 {
struct Lift : CardModel {
    SIM_MODEL(Lift, CardModel)
    SIM_NOT_PORTED(OnPlay);  // MultiplayerOnly; excluded from single-player behavior.
    SIM_NOT_PORTED(OnUpgrade);  // MultiplayerOnly; excluded from single-player behavior.
    SIM_NOT_PORTED(CanonicalVars);  // MultiplayerOnly; excluded from single-player behavior.
    SIM_NOT_PORTED(GainsBlock);  // MultiplayerOnly; excluded from single-player behavior.
    SIM_NOT_PORTED(MultiplayerConstraint);  // MultiplayerOnly; excluded from single-player behavior.
};
}  // namespace sts2
