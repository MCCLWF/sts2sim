// MonsterMoves/Intents/AttackIntent.cs (with SingleAttackIntent,
// MultiAttackIntent and DeathBlowIntent): the damage an attack intent shows.
// Only displays read it; the oracle's state lines include it.
#pragma once

#include <algorithm>

#include "game/All.h"
#include "game/Hooks/Hook.h"
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

namespace sts2::AttackIntent {

inline bool IsAttack(const Intent& intent) { return intent.type == IntentType::Attack || intent.type == IntentType::DeathBlow; }

// Repeats: 1 for single attacks and death blows.
inline int32_t Repeats(const Intent& intent, MonsterModel owner) {
    if (!intent.multi) return 1;
    return intent.repeatCalc != nullptr ? intent.repeatCalc(owner) : intent.repeat;
}

inline Dec DamageCalc(const Intent& intent, MonsterModel owner) {
    return intent.damageCalc != nullptr ? intent.damageCalc(owner) : Dec(intent.damage);
}

// GetSingleDamage(targets, owner). LocalContext.GetMe: the only player.
inline int32_t GetSingleDamage(const Intent& intent, const CreatureList& targets, Creature owner) {
    Dec num = DamageCalc(intent, owner.Monster());
    Player me(&owner.sim(), 1);
    if (me) {
        ModelList modifiers;
        num = Hook::ModifyDamage(me.RunState(), me.Creature().CombatState(), me.Creature(), owner, DamageCalc(intent, owner.Monster()),
                                 ValueProp::Move, CardModel{}, nullptr, ModifyDamageHookType::All, CardPreviewMode::None, modifiers);
    }
    return std::max(0, num.to_int());
}

}  // namespace sts2::AttackIntent
