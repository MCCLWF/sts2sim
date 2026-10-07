// Produces full public-ABI snapshots and independently executed continuations.
// Consumers import them in a fresh process, after unrelated registry warmups.
#include <filesystem>
#include <fstream>
#include <iostream>
#include "env/env.h"
#include "engine/json.h"
#include "env/portable_registry.h"
#include "env/c_api.h"
#include "game/All.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Models/Monsters/Chomper.h"
#include "game/Models/Monsters/Nibbit.h"

using namespace sts2;
namespace {
void check(bool b,const char* why) { if(!b) throw GameError(why); }
void raw(JsonWriter& w,const std::string& data) { snapshot_detail::write_snapshot_json(w,snapshot_detail::Parser(data).parse()); }
void save(const std::filesystem::path& directory,const std::string& name,const Env& source,int steps) {
    void* diagnostic=sts2_env_create(); void* imported=nullptr;
    const auto snapshot=source.ExportSnapshotJson();
    check(sts2_env_import_snapshot(diagnostic,snapshot.data(),snapshot.size(),&imported)==0,sts2_env_last_error(diagnostic));
    check(sts2_env_export_portable_snapshot(imported)==0,sts2_env_last_error(imported));
    const std::string portable=sts2_env_last_result(imported);
    check(portable==source.ExportPortableSnapshotJson(),"C ABI portable export differs from C++");
    sts2_env_destroy(imported); sts2_env_destroy(diagnostic);
    JsonWriter w; w.begin_object().field("case",name).key("local");raw(w,snapshot);
    w.key("portable");raw(w,portable);w.key("continuations").begin_array();
    auto cursor=source.Clone();
    for(int i=0;i<steps && cursor->decision().kind!=DecisionKind::Ended;++i) {
        const auto actions=cursor->LegalActions();check(!actions.empty(),"continuation has no legal action");
        Action action=actions.front();
        if(cursor->decision().kind==DecisionKind::Combat) action={.kind=Action::EndTurn};
        else if(cursor->decision().kind==DecisionKind::Choice && cursor->decision().request.prompt=="count") action=actions.back();
        cursor->Step(action);
        w.begin_object().key("action").begin_object().field("kind",int32_t(action.kind))
            .field("index",action.index).field("target",action.target).field("col",int32_t(action.coord.col))
            .field("row",int32_t(action.coord.row)).end_object().key("expected");raw(w,cursor->ExportPortableSnapshotJson());w.end_object();
    }
    w.end_array().end_object();
    std::ofstream file(directory/(name+".json")); file<<w.str(); check(bool(file),"cannot save fixture");
}
MoveFn stun_fn(std::string_view header,std::string_view method={},MoveFn exclude=nullptr) {
    MoveFn found=nullptr;
    for(const auto& r:native_callbacks()) if(r.kind==NativeCallbackKind::Move && r.symbol.starts_with(header)
        && (method.empty() || r.symbol.find(method)!=std::string::npos) && r.move!=exclude) {
        check(found==nullptr,"ambiguous production stun callback"); found=r.move;
    }
    check(found!=nullptr,"production callback missing at library load");return found;
}
void invalid_cases(const std::filesystem::path& directory,const Env& root) {
    using namespace snapshot_detail;
    const Json original=Parser(root.ExportPortableSnapshotJson()).parse();
    auto emit=[&](std::string_view label,auto mutate) {
        Json changed=original;mutate(changed);
        const auto payload=portable_payload(changed.at("logical_state"),changed.at("registry"));
        changed.object.at("wire_hash").text=std::to_string(portable_wire_hash(payload));
        JsonWriter w;write_snapshot_json(w,changed);
        std::filesystem::create_directories(directory/"invalid");
        std::ofstream file(directory/"invalid"/(std::string(label)+".json"));file<<w.str();check(bool(file),"cannot save invalid fixture");
    };
    auto state=[](Json& j)->Json& { return j.object.at("registry").object.at("machines").array.front().object.at("definition").object.at("states").array.front(); };
    emit("unknown-symbol",[&](Json& j){state(j).object.at("move").text="unknown_callback";});
    emit("callback-type",[&](Json& j){
        for(const auto& c:native_callbacks()) if(c.kind==NativeCallbackKind::Condition) {state(j).object.at("move").text=c.symbol;return;}
        throw GameError("condition binding fixture missing");
    });
    emit("callback-owner",[&](Json& j){
        for(const auto& c:native_callbacks()) if(c.kind==NativeCallbackKind::Move && c.owner=="Chomper") {state(j).object.at("move").text=c.symbol;return;}
        throw GameError("foreign move binding fixture missing");
    });
    emit("definition-link",[&](Json& j){state(j).object.at("follow_up").text="127";});
    emit("duplicate-definition",[&](Json& j){auto& a=j.object.at("registry").object.at("machines").array;a.push_back(a.front());});
    emit("unused-localization",[&](Json& j){
        auto& a=j.object.at("registry").object.at("localization").array;
        const auto key=MakeLocKey("PORTABLE_TEST_UNUSED");
        JsonWriter w;w.begin_object().field("hash",uint64_t(key.hash)).field("key","PORTABLE_TEST_UNUSED").end_object();a.push_back(Parser(w.str()).parse());
    });
    emit("rng-order",[&](Json& j){auto& a=j.object.at("logical_state").object.at("run_rng_streams").array;std::swap(a[0],a[1]);});
    emit("inactive-replay-base",[&](Json& j){auto& s=j.object.at("logical_state");s.object.at("replay_base")=s.at("state");});
    emit("logical-state-hash",[&](Json& j){j.object.at("logical_state").object.at("state_hash").text="1";});
}
}
int main(int argc,char** argv) {
    try {
        check(argc==2 || argc==3,"usage: portable_fixture directory [--matrix]");
        std::filesystem::path directory(argv[1]);std::filesystem::create_directories(directory);
        int cases=0;
        auto emit=[&](const std::string& name,const Env& root,int steps=5){save(directory,name,root,steps);++cases;};
        Env root;root.ResetCombat({.seed="PORTABLEFULL",.character=CharacterId::IRONCLAD},EncounterId::CULTISTS_NORMAL);
        emit("combat",root);
        invalid_cases(directory,root);
        Operation op{.kind=Operation::Kind::Action,.source_hash=root.Hash(),.stop_mask=1,.action={.kind=Action::EndTurn}};
        emit("enemy-boundary",*root.PredictOperation(op));
        auto winning=root.Clone();
        auto enemies=CombatState(&winning->sim()).Enemies();
        for(size_t i=1;i<enemies.size();++i) CreatureCmd::Kill(enemies[i]);
        enemies.front().st().current_hp=1;
        bool attack_found=false;
        for(const auto& action:winning->LegalActions()) if(action.kind==Action::PlayCard && action.target>=0) {
            auto hand=Player(&winning->sim(),1).PlayerCombatState().Hand().Cards();
            if(hand[action.index].Type()==CardType::Attack) {
                op={.kind=Operation::Kind::Action,.source_hash=winning->Hash(),.stop_mask=2,.action=action};
                auto boundary=winning->PredictOperation(op);
                check(boundary->decision().kind==DecisionKind::Boundary,"safe outcome fixture missed boundary");
                emit("safe-outcome-boundary",*boundary,5); attack_found=true;break;
            }
        }
        check(attack_found,"safe outcome fixture has no attack");
        Env neow;neow.Reset({.seed="PORTABLEFULL",.character=CharacterId::DEFECT}); emit("event-text",neow,3);
        op={.kind=Operation::Kind::ObtainRelic,.source_hash=neow.Hash(),.relic_id=uint16_t(RelicId::ASTROLABE)};
        emit("typed-transform-choice",*neow.PredictOperation(op),12);
        op.relic_id=uint16_t(RelicId::ORRERY);emit("typed-reward-choice",*neow.PredictOperation(op),12);
        Creature cultist=CombatState(&root.sim()).Enemies().front();CreatureCmd::Stun(cultist);emit("null-stun",root);
        const MonsterId stun_models[]={MonsterId::LAGAVULIN_MATRIARCH,MonsterId::SLUMBERING_BEETLE,
            MonsterId::TUNNELER,MonsterId::CEREMONIAL_BEAST,MonsterId::CORPSE_SLUG,MonsterId::THIEVING_HOPPER,MonsterId::BOWLBUG_ROCK};
        for(auto id:stun_models) {
            Env e;e.ResetCombat({.seed="PORTABLESTUN",.character=CharacterId::IRONCLAD},EncounterId::CULTISTS_NORMAL);
            auto enemy=CreatureCmd::Add(MonsterModel::ToMutable(&e.sim(),id),CombatState(&e.sim()),CombatSide::Enemy);
            switch(id) {
            case MonsterId::LAGAVULIN_MATRIARCH: CreatureCmd::Stun(enemy,stun_fn("AsleepPower.h:"),"SLASH_MOVE");break;
            case MonsterId::SLUMBERING_BEETLE: CreatureCmd::Stun(enemy,stun_fn("SlumberPower.h:"),"ROLL_OUT_MOVE");break;
            case MonsterId::TUNNELER: CreatureCmd::Stun(enemy,stun_fn("BurrowedPower.h:"),"BITE_MOVE");break;
            case MonsterId::CEREMONIAL_BEAST: CreatureCmd::Stun(enemy,stun_fn("PlowPower.h:"),"BEAST_CRY_MOVE");break;
            case MonsterId::THIEVING_HOPPER: CreatureCmd::Stun(enemy,stun_fn("FlutterPower.h:"));break;
            case MonsterId::BOWLBUG_ROCK: {
                const auto& machine=enemy.Monster().MachineDef();
                CreatureCmd::Stun(enemy,stun_fn("BowlbugRock.h:",":Move:DizzyMove:",machine.states[machine.Find("DIZZY_MOVE")].onPerform));break;
            }
            default: CreatureCmd::Stun(enemy,stun_fn("RavenousPower.h:"));break;
            }
            check(enemy.Monster().NextMove().IsStunned(),"fixture stun was ignored");
            emit("stun-"+std::string(name(id)),e,3);
        }
        // A definition remembers Nibbit's creation-time branch even after its
        // mutable fields change; Chomper's initial state likewise persists.
        for(auto id:{MonsterId::NIBBIT,MonsterId::CHOMPER}) {
            Env e;e.ResetCombat({.seed="PORTABLEIMMUTABLE",.character=CharacterId::SILENT},EncounterId::CULTISTS_NORMAL);
            auto m=MonsterModel::ToMutable(&e.sim(),id);
            if(id==MonsterId::NIBBIT) Nibbit(m).SetIsAlone(true); else Chomper(m).SetScreamFirst(true);
            auto enemy=CreatureCmd::Add(m,CombatState(&e.sim()),CombatSide::Enemy);
            if(id==MonsterId::NIBBIT) Nibbit(enemy.Monster()).SetIsAlone(false);else Chomper(enemy.Monster()).SetScreamFirst(false);
            emit("creation-"+std::string(name(id)),e,3);
        }
        if(argc==3) {
            for(auto character:{CharacterId::IRONCLAD,CharacterId::SILENT,CharacterId::DEFECT,CharacterId::REGENT,CharacterId::NECROBINDER})
                for(int ascension:{0,10}) for(auto encounter:{EncounterId::CULTISTS_NORMAL,EncounterId::SLUMBERING_BEETLE_NORMAL,EncounterId::NIBBITS_NORMAL}) {
                    Env e;e.ResetCombat({.seed="PORTABLECOMBATMATRIX",.character=character,.ascension=ascension},encounter);
                    emit("combat-"+std::string(name(character))+"-"+std::to_string(ascension)+"-"+std::string(name(encounter)),e,5);
                }
            for(const auto& info:all_model_infos()) if(info.kind==ModelKind::Monster && IsUsable(info.kind,info.index)) {
                Env e;e.ResetCombat({.seed="PORTABLEALLMACHINES",.character=CharacterId::DEFECT},EncounterId::CULTISTS_NORMAL);
                // Definition transport inventory only: create and bind the
                // immutable machine without rolling context-sensitive initial
                // branches or claiming this is a naturally spawned encounter.
                auto m=MonsterModel::ToMutable(&e.sim(),static_cast<MonsterId>(info.index));
                CombatState(&e.sim()).CreateCreature(m,CombatSide::Enemy,-1);
                m.SetUpForCombat();
                emit("model-"+std::string(name(static_cast<MonsterId>(info.index))),e,0);
            }
        }
        std::cout<<cases<<" portable fixtures saved\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
