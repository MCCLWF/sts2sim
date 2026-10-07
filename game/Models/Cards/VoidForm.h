// Models/Cards/VoidForm.cs
#pragma once

#include "game/Commands/PlayerCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Models/CardModel.h"

namespace sts2 {

struct VoidForm : CardModel {
    SIM_MODEL(VoidForm, CardModel)

    void OnPlay(PlayerChoiceContext& choiceContext, const CardPlay*) const {
        // CreatureCmd.TriggerAnim is presentation only.
        PowerCmd::Apply(PowerId::VOID_FORM_POWER, choiceContext, Owner().Creature(),
                        DynamicVars()["VoidFormPower"].BaseValue(), Owner().Creature(), *this);
        PlayerCmd::EndTurn(Owner(), false);
    }
    // OnUpgrade's Ethereal removal is covered by the generated upgrade table.
};

}  // namespace sts2
