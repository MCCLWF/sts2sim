#pragma once
#include "engine/json.h"
#include "engine/sim/execution_trace.h"

namespace sts2 {
// Values contain JSON produced exclusively by the typed diagnostic encoder.
inline void WriteExecutionArguments(JsonWriter& w,const std::vector<ExecutionArgument>& arguments) {
    w.begin_array();
    for(const auto& argument:arguments) {
        w.begin_object().field("name",argument.name).field("declared_type",argument.declared_type)
            .field("is_out",bool(argument.flags&1)).field("is_by_ref",bool(argument.flags&2))
            .field("is_in",bool(argument.flags&4)).field("value_complete",argument.value.complete)
            .key("value").trusted_json(argument.value.json).end_object();
    }
    w.end_array();
}
inline void WriteExecutionMethodValues(JsonWriter& w,const ExecutionFrame& frame) {
    w.field("method_values_observed",frame.method_values_observed).field("method_returned",frame.method_returned)
        .field("declared_return_type",frame.declared_return_type).key("parameters_before");
    WriteExecutionArguments(w,frame.parameters_before);
    w.key("parameters_after");WriteExecutionArguments(w,frame.parameters_after);
    w.field("return_value_complete",frame.return_value && frame.return_value->complete).key("return_value");
    if(frame.return_value) w.trusted_json(frame.return_value->json);else w.null();
}
}
