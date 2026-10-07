// Models/MonsterModel.cs and the run-time half of
// MonsterMoves/MonsterMoveStateMachine/*.cs (MonsterMoveStateMachine,
// MoveState, RandomBranchState, ConditionalBranchState).
#include "game/Models/MonsterModel.h"

#include <string>

#include "game/All.h"
#include "game/Combat/CombatManager.h"
#include "game/Hooks/Hook.h"

namespace sts2 {

namespace {
constexpr int8_t kStunned = MonsterModel::stunnedStateIndex;
}

// ------------------------------------------------------------- NextMove

MonsterModel NextMove::Monster() const { return MonsterModel(sim_, creature_); }

std::string_view NextMove::Id() const {
    MonsterModel m = Monster();
    const MonsterState& s = m.st();
    if (s.next_move.stunned) return MonsterModel::stunnedMoveId;
    if (s.next_move.state < 0) return "UNSET_MOVE";
    return move_machine_def(s.machine.definition).states[static_cast<size_t>(s.next_move.state)].id;
}

bool NextMove::IsStunned() const { return Monster().st().next_move.stunned; }

const MonsterStateDef* NextMove::Def() const {
    MonsterModel m = Monster();
    const MonsterState& s = m.st();
    if (s.next_move.stunned || s.next_move.state < 0) return nullptr;
    return &move_machine_def(s.machine.definition).states[static_cast<size_t>(s.next_move.state)];
}

StaticVec<Intent, 4> NextMove::Intents() const {
    StaticVec<Intent, 4> out;
    if (IsStunned()) {
        out.push_back(StunIntent());
    } else if (const MonsterStateDef* d = Def()) {
        for (const Intent& i : d->intents) out.push_back(i);
    }
    return out;
}

// MoveState.CanTransitionAway
bool NextMove::CanTransitionAway() const {
    MonsterModel m = Monster();
    const MonsterState& s = m.st();
    if (s.next_move.stunned) return s.next_move.stun_performed;
    const MonsterStateDef* d = Def();
    if (d == nullptr || !d->mustPerformOnceBeforeTransitioning) return true;
    return (s.machine.performed_at_least_once >> s.next_move.state) & 1u;
}

// ----------------------------------------------------------- MonsterModel

Creature MonsterModel::Creature() const { return sts2::Creature(sim_, index_); }

// C#: Creature.CombatState ?? NullCombatState.Instance
sts2::CombatState MonsterModel::CombatState() const { return Creature().CombatState(); }

bool MonsterModel::IntendsToAttack() const {
    for (const Intent& i : NextMove().Intents()) {
        if (i.type == IntentType::Attack || i.type == IntentType::DeathBlow) return true;
    }
    return false;
}

const MoveMachineDef& MonsterModel::MachineDef() const {
    if (!st().machine.exists) throw GameError("monster has no move state machine");
    return move_machine_def(st().machine.definition);
}

// MonsterModel.SetUpForCombat: MoveStateMachine = GenerateMoveStateMachine();
// SpawnedThisTurn = true. The machine's constructor starts in the initial
// state and logs it when it is a move.
void MonsterModel::SetUpForCombat() {
    MoveStateMachineState& m = st().machine;
    if (m.exists) throw GameError("move state machine has already been set");
    MoveMachineDef def = GenerateMoveStateMachine();
    m = MoveStateMachineState{};
    m.exists = true;
    m.definition = intern_move_machine(std::move(def), MonsterId_());
    for (int8_t& f : m.follow_up) f = -1;
    const MoveMachineDef& d = MachineDef();
    m.current = d.initial;
    if (d.states[static_cast<size_t>(d.initial)].kind == MonsterStateKind::Move) m.log.Append(d.initial, true);
    st().spawned_this_turn = true;
}

// MonsterModel.RollMove -> MonsterMoveStateMachine.RollMove(targets, Creature, RunRng.MonsterAi)
void MonsterModel::RollMove(const CreatureList& targets) {
    FindNextMoveState(Creature(), RunRng().MonsterAi(), /*logMove=*/true);
    MoveStateMachineState& m = st().machine;
    // STUNNED is a synthetic move state in the source state machine.  Its
    // follow-up and callback live in next_move, so keep that state intact until
    // PerformMove consumes it instead of treating the sentinel as invalid.
    if (m.current == kStunned) return;
    if (m.current < 0 || MachineDef().states[static_cast<size_t>(m.current)].kind != MonsterStateKind::Move) {
        throw GameError("current monster state is not a valid move state");
    }
    st().next_move = NextMoveState{};
    st().next_move.state = m.current;
}

// MonsterMoveStateMachine.FindNextMoveState
void MonsterModel::FindNextMoveState(sts2::Creature owner, sts2::Rng& rng, bool logMove) {
    MoveStateMachineState& m = st().machine;
    const MoveMachineDef& d = MachineDef();
    auto is_move = [&](int8_t s) { return s == kStunned || d.states[static_cast<size_t>(s)].kind == MonsterStateKind::Move; };
    auto can_transition_away = [&](int8_t s) -> bool {
        if (s == kStunned) return st().next_move.stun_performed;  // STUNNED: MustPerformOnceBeforeTransitioning
        const MonsterStateDef& sd = d.states[static_cast<size_t>(s)];
        if (sd.kind != MonsterStateKind::Move || !sd.mustPerformOnceBeforeTransitioning) return true;
        return (m.performed_at_least_once >> s) & 1u;
    };
    if (m.current == -1) throw GameError("Cannot find next move state when current state is null.");
    if (!can_transition_away(m.current) || (!m.performed_first_move && is_move(m.current))) return;
    int8_t logged = -1;
    do {
        int8_t next = GetNextState(m.current, rng);  // -1: null or empty id -> initial state
        SetCurrentState(next < 0 ? d.initial : next);
        if (logged == -1 && is_move(m.current) && m.current != kStunned) logged = m.current;  // ShouldAppearInLogs
    } while (!is_move(m.current));
    if (logMove && logged != -1) m.log.Append(logged, d.states[static_cast<size_t>(logged)].kind == MonsterStateKind::Move);
    (void)owner;
}

// MonsterState.GetNextState, returning the state index (-1 = initial state).
int8_t MonsterModel::GetNextState(int8_t state, sts2::Rng& rng) {
    MoveStateMachineState& m = st().machine;
    const MoveMachineDef& d = MachineDef();
    if (state == kStunned) {
        // MoveState.GetNextState: FollowUpState?.Id ?? FollowUpStateId
        if (st().next_move.stun_follow_up < 0) throw GameError("no valid state found for the STUNNED follow-up");
        return st().next_move.stun_follow_up;
    }
    const MonsterStateDef& s = d.states[static_cast<size_t>(state)];
    switch (s.kind) {
    case MonsterStateKind::Move: {
        int8_t f = m.follow_up[state] >= 0 ? m.follow_up[state] : s.followUp;
        if (f >= 0) return f;
        if (!s.followUpId.empty()) throw GameError("no valid state found: " + s.followUpId);
        throw GameError("No valid followup state.");
    }
    case MonsterStateKind::RandomBranch: {
        // float max = States.Sum(x => GetStateWeight(x, owner)): Enumerable.Sum over
        // floats accumulates in double.
        double sum = 0.0;
        for (const StateWeight& w : s.weights) sum += static_cast<double>(GetStateWeight(w));
        float num = rng.NextFloat(static_cast<float>(sum));
        for (const StateWeight& w : s.weights) {
            num -= GetStateWeight(w);
            if (num <= 0.0f) {
                if (w.state < 0) throw GameError("no valid state found in RandomBranchState " + s.id);
                return w.state;
            }
        }
        throw GameError("No valid state found in RandomBranchState " + s.id + "!");
    }
    case MonsterStateKind::ConditionalBranch:
        for (const ConditionalBranch& b : s.branches) {
            bool take = b.condition == nullptr || b.condition(*this);  // Evaluate() > 0
            if (take) {
                if (b.state < 0) throw GameError("no valid state found in ConditionalBranchState " + s.id);
                return b.state;
            }
        }
        throw GameError("No valid next state found.");
    }
    throw GameError("unknown monster state kind");
}

// RandomBranchState.GetStateWeight
float MonsterModel::GetStateWeight(const StateWeight& w) {
    const MoveStateMachineState& m = st().machine;
    if (w.state < 0) throw GameError("RandomBranchState weight names a state outside the machine");
    const auto& log = m.log;
    float num = 1.0f;
    if (w.repeatType == MoveRepeatType::UseOnlyOnce) {
        if (log.Contains(w.state)) num = 0.0f;
    } else if (w.repeatType != MoveRepeatType::CanRepeatForever) {
        float num2 = w.repeatType == MoveRepeatType::CannotRepeat ? 1.0f : static_cast<float>(w.maxTimes);
        num = log.RepeatMultiplier(w.state, num2);
    }
    if (w.cooldown > 0) {
        // StateLog.Where(IsMove).Reverse().Take(cooldown).Any(move.Id == stateId)
        // Registered state IDs are unique, so an age of the same indexed
        // Move is exactly the source's filtered LastN/Id predicate.
        if (log.InMoveCooldown(w.state, w.cooldown)) return 0.0f;
    }
    float weight = w.weightLambda != nullptr ? w.weightLambda(*this) : w.weight;
    return num * weight;
}

// MonsterMoveStateMachine.SetCurrentState: OnExitState of the old state
// (MoveState clears _performedAtLeastOnce), then switch.
void MonsterModel::SetCurrentState(int8_t state) {
    MoveStateMachineState& m = st().machine;
    if (m.current == kStunned) {
        st().next_move.stun_performed = false;
    } else if (m.current >= 0) {
        m.performed_at_least_once &= ~(uint32_t{1} << m.current);
    }
    m.current = state;
}

// MonsterModel.SetMoveImmediate(state, forceTransition) for a state of the machine.
void MonsterModel::SetMoveImmediate(std::string_view stateId, bool forceTransition) {
    if (!NextMove().CanTransitionAway() && !forceTransition) return;
    int8_t s = MachineDef().Find(stateId);
    if (s < 0) throw GameError("SetMoveImmediate: unknown state " + std::string(stateId));
    st().next_move = NextMoveState{};
    st().next_move.state = s;
    SetCurrentState(s);  // MoveStateMachine.ForceCurrentState
}

void MonsterModel::SetMoveImmediateStunned(MoveFn stunMove, std::string_view nextMoveId) {
    if (!NextMove().CanTransitionAway()) return;  // SetMoveImmediate(state) without force
    NextMoveState n;
    n.stunned = true;
    n.stun_move = register_move_fn(stunMove);
    n.stun_follow_up = MachineDef().Find(nextMoveId);
    n.stun_performed = false;
    SetCurrentState(kStunned);  // ForceCurrentState: exits the old state first
    st().next_move = n;
}

std::string_view MonsterModel::LastLoggedMoveId() const {
    const auto& log = st().machine.log;
    if (log.empty()) throw GameError("StateLog is empty");
    return MachineDef().states[static_cast<size_t>(log.last)].id;
}

// MonsterModel.PerformMove
void MonsterModel::PerformMove() {
    sts2::CombatState combatState = CombatState();
    st().is_performing_move = true;
    CreatureList targets = combatState.PlayerCreatures();
    // MoveState.PerformMove: _performedAtLeastOnce = true; await _onPerform(targets)
    NextMoveState move = st().next_move;
    if (move.stunned) {
        st().next_move.stun_performed = true;
        if (MoveFn fn = registered_move_fn(move.stun_move)) fn(*this, targets);
    } else {
        if (move.state < 0) throw GameError("No move has been set for the monster");
        st().machine.performed_at_least_once |= uint32_t{1} << move.state;
        const MonsterStateDef& d = MachineDef().states[static_cast<size_t>(move.state)];
        if (d.onPerform == nullptr) throw GameError("move " + d.id + " has no logic");
        d.onPerform(*this, targets);
    }
    if (st().machine.exists) st().machine.performed_first_move = true;  // MoveStateMachine?.OnMovePerformed
    st().is_performing_move = false;
    sts2::Creature creature = Creature();
    if (creature.IsDead() && Hook::ShouldCreatureBeRemovedFromCombatAfterDeath(combatState, creature)) {
        combatState.RemoveCreature(creature);
    }
}

MonsterModel MonsterModel::ToMutable(Sim* sim, MonsterId id) {
    CreatureRef r = sim->g->creatures.alloc();
    CreatureState& cs = sim->g->creatures[r];
    cs.player = 0;
    cs.monster.id = id;
    return MonsterModel(sim, r);
}

namespace {
int32_t axebot_respawn_hp_bonus(const MonsterModel& m) {
    if (m.MonsterId_() != MonsterId::AXEBOT) return 0;
    const MonsterState& state = m.st();
    if (state.extra[1] == 0) return 0;  // StockAmount has its default value of two.
    return (2 - state.extra[0]) * 10;
}
}  // namespace

int32_t MonsterModel::MinInitialHp() const {
    if (MonsterId_() == MonsterId::TEST_SUBJECT)
        return sim_->g->run.has_ascension(AscensionLevel::ToughEnemies) ? 111 : 100;
    int32_t hp = sim_->g->run.has_ascension(AscensionLevel::ToughEnemies) ? data().min_hp_tough : data().min_hp;
    return hp + axebot_respawn_hp_bonus(*this);
}

int32_t MonsterModel::MaxInitialHp() const {
    if (MonsterId_() == MonsterId::TEST_SUBJECT)
        return sim_->g->run.has_ascension(AscensionLevel::ToughEnemies) ? 111 : 100;
    int32_t hp = sim_->g->run.has_ascension(AscensionLevel::ToughEnemies) ? data().max_hp_tough : data().max_hp;
    return hp + axebot_respawn_hp_bonus(*this);
}

}  // namespace sts2
