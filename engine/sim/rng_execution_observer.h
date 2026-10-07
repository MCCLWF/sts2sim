#pragma once
#include "core/rng_observer.h"
#include "engine/sim/execution_trace.h"

namespace sts2 {
class Sim;
class RngExecutionObserver {
public:
    RngExecutionObserver(Sim& sim,ExecutionTrace* trace);
    RngExecutionObserver(const RngExecutionObserver&)=delete;
    RngExecutionObserver& operator=(const RngExecutionObserver&)=delete;
private:
    static uint64_t Begin(void*,const void*,std::string_view,std::span<const RngTraceValue>,
        std::span<const float>,const RngTraceState&);
    static void Finish(void*,uint64_t,RngTraceValue,const RngTraceState&);
    static void Unwind(void*,uint64_t) noexcept;
    Sim& sim_;
    ExecutionTrace* trace_;
    int32_t current_call_=-1;
    RngObserver observer_;
    RngObserverScope scope_;
};
}
