#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Hibernate : CardModel { SIM_MODEL(Hibernate, CardModel)
    void OnPlay(PlayerChoiceContext& ctx, const CardPlay* play) const {
        PowerCmd::Apply(PowerId::HIBERNATE_POWER, ctx, Owner().Creature(), Dec(1), Owner().Creature(), *this);
        for (int32_t i = 0; i < DynamicVars().Repeat().IntValue(); ++i) OrbCmd::Channel(OrbId::FROST_ORB, ctx, Owner());
    }
};
}
