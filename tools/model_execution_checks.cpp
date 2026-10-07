#include "env/env.h"
#include "env/snapshot_json.h"
#include "game/All.h"
#include "game/Hooks/Hook.h"
#include "engine/sim/model_info.h"
#include "engine/sim/alloc.h"
#include "engine/sim/execution_value_json.h"
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include "game/Models/Canonical.h"
#include "game/Models/ExecutionValue.h"

using namespace sts2;
static bool native_bool(const snapshot_detail::Json& value) {
    if(value.kind!=snapshot_detail::Json::Bool)throw std::runtime_error("native model fact is not a boolean");
    return value.boolean;
}
static void same_json(const snapshot_detail::Json& a,const snapshot_detail::Json& n,const std::string& path) {
    if(a.kind!=n.kind)throw std::runtime_error("native method value kind differs: "+path);
    using J=snapshot_detail::Json;
    if((a.kind==J::Bool && a.boolean!=n.boolean) || ((a.kind==J::Number || a.kind==J::String) && a.text!=n.text))
        throw std::runtime_error("native method value differs: "+path+" CPP="+a.text+" native="+n.text);
    if(a.kind==J::Array) {
        if(a.array.size()!=n.array.size())throw std::runtime_error("native method sequence length differs: "+path);
        for(size_t i=0;i<a.array.size();i++)same_json(a.array[i],n.array[i],path+"["+std::to_string(i)+"]");
    }
    if(a.kind==J::Object) {
        if(a.object.size()!=n.object.size())throw std::runtime_error("native method object fields differ: "+path);
        for(const auto& [key,value]:a.object)same_json(value,n.at(key),path+"."+key);
    }
}
static Dec decimal_bits(const snapshot_detail::Json& value) {
    const auto& bits=value.array;
    if(value.kind!=snapshot_detail::Json::Array || bits.size()!=4)throw std::runtime_error("invalid native decimal bits");
    return Dec::from_bits(bits[0].integer<uint32_t>(),bits[1].integer<uint32_t>(),bits[2].integer<uint32_t>(),bits[3].integer<uint32_t>());
}
static void same_bits(Dec actual,const snapshot_detail::Json& expected,const std::string& where) {
    const auto native=decimal_bits(expected);
    if(actual.lo!=native.lo || actual.mid!=native.mid || actual.hi!=native.hi || actual.flags!=native.flags)
        throw std::runtime_error("native decimal result bits differ: "+where);
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("usage: model_execution_checks <native NDJSON>");
        std::ifstream input(argv[1]);if(!input) throw std::runtime_error("cannot open native model capture");
        auto env=std::make_unique<Env>();
        env->ResetCombat(NewRunOptions{.seed="MODELTRACE",.character=CharacterId::IRONCLAD},EncounterId::CULTISTS_NORMAL);
        auto& sim=env->sim();auto& g=*sim.g;
        // Match the DLL's explicitly constructed pre-start CombatState: normal
        // starting inventory, empty combat piles, one Mawler and three powers.
        for(auto& pile:g.player_combat.piles) pile.clear();
        const auto enemy_ref=g.combat.enemies[0];g.combat.enemies.clear();g.combat.enemies.push_back(enemy_ref);
        g.creatures[enemy_ref].monster.id=MonsterId::MAWLER;
        g.powers.clear();for(auto ref:g.combat.allies) g.creatures[ref].powers.clear();
        g.creatures[enemy_ref].powers.clear();
        Creature dealer(&sim,g.run.player.creature),target(&sim,enemy_ref);
        auto apply=[&](PowerId id,Creature owner,int amount) {
            const auto ref=alloc_power(g,id);PowerModel(&sim,ref).ApplyInternal(owner,Dec(amount),true);
        };
        apply(PowerId::STRENGTH_POWER,dealer,3);apply(PowerId::VULNERABLE_POWER,target,2);apply(PowerId::INTANGIBLE_POWER,target,1);
        // Match the native probe's detached, owned direct-call models; they
        // are not added to the player's combat listener inventory.
        CardModel mutable_card(&sim,g.run.player.deck[0]);
        CardModel power_card(&sim,alloc_card(g,CardId::INFLAME,1));
        RelicModel mutable_relic(&sim,g.run.player.relics[0]);
        RelicModel spiked(&sim,alloc_relic(g,RelicId::SPIKED_GAUNTLETS,1));
        CardModel other_card(&sim,alloc_card(g,CardId::STRIKE_IRONCLAD,0));
        RelicModel other_relic(&sim,alloc_relic(g,RelicId::BURNING_BLOOD,0));
        EventModel mutable_event=EventModel::ToMutable(&sim,EventId::SUNKEN_STATUE);
        size_t rows=0,calls=0,frames=0,parameters=0,outs=0,damage_rows=0,cost_rows=0,identity_rows=0;std::string line;
        if(!std::getline(input,line))throw std::runtime_error("missing native model plan");
        const auto plan=snapshot_detail::Parser(line).parse();
        if(plan.at("format").text!="sts2oracle.model_execution_plan" || plan.at("version").integer<int>()!=2
            || native_bool(plan.at("oracle_test_mode")) || plan.at("game_version").text!="v0.111.0"
            || plan.at("game_dll_sha256").text!="2b40d2df538db1ceb5fa48d958c80ab730ada1e07db88a870aff01a661768b9f"
            || plan.at("model_ref_scheme").text!="explicit_fixture_correspondence; canonical_definition")
            throw std::runtime_error("unqualified native model plan");
        std::set<std::string> planned,seen;
        for(const auto& id:plan.at("case_ids").array)if(id.kind!=snapshot_detail::Json::String || !planned.insert(id.text).second)
            throw std::runtime_error("invalid/duplicate planned model case");
        if(planned.size()!=206)throw std::runtime_error("incomplete native model input matrix");
        bool closed=false;
        while(std::getline(input,line)) {
            const auto expected=snapshot_detail::Parser(line).parse();
            if(closed)throw std::runtime_error("data after native model footer");
            if(expected.at("format").text=="sts2oracle.model_execution_end") {
                if(expected.at("version").integer<int>()!=2 || !native_bool(expected.at("complete"))
                    || expected.at("rows").integer<size_t>()!=rows || seen!=planned)
                    throw std::runtime_error("incomplete native model footer");
                closed=true;continue;
            }
            if(expected.at("format").text!="sts2oracle.model_execution" || expected.at("version").integer<int>()!=2
                || native_bool(expected.at("oracle_test_mode")) || !native_bool(expected.at("unpatched_baseline_equal")))
                throw std::runtime_error("unqualified model input");
            const auto id=expected.at("case_id").text;
            if(!planned.contains(id) || !seen.insert(id).second)throw std::runtime_error("unplanned/duplicate native case");
            const auto kind=expected.at("case_kind").text;
            if(kind=="identity") {
                CanonicalCard canonical_card(&sim,CardId::STRIKE_IRONCLAD),same_card(&sim,CardId::STRIKE_IRONCLAD),different_card(&sim,CardId::DEFEND_IRONCLAD);
                CanonicalRelic canonical_relic(&sim,RelicId::BURNING_BLOOD),same_relic(&sim,RelicId::BURNING_BLOOD);
                const auto name=expected.at("identity_case").text;AbstractModel left,right;
                if(name=="card/canonical_same") {left=canonical_card.get();right=same_card.get();}
                else if(name=="card/canonical_different") {left=canonical_card.get();right=different_card.get();}
                else if(name=="card/canonical_mutable") {left=canonical_card.get();right=mutable_card;}
                else if(name=="card/mutable_same") {left=mutable_card;right=mutable_card;}
                else if(name=="card/mutable_different") {left=mutable_card;right=other_card;}
                else if(name=="relic/canonical_same") {left=canonical_relic.get();right=same_relic.get();}
                else if(name=="relic/canonical_mutable") {left=canonical_relic.get();right=mutable_relic;}
                else if(name=="relic/mutable_different") {left=mutable_relic;right=other_relic;}
                else if(name=="event/canonical_same") {left=EventModel::Canonical(&sim,EventId::SUNKEN_STATUE);right=EventModel::Canonical(&sim,EventId::SUNKEN_STATUE);}
                else if(name=="event/canonical_different") {left=EventModel::Canonical(&sim,EventId::SUNKEN_STATUE);right=EventModel::Canonical(&sim,EventId::SUNKEN_TREASURY);}
                else if(name=="event/canonical_mutable") {left=EventModel::Canonical(&sim,EventId::SUNKEN_STATUE);right=mutable_event;}
                else if(name=="event/mutable_same") {left=mutable_event;right=mutable_event;}
                else throw std::runtime_error("unqualified native identity case");
                same_json(snapshot_detail::Parser(FreezeExecutionValue(left).json).parse(),expected.at("left"),id+".left");
                same_json(snapshot_detail::Parser(FreezeExecutionValue(right).json).parse(),expected.at("right"),id+".right");
                if((left==right)!=native_bool(expected.at("same")))throw std::runtime_error("native reference equality differs: "+id);
                identity_rows++;rows++;continue;
            }
            std::unique_ptr<CanonicalCard> canonical_card;
            const auto card_mode=expected.at("card_mode").text;CardModel card;
            if(card_mode=="canonical") {canonical_card=std::make_unique<CanonicalCard>(&sim,CardId::STRIKE_IRONCLAD);card=canonical_card->get();}
            else if(card_mode=="mutable")card=mutable_card;
            else if(card_mode=="power")card=power_card;
            else if(card_mode!="null")throw std::runtime_error("unqualified native card mode");
            ExecutionTrace trace;trace.collect_causal=true;ModelList modifiers;Dec result;
            if(kind=="damage") {
                const auto flags=static_cast<ModifyDamageHookType>(expected.at("flags").integer<int>());
                const auto damage=Dec(expected.at("damage").integer<int>());
                ExecutionTraceScope scope(sim,&trace);
                result=Hook::ModifyDamage(RunState(&sim),CombatState(&sim),target,dealer,damage,ValueProp::Move,
                    card,nullptr,flags,CardPreviewMode::None,modifiers);
                same_bits(result,expected.at("result_bits"),id);damage_rows++;
            } else if(kind=="energy_cost_out") {
                std::unique_ptr<CanonicalRelic> canonical_relic;AbstractModel source;
                const auto mode=expected.at("source_mode").text;
                if(mode=="canonical") {canonical_relic=std::make_unique<CanonicalRelic>(&sim,RelicId::BURNING_BLOOD);source=canonical_relic->get();}
                else if(mode=="mutable")source=mutable_relic;
                else if(mode=="spiked")source=spiked;
                else if(mode=="event_canonical")source=EventModel::Canonical(&sim,EventId::SUNKEN_STATUE);
                else if(mode=="event_mutable")source=mutable_event;
                else throw std::runtime_error("unqualified native source mode");
                Dec modified=Dec(99);bool returned;
                {ExecutionTraceScope scope(sim,&trace);returned=source.TryModifyEnergyCostInCombat(card,decimal_bits(expected.at("cost_bits")),modified);}
                if(returned!=native_bool(expected.at("result_bool")))throw std::runtime_error("native bool result differs: "+id);
                same_bits(modified,expected.at("modified_cost_bits"),id);cost_rows++;
            } else throw std::runtime_error("unqualified native case kind");
            std::vector<const ExecutionFrame*> actual;
            for(const auto& f:trace.frames)if(f.kind=="model_method")actual.push_back(&f);
            const auto& native=expected.at("calls").array;
            if(actual.size()!=native.size()) throw std::runtime_error("native model call count differs at row "+std::to_string(rows)
                +": CPP="+std::to_string(actual.size())+" native="+std::to_string(native.size()));
            for(size_t i=0;i<actual.size();i++) if(actual[i]->operation!=native[i].at("operation").text
                || actual[i]->source_model_id!=native[i].at("source_model_id").text
                || !actual[i]->finished || actual[i]->unwound)
                throw std::runtime_error("native model call identity/order differs row="+std::to_string(rows)+" call="+std::to_string(i));
            const auto& native_frames=expected.at("frames").array;
            if(trace.frames.size()!=native_frames.size()) throw std::runtime_error("native causal frame count differs row="+std::to_string(rows));
            for(size_t i=0;i<trace.frames.size();i++) {
                const auto& f=trace.frames[i];const auto& n=native_frames[i];
                if(f.unwound)throw std::runtime_error("native synchronous method unexpectedly unwound");
                JsonWriter encoded;encoded.begin_object().field("parent",f.parent).field("kind",f.kind).field("operation",f.operation)
                    .field("source_model_id",f.source_model_id).field("source_is_override",f.source_is_override)
                    .field("source_is_canonical",f.source_is_canonical).field("source_is_canonical_known",f.source_is_canonical_known)
                    .field("sequence",f.sequence).field("finished_sequence",f.finished_sequence).field("finished",f.finished);
                WriteExecutionMethodValues(encoded,f);encoded.end_object();
                const auto values=snapshot_detail::Parser(encoded.str()).parse();
                same_json(values,n,"row="+std::to_string(rows)+" frame="+std::to_string(i)+" "+f.operation);
                if(f.source_is_canonical && (f.source_ref!=0 || f.source_ref_kind!="canonical_definition"))
                    throw std::runtime_error("canonical source exposed scratch pool identity");
                parameters+=f.parameters_before.size();
                for(const auto& argument:f.parameters_before)outs+=bool(argument.flags&1u);
            }
            if(kind=="damage") {
                const auto& native_modifiers=expected.at("modifiers").array;
                if(modifiers.size()!=native_modifiers.size()) throw std::runtime_error("native modifier count differs");
                for(size_t i=0;i<modifiers.size();i++) {
                    const auto* info=model_info(modifiers[i].Kind(),modifiers[i].DenseId());
                    if(!info || info->id!=native_modifiers[i].text) throw std::runtime_error("native modifier identity/order differs");
                }
            }
            calls+=actual.size();frames+=trace.frames.size();rows++;
        }
        if(!input.eof() || !closed || damage_rows!=96 || cost_rows!=98 || identity_rows!=12)throw std::runtime_error("incomplete model capture");
        std::cout<<"PASS native_model rows="<<rows<<" model_calls="<<calls<<" causal_frames="<<frames
            <<" parameter_pairs="<<parameters<<" out_pairs="<<outs<<" damage_rows="<<damage_rows<<" bool_out_rows="<<cost_rows
            <<" identity_rows="<<identity_rows<<'\n';
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
