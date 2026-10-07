// Models/Cards/Capacitor.cs
#pragma once
#include "game/Commands/OrbCmd.h"
#include "game/Models/CardModel.h"
namespace sts2 {
struct Capacitor : CardModel { SIM_MODEL(Capacitor, CardModel)
    void OnPlay(PlayerChoiceContext&, const CardPlay*) const { OrbCmd::AddSlots(Owner(), DynamicVars().Repeat().IntValue()); }
};
}
