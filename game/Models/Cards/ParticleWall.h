// Models/Cards/ParticleWall.cs
#pragma once

#include "game/Commands/CreatureCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct ParticleWall : CardModel {
    SIM_MODEL(ParticleWall, CardModel)

    int32_t CanonicalStarCost() const { return 2; }
    bool GainsBlock() const { return true; }
    void OnPlay(PlayerChoiceContext&, const CardPlay* cardPlay) const {
        CreatureCmd::GainBlock(Owner().Creature(), DynamicVars().Block(), cardPlay);
    }
    CardLocation GetResultLocationForCardPlay() const {
        CardLocation location = GetResultLocationForCardPlay_Default();
        if (location.pileType == PileType::Discard) location.pileType = PileType::Hand;
        return location;
    }
};

}  // namespace sts2
