#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Chaos : CardModel { SIM_MODEL(Chaos, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const {
        static constexpr OrbId valid[] = {OrbId::LIGHTNING_ORB, OrbId::FROST_ORB, OrbId::DARK_ORB, OrbId::PLASMA_ORB, OrbId::GLASS_ORB};
        Rng& rng = Owner().RunState().Rng().CombatOrbs();
        for (int32_t i = 0; i < DynamicVars().Repeat().IntValue(); ++i) OrbCmd::Channel(valid[rng.next_item_index(5)], ctx, Owner());
    }
};
}
