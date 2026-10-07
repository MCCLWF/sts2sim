#include "engine/sim/rng_execution_observer.h"
#include "engine/sim/sim.h"
#include "content/generated/tables.h"
#include <bit>

namespace sts2 {
namespace {
void identify(ExecutionRngCall& call,const void* rng,const GameState& g) {
    for(size_t i=0;i<g.run.rng.rngs.size();++i) if(rng==&g.run.rng.rngs[i]) {
        call.scope="run";call.stream=kRunRngNames[i];return;
    }
    for(size_t i=0;i<g.run.player.rng.rngs.size();++i) if(rng==&g.run.player.rng.rngs[i]) {
        call.scope="player";call.stream=kPlayerRngNames[i];call.owner_ref=g.run.player.creature;return;
    }
    for(size_t i=0;i<std::size(g.run.events);++i) if(rng==&g.run.events[i].rng) {
        call.scope="event";call.stream="event:"+std::to_string(i);
        call.owner_ref=static_cast<uint16_t>(i+1);
        if(g.run.events[i].id!=EventId::NONE) call.owner_model_id=data(g.run.events[i].id).entry;
        return;
    }
    if(rng==&g.combat.encounter_rng) {
        call.scope="encounter";call.stream="encounter";
        if(g.combat.encounter!=EncounterId::NONE) call.owner_model_id=data(g.combat.encounter).entry;
        return;
    }
    for(uint32_t ref=1;ref<=g.creatures.capacity();++ref) if(g.creatures.live(ref)
        && rng==&g.creatures[ref].monster.rng) {
        const auto& creature=g.creatures[ref];call.scope="monster";call.stream="monster";
        call.owner_ref=ref;call.owner_combat_id=creature.combat_id;
        if(creature.monster.id!=MonsterId::NONE) call.owner_model_id=data(creature.monster.id).entry;
        return;
    }
    // A local/copied RNG still has every value/state observed. No pointer is
    // exported and no persistent object lifetime is guessed from its address.
    call.scope="unclassified";
}
}
RngExecutionObserver::RngExecutionObserver(Sim& sim,ExecutionTrace* trace)
    :sim_(sim),trace_(trace),observer_{this,Begin,Finish,Unwind},
     scope_(trace && trace->collect_rng ? &observer_:nullptr) {}
uint64_t RngExecutionObserver::Begin(void* raw,const void* rng,std::string_view operation,
    std::span<const RngTraceValue> parameters,std::span<const float> weights,const RngTraceState& before) {
    auto& self=*static_cast<RngExecutionObserver*>(raw);auto& trace=*self.trace_;
    ExecutionRngCall call;call.operation=operation;call.before=before;call.parent_call=self.current_call_;
    call.parameters.assign(parameters.begin(),parameters.end());
    for(float weight:weights) call.weight_bits.push_back(std::bit_cast<uint32_t>(weight));
    identify(call,rng,*self.sim_.g);
    // A call needs both a causal frame and one call record. Reject before
    // changing the active parent or consuming RNG if only one slot remains.
    trace.CheckLimit(2);
    const auto previous_sequence=trace.next_sequence;
    ExecutionFrame frame;frame.operation=std::string("rng.")+std::string(operation);frame.kind="rng_call";
    frame.source_kind=self.sim_.damage_source_kind;frame.source_id=self.sim_.damage_source_id;
    call.frame=trace.Enter(std::move(frame));
    call.sequence=trace.next_sequence++;
    const auto index=static_cast<int32_t>(trace.rng_calls.size());
    const auto previous_frame=trace.frames[static_cast<size_t>(call.frame)].parent;
    try { trace.rng_calls.push_back(std::move(call)); }
    catch(...) {
        trace.frames.pop_back();trace.next_sequence=previous_sequence;
        trace.current_frame=previous_frame;throw;
    }
    self.current_call_=index;return static_cast<uint64_t>(index);
}
void RngExecutionObserver::Finish(void* raw,uint64_t token,RngTraceValue result,const RngTraceState& after) {
    auto& self=*static_cast<RngExecutionObserver*>(raw);auto& call=self.trace_->rng_calls.at(token);
    call.result=result;call.after=after;call.finished=true;call.finished_sequence=self.trace_->next_sequence++;
    Unwind(raw,token);
}
void RngExecutionObserver::Unwind(void* raw,uint64_t token) noexcept {
    auto& self=*static_cast<RngExecutionObserver*>(raw);const auto& call=self.trace_->rng_calls[static_cast<size_t>(token)];
    self.trace_->Leave(call.frame,!call.finished);
    self.current_call_=call.parent_call;
}
}
