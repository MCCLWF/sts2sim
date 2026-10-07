// Models/Monsters/DecimillipedeSegmentMiddle.cs
#pragma once

#include "game/Models/Monsters/DecimillipedeSegmentLogic.h"

namespace sts2 {

struct DecimillipedeSegmentMiddle : DecimillipedeSegmentLogic {
    SIM_MODEL(DecimillipedeSegmentMiddle, DecimillipedeSegmentLogic)

    SIM_NOT_PORTED(GenerateAnimator);
    float HpBarSizeReduction() const { return 35.0f; }
    bool ShouldFadeAfterDeath() const { return false; }
    bool ShouldDisappearFromDoom() const { return false; }
    bool ShouldShowInCompendium() const { return false; }
    bool CanChangeScale() const { return false; }
    // Source hook changes only the displayed texture.
    void AfterDeath(PlayerChoiceContext&, sts2::Creature, bool, float) const {}
    void AfterAddedToRoom() const { DecimillipedeSegmentLogic::AfterAddedToRoom(); }
    MoveMachineDef GenerateMoveStateMachine() const { return DecimillipedeSegmentLogic::GenerateMoveStateMachine(); }
};

}  // namespace sts2
