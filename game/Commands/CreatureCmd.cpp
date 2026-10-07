// Commands/CreatureCmd.cs. VFX, SFX, screen shake, map-point history and
// waits are dropped.
#include "game/Commands/CreatureCmd.h"

#include <algorithm>
#include "game/Combat/CombatManager.h"
#include "game/Combat/History/CombatHistory.h"
#include "game/Commands/PlayerCmd.h"
#include "game/Hooks/Hook.h"
#include "game/Models/EncounterModel.h"

namespace sts2::CreatureCmd {

namespace {
CreatureList one(Creature c) {
    CreatureList l;
    l.push_back(c);
    return l;
}
Creature owner_creature(CardModel card) { return card ? card.Owner().Creature() : Creature{}; }
}  // namespace

Creature Add(MonsterModel monster, CombatState combatState, CombatSide side, std::string_view slotName) {
    int8_t slot = slotName.empty() ? int8_t{-1} : EncounterModel(&combatState.sim()).SlotIndex(slotName);
    Creature creature = combatState.CreateCreature(monster, side, slot);
    Add(creature);
    return creature;
}

void Add(Creature creature) {
    CombatManager cm(&creature.sim());
    if (!cm.IsInProgress()) throw GameError("Attempted to add a creature outside of combat.");
    CombatState combatState = creature.CombatState();
    if (!combatState) throw GameError("Attempted to add a creature with no combat state.");
    // IsLiveCombat() is always true for a real CombatState.
    combatState.AddCreature(creature);
    cm.AddCreature(creature);
    cm.AfterCreatureAdded(creature);
    if (combatState.CurrentSide() != CombatSide::Enemy && creature.IsMonster()) {
        CreatureList players;
        for (Player p : combatState.Players()) players.push_back(p.Creature());
        creature.PrepareForNextTurn(players, /*rollNewMove=*/false);
    }
    // CurrentMapPointHistoryEntry...MonsterIds: run history only.
    Hook::AfterCreatureAddedToCombat(creature.CombatState(), creature);
}

void Escape(Creature creature, bool removeCreatureNode) {
    if (creature.IsDead()) return;
    if (!creature.CombatState()) return;
    creature.RemoveAllPowersInternalExcept();
    CombatManager(&creature.sim()).RemoveCreature(creature);
    if (creature.CombatState()) creature.CombatState().CreatureEscaped(creature);
}

DamageResults Damage(PlayerChoiceContext& ctx, Creature target, DynamicVar damageVar, CardModel cardSource, const CardPlay* cardPlay) {
    return Damage(ctx, target, damageVar.BaseValue(), damageVar.Props(), cardSource, cardPlay);
}
DamageResults Damage(PlayerChoiceContext& ctx, Creature target, Dec amount, ValueProp props, CardModel cardSource, const CardPlay* cardPlay) {
    return Damage(ctx, one(target), amount, props, owner_creature(cardSource), cardSource, cardPlay);
}
DamageResults Damage(PlayerChoiceContext& ctx, const CreatureList& targets, DynamicVar damageVar, Creature dealer) {
    return Damage(ctx, targets, damageVar.BaseValue(), damageVar.Props(), dealer);
}
DamageResults Damage(PlayerChoiceContext& ctx, const CreatureList& targets, Dec amount, ValueProp props, Creature dealer) {
    return Damage(ctx, targets, amount, props, dealer, CardModel{}, nullptr);
}
DamageResults Damage(PlayerChoiceContext& ctx, Creature target, DynamicVar damageVar, Creature dealer) {
    return Damage(ctx, target, damageVar.BaseValue(), damageVar.Props(), dealer);
}
DamageResults Damage(PlayerChoiceContext& ctx, Creature target, Dec amount, ValueProp props, Creature dealer) {
    return Damage(ctx, one(target), amount, props, dealer, CardModel{}, nullptr);
}
DamageResults Damage(PlayerChoiceContext& ctx, Creature target, DynamicVar damageVar, Creature dealer, CardModel cardSource, const CardPlay* cardPlay) {
    return Damage(ctx, one(target), damageVar.BaseValue(), damageVar.Props(), dealer, cardSource, cardPlay);
}
DamageResults Damage(PlayerChoiceContext& ctx, Creature target, Dec amount, ValueProp props, Creature dealer, CardModel cardSource, const CardPlay* cardPlay) {
    return Damage(ctx, one(target), amount, props, dealer, cardSource, cardPlay);
}
DamageResults Damage(PlayerChoiceContext& ctx, const CreatureList& targets, DynamicVar damageVar, Creature dealer, CardModel cardSource, const CardPlay* cardPlay) {
    return Damage(ctx, targets, damageVar.BaseValue(), damageVar.Props(), dealer, cardSource, cardPlay);
}

DamageResults Damage(PlayerChoiceContext& choiceContext, const CreatureList& targets, Dec amount, ValueProp props,
                     Creature dealer, CardModel cardSource, const CardPlay* cardPlay) {
    DamageResults results;
    if (dealer && dealer.IsDead()) {
        for (Creature t : targets) results.push_back(DamageResult(t, props));
        return results;
    }
    CreatureList targetList = targets;
    if (targetList.empty()) return results;
    Sim* sim = &targetList[0].sim();
    CombatState combatState = targetList[0].CombatState();
    RunState runState(sim);  // IRunState.GetFrom(targets + dealer)
    CombatManager cm(sim);
    for (Creature originalTarget : targetList) {
        if (originalTarget.IsDead()) continue;
        ModelList modifiers;
        Dec modifiedAmount = Hook::ModifyDamage(runState, combatState, originalTarget, dealer, amount, props, cardSource,
                                                cardPlay, ModifyDamageHookType::All, CardPreviewMode::None, modifiers);
        Hook::AfterModifyingDamageAmount(runState, combatState, cardSource, modifiers);
        Hook::BeforeDamageReceived(choiceContext, runState, combatState, originalTarget, modifiedAmount, props, dealer, cardSource);
        Creature creature = originalTarget.PetOwner() ? originalTarget.PetOwner().Creature() : originalTarget;
        Dec blockedDamage = creature.DamageBlockInternal(modifiedAmount, props);
        Dec unblockedDamage = Hook::ModifyHpLost(runState, combatState, originalTarget, dec_max(modifiedAmount - blockedDamage, Dec(0)),
                                                 props, dealer, cardSource, HpLossHookPhase::BeforeOsty, modifiers);
        Hook::AfterModifyingHpLostBeforeOsty(runState, combatState, modifiers);
        Creature unblockedDamageTarget = !combatState ? originalTarget
                                                      : Hook::ModifyUnblockedDamageTarget(combatState, originalTarget, unblockedDamage, props, dealer);
        unblockedDamage = Hook::ModifyHpLost(runState, combatState, unblockedDamageTarget, unblockedDamage, props, dealer, cardSource,
                                             HpLossHookPhase::AfterOsty, modifiers);
        Hook::AfterModifyingHpLostAfterOsty(runState, combatState, modifiers);
        DamageResult unblockedDamageResult = unblockedDamageTarget.LoseHpInternal(unblockedDamage, props);
        StaticVec<DamageResult, 2> damageResults;
        bool wasBlockBroken = originalTarget.Block() <= 0 && blockedDamage > Dec(0);
        bool wasFullyBlocked = !has_flag(props, ValueProp::Unblockable) && (blockedDamage > Dec(0) || originalTarget.Block() > 0) &&
                               unblockedDamage.to_int() == 0;
        if (originalTarget == unblockedDamageTarget) {
            unblockedDamageResult.BlockedDamage = blockedDamage.to_int();
            unblockedDamageResult.WasBlockBroken = wasBlockBroken;
            unblockedDamageResult.WasFullyBlocked = wasFullyBlocked;
            damageResults.push_back(unblockedDamageResult);
        } else {
            damageResults.push_back(unblockedDamageResult);
            Dec originalTargetDamage = Hook::ModifyHpLost(runState, combatState, originalTarget, Dec(unblockedDamageResult.OverkillDamage),
                                                          props, dealer, cardSource, HpLossHookPhase::AfterOsty, modifiers);
            Hook::AfterModifyingHpLostAfterOsty(runState, combatState, modifiers);
            DamageResult damageResult = !(originalTargetDamage > Dec(0)) ? DamageResult(originalTarget, props)
                                                                         : originalTarget.LoseHpInternal(originalTargetDamage, props);
            damageResult.BlockedDamage = blockedDamage.to_int();
            damageResult.WasBlockBroken = wasBlockBroken;
            damageResult.WasFullyBlocked = wasFullyBlocked;
            damageResults.push_back(damageResult);
        }
        for (const DamageResult& item : damageResults) {
            // Observe the actual result independently of vanilla History.
            // Lethal hits remain visible without changing its ending guard.
            if (auto* trace = sim->execution_trace) {
                ExecutionEvent event;
                event.kind = ExecutionEventKind::DamageResolved;
                event.creature = item.Receiver.Index();
                event.creature_combat_id = item.Receiver.CombatId();
                event.dealer = dealer ? dealer.Index() : 0;
                event.dealer_combat_id = dealer ? dealer.CombatId() : UINT32_MAX;
                event.card = cardSource ? cardSource.Index() : 0;
                event.card_id = cardSource ? static_cast<uint16_t>(cardSource.CardId_()) : 0;
                event.blocked = item.BlockedDamage;
                event.unblocked = item.UnblockedDamage;
                event.overkill = item.OverkillDamage;
                event.props = static_cast<int32_t>(item.Props);
                event.block_broken = item.WasBlockBroken;
                event.fully_blocked = item.WasFullyBlocked;
                event.target_killed = item.WasTargetKilled;
                event.damage_source_kind = sim->damage_source_kind;
                event.damage_source_id = sim->damage_source_id;
                trace->Record(std::move(event));
            }
            if (cm.IsInProgress() && !cm.IsEnding()) {
                cm.History().DamageReceived(combatState, item.Receiver, dealer, item, cardSource);
            }
        }
        for (const DamageResult& r : damageResults) results.push_back(r);
    }
    CreatureList killedCreatures;
    for (const DamageResult& unblockedDamageResult : results) {
        Creature originalTarget = unblockedDamageResult.Receiver;
        if (unblockedDamageResult.WasBlockBroken) {
            Hook::AfterBlockBroken(originalTarget.CombatState(), choiceContext, originalTarget, dealer);
        }
        if (unblockedDamageResult.UnblockedDamage > 0) {
            Hook::AfterCurrentHpChanged(runState, combatState, originalTarget, Dec(-unblockedDamageResult.UnblockedDamage));
        }
        if (dealer && dealer.Player() && !originalTarget.Player()) {
            dealer.Player().st().damage_dealt += unblockedDamageResult.UnblockedDamage;
        }
        if (combatState) {
            Hook::AfterDamageGiven(choiceContext, combatState, dealer, unblockedDamageResult, props, originalTarget, cardSource);
        }
        if (!unblockedDamageResult.WasTargetKilled || !originalTarget.IsDead()) {
            Hook::AfterDamageReceived(choiceContext, runState, combatState, originalTarget, unblockedDamageResult, props, dealer, cardSource);
        } else {
            killedCreatures.push_back(originalTarget);
        }
    }
    Kill(killedCreatures);
    return results;
}

namespace {
void KillWithoutCheckingWinCondition(Creature creature, bool force, int recursion = 0) {
    Sim* sim = &creature.sim();
    CombatManager cm(sim);
    if ((!creature.CombatState() && !creature.IsPlayer())) return;  // CombatState.IsLiveCombat() is always true
    if (!cm.IsInProgress() && creature.IsPlayer() && creature.Player().RunState().Players().size() > 1 && !force) {
        Heal(creature, Dec(1));
        return;
    }
    CombatState combatState = creature.CombatState();
    RunState runState(sim);
    int32_t currentHp = creature.CurrentHp();
    if (currentHp > 0) {
        creature.LoseHpInternal(Dec(currentHp), ValueProp::Unblockable | ValueProp::Unpowered);
        Hook::AfterCurrentHpChanged(runState, creature.CombatState(), creature, Dec(-currentHp));
    }
    Hook::BeforeDeath(runState, combatState, creature);
    AbstractModel preventer;
    if (force || creature.MaxHp() <= 0 || Hook::ShouldDie(runState, combatState, creature, preventer)) {
        creature.InvokeDiedEvent();
        bool shouldRemoveFromCombat = combatState && Hook::ShouldCreatureBeRemovedFromCombatAfterDeath(combatState, creature);
        float deathAnimLength = 0.0f;  // no creature node headless
        Hook::AfterDeath(runState, combatState, creature, /*wasRemovalPrevented=*/false, deathAnimLength);
        CreatureList teammates;
        if (combatState) {
            for (Creature t : combatState.GetTeammatesOf(creature)) {
                if (t.IsAlive()) teammates.push_back(t);
            }
        }
        if (shouldRemoveFromCombat && creature.Side() == CombatSide::Enemy && combatState &&
            combatState.st().enemies.contains(creature.Index())) {
            cm.RemoveCreature(creature);
            MonsterModel monster = creature.Monster();
            if (monster && !monster.IsPerformingMove()) combatState.RemoveCreature(creature);
        }
        bool isPrimaryEnemy = creature.IsPrimaryEnemy();
        for (PowerModel item : creature.RemoveAllPowersAfterDeath()) item.AfterRemoved(creature);
        if (creature.Side() == CombatSide::Enemy) {
            bool allSecondary = true;
            for (Creature t : teammates) allSecondary &= t.IsSecondaryEnemy();
            if (isPrimaryEnemy && !teammates.empty() && allSecondary) Kill(teammates);
        } else if (creature.IsPlayer()) {
            Player player = creature.Player();
            if (player.PlayerCombatState()) {
                player.PlayerCombatState().st().orbs.clear();
                player.PlayerCombatState().st().orb_capacity = 0;
            }
            Creature osty = player.PlayerCombatState() ? player.PlayerCombatState().GetPet(MonsterId::OSTY) : Creature{};
            if (osty && osty.IsAlive()) Kill(osty, force);
            player.DeactivateHooks();
            if (combatState) {
                // CombatManager.HandlePlayerDeath: only while another player lives.
            }
        }
    } else {
        if (recursion >= 10) throw GameError("something is continually preventing the last creature from being killed");
        Hook::AfterDeath(runState, combatState, creature, /*wasRemovalPrevented=*/true, 0.0f);
        Hook::AfterPreventingDeath(runState, combatState, preventer, creature);
        if (creature.IsDead()) KillWithoutCheckingWinCondition(creature, force, recursion + 1);
    }
}
}  // namespace

void Kill(Creature creature, bool force) { Kill(one(creature), force); }

void Kill(const CreatureList& creatures, bool force) {
    if (creatures.empty()) return;
    Sim* sim = &creatures[0].sim();
    bool anyPlayer = false;
    for (Creature c : creatures) anyPlayer |= c.IsPlayer();
    CreatureList list = creatures;
    for (Creature item : list) KillWithoutCheckingWinCondition(item, force);
    CombatManager cm(sim);
    RunState runState(sim);
    bool allPlayersDead = true;
    for (Player p : runState.Players()) allPlayersDead &= p.Creature().IsDead();
    if (anyPlayer && allPlayersDead) {
        if (cm.IsInProgress()) cm.LoseCombat();
        // RunManager.OnEnded (TestMode skips it): the run is over.
    } else {
        if (!cm.IsInProgress()) return;
        for (Creature item2 : list) {
            CombatState combatState = item2.CombatState();
            if (combatState && combatState.CurrentSide() == CombatSide::Player && item2.IsDead() && item2.IsPlayer()) {
                PlayerCmd::EndTurn(item2.Player(), /*canBackOut=*/false);
            }
        }
    }
}

Dec GainBlock(Creature creature, DynamicVar blockVar, const CardPlay* cardPlay, bool fast) {
    return GainBlock(creature, blockVar.BaseValue(), blockVar.Props(), cardPlay, fast);
}

Dec GainBlock(Creature creature, Dec amount, ValueProp props, const CardPlay* cardPlay, bool fast) {
    CombatManager cm(&creature.sim());
    if (cm.IsOverOrEnding()) return Dec(0);
    if (creature.IsDead()) return Dec(0);
    CombatState combatState = creature.CombatState();
    CardModel card = cardPlay ? cardPlay->Card : CardModel{};
    Hook::BeforeBlockGained(combatState, creature, amount, props, card);
    ModelList modifiers;
    Dec modifiedAmount = Hook::ModifyBlock(combatState, creature, amount, props, card, cardPlay, modifiers);
    modifiedAmount = dec_max(modifiedAmount, Dec(0));
    Hook::AfterModifyingBlockAmount(combatState, modifiedAmount, card, cardPlay, modifiers);
    if (modifiedAmount > Dec(0)) {
        creature.GainBlockInternal(modifiedAmount);
        cm.History().BlockGained(combatState, creature, modifiedAmount.to_int(), props, cardPlay);
    }
    Hook::AfterBlockGained(combatState, creature, modifiedAmount, props, card);
    return modifiedAmount;
}

void LoseBlock(PlayerChoiceContext& choiceContext, Creature target, Dec amount, Creature remover) {
    CombatManager cm(&target.sim());
    if (!cm.IsOverOrEnding() && !target.IsDead() && !(amount <= Dec(0))) {
        int32_t block = target.Block();
        target.LoseBlockInternal(amount);
        if (block > 0 && target.Block() <= 0) Hook::AfterBlockBroken(target.CombatState(), choiceContext, target, remover);
    }
}

void Heal(Creature creature, Dec amount, bool playAnim) {
    CombatManager cm(&creature.sim());
    if (cm.IsEnding() && !creature.IsPlayer()) return;
    creature.HealInternal(amount);
    if (amount > Dec(0) && creature.CombatState()) {
        Hook::AfterCurrentHpChanged(RunState(&creature.sim()), creature.CombatState(), creature, amount);
    }
}

void SetCurrentHp(Creature creature, Dec amount) {
    Dec num(creature.CurrentHp());
    creature.SetCurrentHpInternal(amount);
    if (amount != num) Hook::AfterCurrentHpChanged(RunState(&creature.sim()), creature.CombatState(), creature, amount - num);
    if (creature.IsDead()) Kill(creature);
}

void GainMaxHp(Creature creature, Dec amount) {
    if (amount < Dec(0)) throw GameError("amount must be non-negative. Use LoseMaxHp for max HP loss.");
    Dec num = SetMaxHp(creature, Dec(creature.MaxHp()) + amount);
    Heal(creature, num);
}

void LoseMaxHp(PlayerChoiceContext& choiceContext, Creature creature, Dec amount, bool isFromCard) {
    if (amount < Dec(0)) throw GameError("amount must be non-negative. Use GainMaxHp for max HP gain.");
    Dec newMaxHp = Dec(creature.MaxHp()) - amount;
    if (newMaxHp < Dec(creature.CurrentHp())) {
        ValueProp props = isFromCard ? (ValueProp::Unblockable | ValueProp::Unpowered | ValueProp::Move)
                                     : (ValueProp::Unblockable | ValueProp::Unpowered);
        Damage(choiceContext, creature, Dec(creature.CurrentHp()) - newMaxHp, props, Creature{}, CardModel{}, nullptr);
    }
    SetMaxHp(creature, dec_max(Dec::lit(10, 1), newMaxHp));  // Math.Max(1.0m, newMaxHp)
}

Dec SetMaxHp(Creature creature, Dec amount) {
    int32_t oldMaxHp = creature.MaxHp();
    creature.SetMaxHpInternal(dec_max(Dec(0), amount));
    int32_t newMaxHp = creature.MaxHp();
    if (creature.MaxHp() <= 0) Kill(creature);
    return Dec(newMaxHp - oldMaxHp);
}

void SetMaxAndCurrentHp(Creature creature, Dec amount) {
    SetMaxHp(creature, amount);
    SetCurrentHp(creature, amount);
}

void Stun(Creature creature, std::string_view nextMoveId) { Stun(creature, nullptr, nextMoveId); }

void Stun(Creature creature, MoveFn stunMove, std::string_view nextMoveId) { creature.StunInternal(stunMove, nextMoveId); }

}  // namespace sts2::CreatureCmd
