// Commands/CreatureCmd.cs
#pragma once

#include <string_view>

#include "game/All.h"

namespace sts2 {

using DamageResults = StaticVec<DamageResult, 32>;

namespace CreatureCmd {

// Add<T>(combatState, slotName) is Add(MonsterModel::ToMutable(sim, id), ...).
Creature Add(MonsterModel monster, CombatState combatState, CombatSide side = CombatSide::Enemy,
             std::string_view slotName = {});
void Add(Creature creature);
void Escape(Creature creature, bool removeCreatureNode = true);

// Damage: every overload ends in the one taking a target list.
DamageResults Damage(PlayerChoiceContext& choiceContext, Creature target, DynamicVar damageVar, CardModel cardSource, const CardPlay* cardPlay);
DamageResults Damage(PlayerChoiceContext& choiceContext, Creature target, Dec amount, ValueProp props, CardModel cardSource, const CardPlay* cardPlay);
DamageResults Damage(PlayerChoiceContext& choiceContext, const CreatureList& targets, DynamicVar damageVar, Creature dealer);
DamageResults Damage(PlayerChoiceContext& choiceContext, const CreatureList& targets, Dec amount, ValueProp props, Creature dealer);
DamageResults Damage(PlayerChoiceContext& choiceContext, Creature target, DynamicVar damageVar, Creature dealer);
DamageResults Damage(PlayerChoiceContext& choiceContext, Creature target, Dec amount, ValueProp props, Creature dealer);
DamageResults Damage(PlayerChoiceContext& choiceContext, Creature target, DynamicVar damageVar, Creature dealer, CardModel cardSource, const CardPlay* cardPlay);
DamageResults Damage(PlayerChoiceContext& choiceContext, Creature target, Dec amount, ValueProp props, Creature dealer, CardModel cardSource, const CardPlay* cardPlay);
DamageResults Damage(PlayerChoiceContext& choiceContext, const CreatureList& targets, DynamicVar damageVar, Creature dealer, CardModel cardSource, const CardPlay* cardPlay);
DamageResults Damage(PlayerChoiceContext& choiceContext, const CreatureList& targets, Dec amount, ValueProp props, Creature dealer, CardModel cardSource, const CardPlay* cardPlay);

void Kill(Creature creature, bool force = false);
void Kill(const CreatureList& creatures, bool force = false);

Dec GainBlock(Creature creature, DynamicVar blockVar, const CardPlay* cardPlay, bool fast = false);
Dec GainBlock(Creature creature, Dec amount, ValueProp props, const CardPlay* cardPlay, bool fast = false);
void LoseBlock(PlayerChoiceContext& choiceContext, Creature target, Dec amount, Creature remover);
void Heal(Creature creature, Dec amount, bool playAnim = true);
void SetCurrentHp(Creature creature, Dec amount);
void GainMaxHp(Creature creature, Dec amount);
void LoseMaxHp(PlayerChoiceContext& choiceContext, Creature creature, Dec amount, bool isFromCard);
Dec SetMaxHp(Creature creature, Dec amount);
void SetMaxAndCurrentHp(Creature creature, Dec amount);
void Stun(Creature creature, std::string_view nextMoveId = {});
void Stun(Creature creature, MoveFn stunMove, std::string_view nextMoveId = {});
inline void TriggerAnim(Creature, std::string_view, float) {}  // animation only

}  // namespace CreatureCmd
}  // namespace sts2
