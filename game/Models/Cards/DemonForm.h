// Models/Cards/DemonForm.cs
#include "game/Commands/PowerCmd.h"
#pragma once
#include "game/All.h"
#include "game/Commands/PlayerCmd.h"
namespace sts2 {
struct DemonForm : CardModel { SIM_MODEL(DemonForm, CardModel)
void OnPlay(PlayerChoiceContext& ctx, const CardPlay*) const { PowerCmd::Apply(PowerId::DEMON_FORM_POWER,ctx,Owner().Creature(),DynamicVars()["StrengthPower"].BaseValue(),Owner().Creature(),*this); }
};
}
