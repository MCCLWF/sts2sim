// Scoped observations of real virtual dispatch calls. No gameplay state.
#pragma once
#include "game/Models/AbstractModel.h"
#include "game/Models/MethodValueScope.h"
#include <exception>

namespace sts2 {
class ModelExecutionScope : private MethodValueScope {
public:
    ModelExecutionScope(Sim* sim,AbstractModel model,Member member,std::string_view name) {
        if(sim && sim->execution_trace && sim->execution_trace->collect_causal) Begin(*sim,model,member,name);
    }
    ~ModelExecutionScope() {if(trace_) trace_->Leave(frame_,std::uncaught_exceptions()>uncaught_);}
    ModelExecutionScope(const ModelExecutionScope&)=delete;
    ModelExecutionScope& operator=(const ModelExecutionScope&)=delete;
    template<class Body,class Before,class After>
    decltype(auto) Invoke(std::string_view return_type,Body&& body,Before&& before,After&& after) {
        return ObserveMethod(return_type,std::forward<Body>(body),std::forward<Before>(before),std::forward<After>(after));
    }
private:
    void Begin(Sim&,AbstractModel,Member,std::string_view);
    ExecutionTrace* trace_=nullptr;
    int32_t frame_=-1;
    int uncaught_=0;
};
}
