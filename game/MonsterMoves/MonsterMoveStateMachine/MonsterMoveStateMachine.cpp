// MonsterMoves/MonsterMoveStateMachine/*.cs: definition building and interning.
// The run-time half (RollMove, FindNextMoveState, ...) is in MonsterModel.cpp.
#include "game/MonsterMoves/MonsterMoveStateMachine/MonsterMoveStateMachine.h"

#include <deque>
#include <mutex>
#include <algorithm>

#include "engine/sim/errors.h"
#include "engine/sim/state.h"
#include "game/All.h"

namespace sts2 {

Intent SingleAttackIntent(int32_t damage) { return Intent{.type = IntentType::Attack, .damage = damage}; }
Intent SingleAttackIntent(IntentDamageFn damageCalc) { return Intent{.type = IntentType::Attack, .damageCalc = damageCalc}; }
Intent MultiAttackIntent(int32_t damage, int32_t repeat) {
    return Intent{.type = IntentType::Attack, .damage = damage, .repeat = repeat, .multi = true};
}
Intent MultiAttackIntent(int32_t damage, IntentRepeatFn repeatCalc) {
    return Intent{.type = IntentType::Attack, .damage = damage, .multi = true, .repeatCalc = repeatCalc};
}
Intent DeathBlowIntent(IntentDamageFn damageCalc) { return Intent{.type = IntentType::DeathBlow, .damageCalc = damageCalc}; }
Intent BuffIntent() { return Intent{.type = IntentType::Buff}; }
Intent DebuffIntent(bool strong) { return Intent{.type = strong ? IntentType::DebuffStrong : IntentType::Debuff, .strong = strong}; }
Intent DefendIntent() { return Intent{.type = IntentType::Defend}; }
Intent StunIntent() { return Intent{.type = IntentType::Stun}; }
Intent SleepIntent() { return Intent{.type = IntentType::Sleep}; }
Intent EscapeIntent() { return Intent{.type = IntentType::Escape}; }
Intent HealIntent() { return Intent{.type = IntentType::Heal}; }
Intent HiddenIntent() { return Intent{.type = IntentType::Hidden}; }
Intent SummonIntent() { return Intent{.type = IntentType::Summon}; }
Intent CardDebuffIntent() { return Intent{.type = IntentType::CardDebuff}; }
Intent StatusIntent(int32_t count) { return Intent{.type = IntentType::StatusCard, .count = count}; }
Intent UnknownIntent() { return Intent{.type = IntentType::Unknown}; }

int8_t MoveMachineDef::Find(std::string_view id) const {
    for (size_t i = 0; i < states.size(); ++i) {
        if (states[i].id == id) return static_cast<int8_t>(i);
    }
    return -1;
}

// --------------------------------------------------------------- interning

namespace {
struct Registry {
    std::mutex mu;
    std::deque<MoveMachineDef> defs;  // stable addresses; index 0 unused
    std::vector<std::vector<MonsterId>> owners;
    Registry() { defs.emplace_back(); owners.emplace_back(); }
};
Registry& registry() {
    static Registry r;
    return r;
}
}  // namespace

uint16_t intern_move_machine(MoveMachineDef def, MonsterId owner) {
    Registry& r = registry();
    std::lock_guard<std::mutex> lock(r.mu);
    for (size_t i = 1; i < r.defs.size(); ++i) {
        if (r.defs[i] == def) {
            if (std::find(r.owners[i].begin(), r.owners[i].end(), owner) == r.owners[i].end()) r.owners[i].push_back(owner);
            return static_cast<uint16_t>(i);
        }
    }
    if (r.defs.size() >= 0xFFFF) throw CapacityError("too many move machine definitions");
    r.owners.push_back({owner});
    try { r.defs.push_back(std::move(def)); }
    catch (...) { r.owners.pop_back(); throw; }  // preserve registry alignment on allocation failure
    return static_cast<uint16_t>(r.defs.size() - 1);
}

bool move_machine_matches_model(uint16_t index, MonsterId owner) {
    Registry& r = registry();
    std::lock_guard<std::mutex> lock(r.mu);
    return index != 0 && index < r.owners.size()
        && std::find(r.owners[index].begin(), r.owners[index].end(), owner) != r.owners[index].end();
}

namespace {
struct MoveFnRegistry {
    std::mutex mu;
    std::vector<MoveFn> fns{nullptr};
};
MoveFnRegistry& move_fns() {
    static MoveFnRegistry r;
    return r;
}
}  // namespace

uint16_t register_move_fn(MoveFn fn) {
    if (fn == nullptr) return 0;
    MoveFnRegistry& r = move_fns();
    std::lock_guard<std::mutex> lock(r.mu);
    for (size_t i = 1; i < r.fns.size(); ++i) {
        if (r.fns[i] == fn) return static_cast<uint16_t>(i);
    }
    if (r.fns.size() >= 0xFFFF) throw CapacityError("too many registered stun callbacks");
    r.fns.push_back(fn);
    return static_cast<uint16_t>(r.fns.size() - 1);
}

MoveFn registered_move_fn(uint16_t id) {
    MoveFnRegistry& r = move_fns();
    std::lock_guard<std::mutex> lock(r.mu);
    return id < r.fns.size() ? r.fns[id] : nullptr;
}

const MoveMachineDef& move_machine_def(uint16_t index) {
    Registry& r = registry();
    std::lock_guard<std::mutex> lock(r.mu);
    if (index == 0 || index >= r.defs.size()) throw GameError("unknown move machine definition");
    return r.defs[index];
}

// ------------------------------------------------------------------ builder

std::string_view MoveState::Id() const { return b_->def(i_).id; }

void MoveState::SetFollowUpState(MoveState next) const {
    MonsterStateDef& d = b_->def(i_);
    d.followUp = next.index();  // builder index; remapped in Finish
    d.followUpId.clear();
}

void MoveState::SetFollowUpState(const RandomBranchState& next) const {
    MonsterStateDef& d = b_->def(i_);
    d.followUp = next.index();
    d.followUpId.clear();
}

void MoveState::SetFollowUpState(const ConditionalBranchState& next) const {
    MonsterStateDef& d = b_->def(i_);
    d.followUp = next.index();
    d.followUpId.clear();
}

void MoveState::SetFollowUpStateId(std::string_view id) const {
    MonsterStateDef& d = b_->def(i_);
    d.followUp = -1;
    d.followUpId = std::string(id);
}

void MoveState::SetMustPerformOnceBeforeTransitioning(bool v) const { b_->def(i_).mustPerformOnceBeforeTransitioning = v; }

void RandomBranchState::Add(StateWeight w) const { b_->def(i_).weights.push_back(w); }

// RandomBranchState.cs overloads, argument for argument.
void RandomBranchState::AddBranch(MoveState state, int32_t cooldown, MoveRepeatType repeatType, WeightFn weight) const {
    if (repeatType == MoveRepeatType::CanRepeatXTimes) {
        throw GameError("Use other constructor to specify number of repeats");
    }
    Add(StateWeight{.state = state.index(), .repeatType = repeatType, .weightLambda = weight, .cooldown = cooldown});
}

void RandomBranchState::AddBranch(MoveState state, int32_t cooldown, int32_t maxRepeats, WeightFn weight) const {
    Add(StateWeight{.state = state.index(), .repeatType = MoveRepeatType::CanRepeatXTimes, .maxTimes = maxRepeats,
                    .weightLambda = weight, .cooldown = cooldown});
}

void RandomBranchState::AddBranch(MoveState state, int32_t maxRepeats, WeightFn weight) const {
    AddBranch(state, 0, maxRepeats, weight);
}

void RandomBranchState::AddBranch(MoveState state, int32_t cooldown, MoveRepeatType repeatType, float weight) const {
    // AddBranch(state, cooldown, repeatType, () => weight)
    if (repeatType == MoveRepeatType::CanRepeatXTimes) {
        throw GameError("Use other constructor to specify number of repeats");
    }
    Add(StateWeight{.state = state.index(), .repeatType = repeatType, .weight = weight, .cooldown = cooldown});
}

void RandomBranchState::AddBranch(MoveState state, MoveRepeatType repeatType, float weight) const {
    AddBranch(state, 0, repeatType, weight);
}

void RandomBranchState::AddBranch(MoveState state, MoveRepeatType repeatType, WeightFn weight) const {
    AddBranch(state, 0, repeatType, weight);
}

void RandomBranchState::AddBranch(MoveState state, int32_t maxRepeats, float weight) const {
    // AddBranch(state, maxRepeats, () => weight) -> AddBranch(state, 0, maxRepeats, weight)
    Add(StateWeight{.state = state.index(), .repeatType = MoveRepeatType::CanRepeatXTimes, .maxTimes = maxRepeats,
                    .weight = weight, .cooldown = 0});
}

void RandomBranchState::AddBranch(MoveState state, int32_t cooldown, MoveRepeatType repeatType) const {
    AddBranch(state, cooldown, repeatType, 1.0f);
}

void RandomBranchState::AddBranch(MoveState state, int32_t maxRepeats) const { AddBranch(state, maxRepeats, 1.0f); }

void RandomBranchState::AddBranch(MoveState state, MoveRepeatType repeatType) const {
    AddBranch(state, repeatType, 1.0f);
}

void ConditionalBranchState::AddState(MoveState move, CondFn condition) const {
    b_->def(i_).branches.push_back(ConditionalBranch{move.index(), condition});
}

void ConditionalBranchState::AddState(RandomBranchState branch, CondFn condition) const {
    b_->def(i_).branches.push_back(ConditionalBranch{branch.index(), condition});
}

RandomBranchState MoveMachineBuilder::RandomBranch(std::string_view id) {
    MonsterStateDef d;
    d.kind = MonsterStateKind::RandomBranch;
    d.id = std::string(id);
    return RandomBranchState(this, Create(std::move(d)));
}

ConditionalBranchState MoveMachineBuilder::ConditionalBranch(std::string_view id) {
    MonsterStateDef d;
    d.kind = MonsterStateKind::ConditionalBranch;
    d.id = std::string(id);
    return ConditionalBranchState(this, Create(std::move(d)));
}

int8_t MoveMachineBuilder::Create(MonsterStateDef d) {
    if (created_.size() >= static_cast<size_t>(kMaxMonsterStates)) throw CapacityError("too many monster states");
    created_.push_back(std::move(d));
    return static_cast<int8_t>(created_.size() - 1);
}

// new MonsterMoveStateMachine(list, initial): registers the listed states
// (States.Add throws on duplicate ids) and resolves references by id, the way
// the C# machine looks states up at run time.
MoveMachineDef MoveMachineBuilder::Finish(int8_t initial) {
    MoveMachineDef m;
    for (int8_t b : order_) {
        const MonsterStateDef& d = created_[static_cast<size_t>(b)];
        for (const MonsterStateDef& e : m.states) {
            if (e.id == d.id) throw GameError("duplicate monster state " + d.id);
        }
        m.states.push_back(d);
    }
    auto by_builder_index = [&](int8_t b) -> std::string { return created_[static_cast<size_t>(b)].id; };
    for (MonsterStateDef& d : m.states) {
        if (d.followUp >= 0) {
            std::string id = by_builder_index(d.followUp);
            d.followUp = m.Find(id);
            if (d.followUp < 0) d.followUpId = id;  // not registered: fails on lookup, like C#
        } else if (!d.followUpId.empty()) {
            d.followUp = m.Find(d.followUpId);
        }
        if (d.followUp >= 0) d.followUpId.clear();
        for (StateWeight& w : d.weights) w.state = m.Find(by_builder_index(w.state));
        for (auto& c : d.branches) c.state = m.Find(by_builder_index(c.state));
    }
    m.initial = m.Find(created_[static_cast<size_t>(initial)].id);
    if (m.initial < 0) throw GameError("initial monster state is not registered");
    return m;
}

}  // namespace sts2
