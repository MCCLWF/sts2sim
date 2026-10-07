#pragma once
#include <functional>
#include "game/Models/ExecutionValue.h"

namespace sts2 {
// The exact original method body is invoked once. Completion values are only
// published after it returned normally; unwind never fabricates out/results.
class MethodValueScope {
protected:
    template<class Body,class Before,class After>
    decltype(auto) ObserveMethod(std::string_view return_type,Body&& body,Before&& before,After&& after) {
        using Result=std::invoke_result_t<Body>;
        if(!value_trace_) return std::invoke(std::forward<Body>(body));
        auto& frame=value_trace_->frames[static_cast<size_t>(value_frame_)];
        frame.parameters_before=std::invoke(std::forward<Before>(before));
        frame.declared_return_type=return_type;frame.method_values_observed=true;
        if constexpr(std::is_void_v<Result>) {
            std::invoke(std::forward<Body>(body));
            FinishMethod(return_type=="Task" ? ExecutionValue{"{\"kind\":\"awaited_completion\"}"}
                : VoidExecutionValue(),std::invoke(std::forward<After>(after)));
        } else {
            // Result is a declared by-value game type; no gameplay getter is
            // introduced here. NRVO retains the original return semantics.
            Result result=std::invoke(std::forward<Body>(body));
            FinishMethod(FreezeExecutionValue(result),std::invoke(std::forward<After>(after)));
            return result;
        }
    }
    void AttachMethodValueFrame(ExecutionTrace* trace,int32_t frame) {value_trace_=trace;value_frame_=frame;}
private:
    void FinishMethod(ExecutionValue value,std::vector<ExecutionArgument> after) {
        auto& frame=value_trace_->frames[static_cast<size_t>(value_frame_)];
        frame.return_value=std::move(value);frame.parameters_after=std::move(after);frame.method_returned=true;
    }
    ExecutionTrace* value_trace_=nullptr;
    int32_t value_frame_=-1;
};
}
