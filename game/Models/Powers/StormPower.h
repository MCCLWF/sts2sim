#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct StormPower : PowerModel { SIM_MODEL(StormPower, PowerModel)
    void BeforeCardPlayed(const CardPlay* play) const {
        if (play && play->Card.Owner() == Owner().Player() && play->Card.Type() == CardType::Power)
            CombatState().CaptureCardPlayPowerAmount(*this, play->Card, Amount());
    }
    void AfterCardPlayed(PlayerChoiceContext& ctx, const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner().Player()) return;
        int32_t amount = 0;
        if (!CombatState().TakeCardPlayPowerAmount(*this, play->Card, amount) || amount <= 0) return;
        for (int32_t i = 0; i < amount; ++i) OrbCmd::Channel(OrbId::LIGHTNING_ORB, ctx, Owner().Player());
    }
};
}
