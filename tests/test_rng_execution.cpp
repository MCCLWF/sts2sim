#include "env/preview.h"
#include "env/snapshot_json.h"
#include "game/All.h"
#include "game/Models/Cards/StrikeIronclad.h"
#include "game/Models/Canonical.h"
#include "game/Models/ExecutionValue.h"
#include <future>
#include <iostream>
#include <set>
#include <stdexcept>

using namespace sts2;
namespace {
void require(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
std::unique_ptr<Env> combat() {
    auto env=std::make_unique<Env>();
    env->ResetCombat(NewRunOptions{.seed="RNGTRACEOWNED",.character=CharacterId::IRONCLAD},EncounterId::CULTISTS_NORMAL);
    // Ten-card starter deck: the next turn after this one must shuffle.
    env->Step(Action{.kind=Action::EndTurn});
    return env;
}
void verify(const ExecutionTrace& trace) {
    std::set<uint64_t> sequence;
    for(size_t i=0;i<trace.frames.size();i++) {
        const auto& f=trace.frames[i];
        require(f.finished && f.sequence<f.finished_sequence,"unfinished/invalid frame order");
        require(sequence.insert(f.sequence).second && sequence.insert(f.finished_sequence).second,"duplicate frame order");
        require(f.parent<static_cast<int32_t>(i) && f.parent>=-1,"invalid causal frame parent");
        if(f.parent>=0) {
            const auto& p=trace.frames[static_cast<size_t>(f.parent)];
            require(p.sequence<f.sequence && f.finished_sequence<p.finished_sequence,"frame outside parent lifetime");
        }
    }
    for(const auto& e:trace.events) require(sequence.insert(e.sequence).second,"duplicate event order");
    for(const auto& e:trace.power_events) require(sequence.insert(e.sequence).second,"duplicate power order");
    for(size_t i=0;i<trace.rng_calls.size();i++) {
        const auto& c=trace.rng_calls[i];
        require(c.finished && c.sequence<c.finished_sequence,"unfinished/invalid RNG order");
        require(sequence.insert(c.sequence).second && sequence.insert(c.finished_sequence).second,"duplicate RNG order");
        require(c.frame>=0 && static_cast<size_t>(c.frame)<trace.frames.size(),"invalid RNG frame");
        require(c.parent_call<static_cast<int32_t>(i),"RNG call parent must precede child");
        if(c.parent_call>=0) {
            const auto& p=trace.rng_calls.at(static_cast<size_t>(c.parent_call));
            require(p.sequence<c.sequence && c.finished_sequence<p.finished_sequence,"child outside parent lifetime");
            require(trace.frames[static_cast<size_t>(c.frame)].parent==p.frame,"wrong RNG causal parent");
        }
    }
    require(trace.current_frame==-1,"trace retained active causal frame");
}
void scopes() {
    auto env=combat();auto& sim=env->sim();ExecutionTrace trace;trace.collect_rng=true;
    {
        ExecutionTraceScope outer(sim,&trace);
        const auto* observer=active_rng_observer;require(observer!=nullptr,"missing privileged observer");
        for(size_t i=0;i<sim.g->run.rng.rngs.size();i++) {
            const auto start=trace.rng_calls.size();sim.g->run.rng.rngs[i].NextBool();
            const auto& c=trace.rng_calls.at(start);
            require(c.scope=="run" && c.stream==kRunRngNames[i],"run stream identity mismatch");
        }
        for(size_t i=0;i<sim.g->run.player.rng.rngs.size();i++) {
            const auto start=trace.rng_calls.size();sim.g->run.player.rng.rngs[i].NextBool();
            const auto& c=trace.rng_calls.at(start);
            require(c.scope=="player" && c.stream==kPlayerRngNames[i] && c.owner_ref==sim.g->run.player.creature,"player stream identity mismatch");
        }
        for(size_t i=0;i<std::size(sim.g->run.events);i++) {
            const auto start=trace.rng_calls.size();sim.g->run.events[i].rng.NextBool();
            const auto& c=trace.rng_calls.at(start);
            require(c.scope=="event" && c.stream=="event:"+std::to_string(i),"event stream identity mismatch");
        }
        sim.g->combat.encounter_rng.NextBool();require(trace.rng_calls.back().scope=="encounter","encounter identity mismatch");
        for(auto enemy:CombatState(&sim).Enemies()) {
            const auto start=trace.rng_calls.size();auto& owner=enemy.st();owner.monster.rng.NextBool();
            const auto& c=trace.rng_calls.at(start);
            require(c.scope=="monster" && c.owner_ref==enemy.Index() && c.owner_combat_id==static_cast<uint32_t>(owner.combat_id)
                && c.owner_model_id==data(owner.monster.id).entry,"monster stream identity mismatch");
        }
        Rng local(123);local.next_float();require(trace.rng_calls.back().scope=="unclassified","local lifetime guessed");
        const auto count=trace.rng_calls.size();
        {
            ExecutionTraceScope hidden(sim,nullptr);require(!active_rng_observer,"untraced scope inherited observer");
            local.NextBool();
        }
        require(trace.rng_calls.size()==count && active_rng_observer==observer,"nested observer not restored");
        {
            ExecutionTrace nested;nested.collect_rng=true;ExecutionTraceScope child(sim,&nested);
            local.NextBool();require(nested.rng_calls.size()==1,"nested observed scope lost call");
        }
        require(active_rng_observer==observer && trace.rng_calls.size()==count,"nested trace leaked into parent");
        local.NextBool();
    }
    require(!active_rng_observer && !sim.execution_trace,"scoped observer retained");verify(trace);
}
void capacity() {
    auto env=combat();auto& sim=env->sim();auto& rng=sim.g->combat.encounter_rng;
    const Rng original=rng;ExecutionTrace trace;trace.collect_rng=true;trace.limit=1;
    {
        ExecutionTraceScope scope(sim,&trace);
        try {rng.NextBool();throw std::runtime_error("half-call capacity accepted");}
        catch(const TraceLimitExceeded&) {}
        require(rng==original && trace.current_frame==-1 && trace.rng_calls.empty() && trace.frames.empty(),"capacity failed after mutation");
        trace.limit=3;
        try {rng.next_unsigned_int();throw std::runtime_error("nested capacity accepted");}
        catch(const TraceLimitExceeded&) {}
        require(trace.current_frame==-1 && rng==original,"nested capacity unwind failed");
    }
    require(!active_rng_observer && !sim.execution_trace,"capacity retained observer");
    auto resumed=PreviewAction(*env,Action{.kind=Action::EndTurn},{.collect_rng=true});
    require(!resumed.trace.rng_calls.empty(),"prediction after capacity lost observer");verify(resumed.trace);
}
void predictions() {
    auto env=combat();const auto hash=env->Hash();
    auto plain=env->Predict(Action{.kind=Action::EndTurn});
    auto observed=PreviewAction(*env,Action{.kind=Action::EndTurn},{.collect_rng=true});
    require(plain->ExportSnapshotJson()==observed.successor->ExportSnapshotJson(),"RNG observer changed successor bytes");
    require(env->Hash()==hash && !active_rng_observer,"RNG observer changed source");
    require(!observed.trace.rng_calls.empty(),"combat produced no RNG records");verify(observed.trace);
    for(const auto& frame:observed.trace.frames)
        require(frame.kind!="model_method" && frame.kind!="hook","default diagnostic leaked hidden model order");
    auto causal=PreviewAction(*env,Action{.kind=Action::EndTurn},{.collect_rng=true,.collect_causal=true});
    require(plain->ExportSnapshotJson()==causal.successor->ExportSnapshotJson(),"causal diagnostics changed full successor state");
    require(env->Hash()==hash,"causal diagnostics changed source");verify(causal.trace);
    bool models=false,hooks=false;
    for(const auto& frame:causal.trace.frames) {
        if(frame.kind=="model_method") {
            models=true;require(!frame.source_model_id.empty() && !frame.source_model_kind.empty(),"model identity not frozen");
        }
        hooks|=frame.kind=="hook";
        if(frame.kind=="model_method" || frame.kind=="hook") {
            require(frame.method_values_observed && frame.method_returned && frame.return_value.has_value(),
                "normal method omitted entry/completion/return facts");
            require(!frame.declared_return_type.empty() && frame.parameters_before.size()==frame.parameters_after.size(),
                "method signature/completion argument mismatch");
            for(size_t i=0;i<frame.parameters_before.size();i++) {
                const auto& before=frame.parameters_before[i];const auto& after=frame.parameters_after[i];
                require(before.name==after.name && before.declared_type==after.declared_type && before.flags==after.flags,
                    "method direction metadata changed during completion");
            }
        } else require(!frame.method_values_observed && !frame.return_value,"ordinary frame fabricated method values");
    }
    require(models && hooks,"causal preview omitted dispatched models/hooks");
    try {PreviewAction(*env,Action{.kind=Action::EndTurn},{.collect_trace=false,.collect_causal=true});throw std::runtime_error("hidden causal accepted");}
    catch(const InvalidRequest&) {}
    try {PreviewAction(*env,Action{.kind=Action::EndTurn},{.collect_trace=false,.collect_rng=true});throw std::runtime_error("hidden RNG accepted");}
    catch(const InvalidRequest&) {}
    auto job=[] {
        require(!active_rng_observer,"thread inherited RNG observer");auto env=combat();
        auto preview=PreviewAction(*env,Action{.kind=Action::EndTurn},{.collect_rng=true});verify(preview.trace);
        require(!active_rng_observer,"thread retained RNG observer");return preview.successor->Hash();
    };
    auto first=std::async(std::launch::async,job);auto second=std::async(std::launch::async,job);
    require(first.get()==second.get() && !active_rng_observer,"concurrent observer contaminated state");
}
void canonical_references() {
    static_assert(std::is_copy_constructible_v<GameState>);
    static_assert(std::is_trivially_copyable_v<AbstractModel>);
    auto env=combat();auto& sim=env->sim();
    const auto original=env->ExportSnapshotJson();
    ExecutionTrace trace;trace.collect_causal=true;AbstractModel retained;
    const auto card_count=sim.g->cards.size(),relic_count=sim.g->relics.size();
    {
        CanonicalCard canonical(&sim,CardId::STRIKE_IRONCLAD),same(&sim,CardId::STRIKE_IRONCLAD);
        CanonicalRelic relic(&sim,RelicId::BURNING_BLOOD);
        retained=canonical.get();
        require(canonical.get().Index()!=same.get().Index(),"canonical identity fixture did not use separate scratch slots");
        require(canonical.get()==same.get() && canonical.get().IsCanonical() && !canonical.get().IsMutable(),
            "ModelDb definition identity depends on scratch storage");
        auto concrete=canonical.get().Rebind<StrikeIronclad>();
        require(concrete.IsCanonical() && concrete.DenseId()==canonical.get().DenseId(),"concrete dispatch lost canonical identity");
        ExecutionTraceScope scope(sim,&trace);
        require(canonical.get().Type()==CardType::Attack,"canonical getter result");
        Dec modified=Dec(99);relic.get().TryModifyEnergyCostInCombat(canonical.get(),Dec(1),modified);
    }
    require(sim.g->cards.size()==card_count && sim.g->relics.size()==relic_count,"canonical scratch escaped wrapper lifetime");
    require(env->ExportSnapshotJson()==original,"canonical observer changed complete source state/RNG");
    const auto frozen=FreezeExecutionValue(retained).json;
    {
        CanonicalCard reused(&sim,CardId::DEFEND_IRONCLAD);
        require(reused.get().Index()==retained.Index(),"canonical reuse fixture did not reuse scratch slot");
        require(reused.get()!=retained && FreezeExecutionValue(retained).json==frozen,
            "retained canonical diagnostic identity rebound to reused scratch model");
    }
    require(trace.frames.size()==2,"canonical method contract did not record exact direct calls");
    for(const auto& frame:trace.frames)require(frame.source_is_canonical_known && frame.source_is_canonical && frame.source_ref==0
        && frame.source_ref_kind=="canonical_definition" && frame.method_returned,"canonical frame exposed scratch instance identity");
    const auto parameter=snapshot_detail::Parser(trace.frames[1].parameters_before[0].value.json).parse();
    require(parameter.at("is_canonical").boolean && parameter.at("ref").integer<int>()==0,"canonical argument exposed scratch identity");
    require(env->ExportSnapshotJson()==original,"canonical scratch reuse changed source state");
    const auto opaque=FreezeExecutionValue(AbstractModel(&sim,ModelKind::Character,1));
    require(!opaque.complete && snapshot_detail::Parser(opaque.json).parse().at("is_canonical").is_null(),
        "unknown model representation fabricated a mutability fact");
    verify(trace);
}
}
int main() {try {scopes();capacity();predictions();canonical_references();std::cout<<"RNG execution contracts passed\n";}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
