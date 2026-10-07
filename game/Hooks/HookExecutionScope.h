#pragma once
#include "engine/sim/sim.h"
#include "game/Models/MethodValueScope.h"
#include <concepts>
#include <exception>

namespace sts2 {
// Resolve the existing execution context from actual Hook arguments. This
// neither evaluates game getters nor installs a global gameplay context.
class HookExecutionScope : private MethodValueScope {
public:
    template<class... Args> HookExecutionScope(std::string_view name,const Args&... args) {
        auto* sim=Pick(args...);
        if(!sim || !sim->execution_trace || !sim->execution_trace->collect_causal) return;
        ExecutionFrame frame;frame.kind="hook";frame.operation=name;
        frame.source_kind=sim->damage_source_kind;frame.source_id=sim->damage_source_id;
        auto* trace=sim->execution_trace;const auto index=trace->Enter(std::move(frame));
        trace_=trace;frame_=index;uncaught_=std::uncaught_exceptions();
        AttachMethodValueFrame(trace_,frame_);
    }
    ~HookExecutionScope() {if(trace_) trace_->Leave(frame_,std::uncaught_exceptions()>uncaught_);}
    HookExecutionScope(const HookExecutionScope&)=delete;
    HookExecutionScope& operator=(const HookExecutionScope&)=delete;
    template<class Body,class Before,class After>
    decltype(auto) Invoke(std::string_view return_type,Body&& body,Before&& before,After&& after) {
        return ObserveMethod(return_type,std::forward<Body>(body),std::forward<Before>(before),std::forward<After>(after));
    }
private:
    static Sim* Pick() {return nullptr;}
    template<class T,class... Args> static Sim* Pick(const T& value,const Args&... rest) {
        if constexpr(requires {{value.sim()} -> std::same_as<Sim&>;}) {
            if constexpr(requires {static_cast<bool>(value);}) {
                if(value) return &value.sim();
            } else return &value.sim();
        }
        return Pick(rest...);
    }
    ExecutionTrace* trace_=nullptr;
    int32_t frame_=-1;
    int uncaught_=0;
};
}
