#pragma once
#include "game/Commands/PlayerCmd.h"
#include "game/Models/PowerModel.h"
namespace sts2 {
struct SubroutinePower : PowerModel { SIM_MODEL(SubroutinePower, PowerModel)
    void BeforeCardPlayed(const CardPlay* play) const {
        if (play && play->Card.Owner() == Owner().Player() && play->Card.Type() == CardType::Power)
            CombatState().CaptureCardPlayPowerAmount(*this, play->Card, Amount());
    }
    void AfterCardPlayed(PlayerChoiceContext&, const CardPlay* play) const {
        if (!play || play->Card.Owner() != Owner().Player()) return;
        int32_t amount = 0;
        if (!CombatState().TakeCardPlayPowerAmount(*this, play->Card, amount) || amount <= 0) return;
        for (int32_t i = 0; i < amount; ++i) PlayerCmd::GainEnergy(Dec(1), Owner().Player());
    }
};
}
