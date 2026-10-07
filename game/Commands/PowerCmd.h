// Commands/PowerCmd.cs. The generic Apply<T> / Remove<T> take the power id as
// their first argument: PowerCmd.Apply<VulnerablePower>(ctx, t, 2m, a, c)
// becomes PowerCmd::Apply(PowerId::VULNERABLE_POWER, ctx, t, Dec(2), a, c).
#pragma once

#include "game/All.h"

namespace sts2::PowerCmd {

StaticVec<PowerModel, kMaxCreatureList> Apply(PowerId power, PlayerChoiceContext& choiceContext, const CreatureList& targets,
                                              Dec amount, Creature applier, CardModel cardSource, bool silent = false);
PowerModel Apply(PowerId power, PlayerChoiceContext& choiceContext, Creature target, Dec amount, Creature applier,
                 CardModel cardSource, bool silent = false);
void Apply(PlayerChoiceContext& choiceContext, PowerModel power, Creature target, Dec amount, Creature applier,
           CardModel cardSource, bool silent = false);
PowerModel FindExistingInstanceForStacking(PowerId basePower, Creature target, Creature applier);
void Decrement(PowerModel power);
void TickDownDuration(PowerModel power);
int32_t ModifyAmount(PlayerChoiceContext& choiceContext, PowerModel power, Dec offset, Creature applier, CardModel cardSource,
                     bool silent = false);
void Remove(PowerId power, Creature creature);
void Remove(PowerModel power);

}  // namespace sts2::PowerCmd
