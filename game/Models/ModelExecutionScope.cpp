#include "game/Models/ModelExecutionScope.h"
#include "game/Models/AfflictionModel.h"
#include "engine/sim/model_info.h"

namespace sts2 {
void ModelExecutionScope::Begin(Sim& sim,AbstractModel model,Member member,std::string_view name) {
    ExecutionFrame frame;frame.kind="model_method";frame.operation=name;
    frame.source_kind=sim.damage_source_kind;frame.source_id=sim.damage_source_id;
    if(model) {
        constexpr const char* kinds[]={"card","relic","power","potion","monster","encounter","event",
            "enchantment","affliction","orb","character","act","card_pool","relic_pool","potion_pool","modifier"};
        const auto kind=static_cast<size_t>(model.Kind());
        frame.source_model_kind=kind<std::size(kinds) ? kinds[kind]:"unknown";
        frame.source_ref=model.Index();frame.source_ref_kind="pool";
        frame.source_is_canonical=model.IsCanonical();
        frame.source_is_canonical_known=model.Kind()<=ModelKind::Orb;
        switch(model.Kind()) {
        case ModelKind::Event:frame.source_ref_kind="event_slot";break;
        case ModelKind::Encounter:frame.source_ref_kind="combat_encounter";break;
        case ModelKind::Enchantment:frame.source_ref_kind="card_attachment";break;
        case ModelKind::Affliction:
            frame.source_ref_kind=AfflictionModel(&model.sim(), model.Index(), true).IsPending()
                ? "pending_definition" : "card_attachment";break;
        default:break;
        }
        if(model.IsCanonical()) {frame.source_ref=0;frame.source_ref_kind="canonical_definition";}
        // DenseId has no instance contract for character/act/pool/modifier.
        // Do not make a formerly safe default getter throw for diagnostics.
        if(model.Kind()<=ModelKind::Orb) {
            if(const auto* info=model_info(model.Kind(),model.DenseId())) {
                frame.source_model_id=info->id;frame.source_is_override=info->overrides.has(member);
            }
        }
    }
    auto* trace=sim.execution_trace;
    const auto index=trace->Enter(std::move(frame));
    trace_=trace;frame_=index;uncaught_=std::uncaught_exceptions();
    AttachMethodValueFrame(trace_,frame_);
}
}
