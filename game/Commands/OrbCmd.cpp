// Commands/OrbCmd.cs and Entities/Orbs/OrbQueue.cs
#include "game/Commands/OrbCmd.h"

#include <algorithm>

#include "game/Combat/CombatManager.h"
#include "game/Combat/History/CombatHistory.h"
#include "game/Hooks/Hook.h"

namespace sts2::OrbCmd {

void AddSlots(Player player, int32_t amount) {
    if (CombatManager(&player.sim()).IsOverOrEnding()) return;
    PlayerCombatData& state = player.PlayerCombatState().st();
    amount = std::min(10 - state.orb_capacity, amount);
    state.orb_capacity += amount;
}

void RemoveSlots(Player player, int32_t amount) {
    if (CombatManager(&player.sim()).IsOverOrEnding()) return;
    PlayerCombatData& state = player.PlayerCombatState().st();
    amount = std::min(state.orb_capacity, amount);
    state.orb_capacity = std::max(0, state.orb_capacity - amount);
    while (state.orbs.size() > static_cast<size_t>(state.orb_capacity)) state.orbs.pop_back();
}

namespace {
void Evoke(PlayerChoiceContext& choiceContext, Player player, OrbModel orb, bool dequeue) {
    if (CombatManager(&player.sim()).IsOverOrEnding()) return;
    auto& orbs = player.PlayerCombatState().st().orbs;
    if (orbs.empty()) return;
    bool removed = false;
    if (dequeue) removed = orbs.remove(orb.Index());
    choiceContext.PushModel(orb);
    CreatureList targets = orb.Evoke(choiceContext);
    choiceContext.PopModel(orb);
    if (player.Creature().CombatState()) {
        Hook::AfterOrbEvoked(choiceContext, player.Creature().CombatState(), orb, targets);
        if (removed) orb.RemoveInternal();
    }
}
}  // namespace

void EvokeNext(PlayerChoiceContext& choiceContext, Player player, bool dequeue) {
    const auto& orbs = player.PlayerCombatState().st().orbs;
    if (orbs.empty()) return;
    OrbModel orb(&player.sim(), orbs[0]);
    choiceContext.PushModel(orb);
    Evoke(choiceContext, player, orb, dequeue);
    choiceContext.PopModel(orb);
}

void EvokeLast(PlayerChoiceContext& choiceContext, Player player, bool dequeue) {
    const auto& orbs = player.PlayerCombatState().st().orbs;
    if (orbs.empty()) return;
    OrbModel orb(&player.sim(), orbs.back());
    choiceContext.PushModel(orb);
    Evoke(choiceContext, player, orb, dequeue);
    choiceContext.PopModel(orb);
}

void Channel(OrbId id, PlayerChoiceContext& choiceContext, Player player) {
    OrbRef ref = player.sim().g->orbs.alloc();  // Channel<T> creates a mutable orb before Channel.
    OrbModel orb(&player.sim(), ref);
    orb.st().id = id;
    if (CombatManager(&player.sim()).IsOverOrEnding()) {
        player.sim().g->orbs.free(ref);
        return;
    }
    PlayerCombatData& state = player.PlayerCombatState().st();
    CombatState combat = player.Creature().CombatState();
    if (player.st().base_orb_slots == 0 && state.orb_capacity == 0) AddSlots(player, 1);
    orb.st().owner = player.Index();
    if (state.orbs.size() >= static_cast<size_t>(state.orb_capacity)) EvokeNext(choiceContext, player);
    if (state.orb_capacity == 0) return;
    if (state.orbs.size() >= static_cast<size_t>(state.orb_capacity)) throw GameError("OrbQueue is full");
    state.orbs.push_back(ref);
    if (auto* boundary = player.sim().execution_trace) {
        ExecutionEvent event;
        event.kind = ExecutionEventKind::OrbChanneled;
        event.card = orb.Index();
        event.card_id = static_cast<uint16_t>(id);
        event.owner = player.Index();
        event.flag = id == OrbId::LIGHTNING_ORB;
        boundary->Record(event);
    }
    CombatManager(&player.sim()).History().OrbChanneled(combat, orb);
    Hook::AfterOrbChanneled(combat, choiceContext, player, orb);
}

void Passive(PlayerChoiceContext& choiceContext, OrbModel orb, Creature target, bool countAffectedByHooks) {
    if (CombatManager(&orb.sim()).IsOverOrEnding()) return;
    choiceContext.PushModel(orb);
    if (countAffectedByHooks) orb.TriggerPassive(choiceContext, target);
    else orb.Passive(choiceContext, target);
    choiceContext.PopModel(orb);
}

void BeforeTurnEnd(PlayerChoiceContext& choiceContext, Player player) {
    auto orbs = player.PlayerCombatState().st().orbs;
    for (OrbRef ref : orbs) {
        if (!player.Creature().CombatState()) return;
        OrbModel(&player.sim(), ref).BeforeTurnEndOrbTrigger(choiceContext);
    }
}

void AfterTurnStart(PlayerChoiceContext& choiceContext, Player player) {
    auto orbs = player.PlayerCombatState().st().orbs;
    for (OrbRef ref : orbs) {
        if (!player.Creature().CombatState()) return;
        OrbModel(&player.sim(), ref).AfterTurnStartOrbTrigger(choiceContext);
    }
}

}  // namespace sts2::OrbCmd
