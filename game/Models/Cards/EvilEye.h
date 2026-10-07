// Models/Cards/EvilEye.cs
#include "game/Commands/CreatureCmd.h"
#pragma once
#include "game/All.h"
namespace sts2 {
struct EvilEye : CardModel { SIM_MODEL(EvilEye, CardModel)
bool GainsBlock() const { return true; }
void OnPlay(PlayerChoiceContext& ctx, const CardPlay* p) const { int32_t n=CombatState().st().player_cards_exhausted_this_turn > 0 ? 2 : 1; for(int32_t i=0;i<n;++i) CreatureCmd::GainBlock(Owner().Creature(),DynamicVars().Block(),p); }
};
}
