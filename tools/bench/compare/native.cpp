#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <thread>
#include <vector>
#include <barrier>
#include "env/env.h"
#include "env/preview.h"
#include "game/Runs/RunFlow.h"
#include "tools/cli/dump.h"
using namespace sts2;
using Clock = std::chrono::steady_clock;
struct Case { std::string id, seed, character, encounter; int asc, act, advance; };
template<class Id, class Names> Id parse(const std::string& value, const Names& names) {
    for(size_t i=0;i<names.size();++i) if(names[i]==value) return static_cast<Id>(i);
    throw std::runtime_error("unknown ID: "+value);
}
std::unique_ptr<Env> setup(const Case& c) {
    auto e=std::make_unique<Env>();
    e->ResetCombat({.seed=c.seed,.character=parse<CharacterId>(c.character,kCharacterIdNames),.ascension=c.asc},
                   parse<EncounterId>(c.encounter,kEncounterIdNames),c.act);
    for(int i=0;i<c.advance;++i) e->Step({.kind=Action::EndTurn});
    if(e->decision().kind!=DecisionKind::Combat) throw std::runtime_error("root is not active combat");
    return e;
}
void emit(JsonWriter& w) { std::cout<<w.str()<<'\n'; std::cout.flush(); }
void state(const Case& c,const char* boundary,Env& e) {
    JsonWriter w;w.begin_object().field("type","state").field("case",c.id).field("boundary",boundary).key("state");
    write_combat_state(w,&e.sim());w.end_object();emit(w);
}
template<class F> void measure(const Case& c,const char* operation,F op,int n,int warm,int samples) {
    uint64_t sink=0;for(int i=0;i<warm;++i)sink+=op();
    std::vector<double> ns;
    for(int s=0;s<samples;++s){auto b=Clock::now();for(int i=0;i<n;++i)sink+=op();
        ns.push_back(std::chrono::duration<double,std::nano>(Clock::now()-b).count()/n);}
    JsonWriter w;w.begin_object().field("type","measurement").field("case",c.id).field("operation",operation)
        .field("iterations",n).field("warmup",warm).field("sink",sink).key("samples_ns").begin_array();
    for(auto v:ns) { w.value(v); }
    w.end_array().end_object();emit(w);
}
void run(const Case& c,int n,int warm,int samples,bool verify) {
    auto root=setup(c);auto hash=root->Hash();const Action end{.kind=Action::EndTurn};
    auto legal=root->LegalActions();
    auto it=std::find_if(legal.begin(),legal.end(),[&](auto a){
        if(a.kind!=Action::PlayCard)return false;
        const auto id=Player(&root->sim(),1).PlayerCombatState().Hand().Cards()[a.index].data().entry;
        return id.starts_with("STRIKE_") || id.starts_with("DEFEND_");
    });
    if(it==legal.end())throw std::runtime_error("no basic card at root");
    const Action card=*it;
    JsonWriter a;a.begin_object().field("type","action").field("case",c.id).field("hand_index",card.index)
        .field("target",card.target).end_object();emit(a);
    state(c,"root",*root);
    auto after=root->Clone();after->Step(end);state(c,"after_endturn",*after);
    auto direct=root->Clone();RunFlow::EndTurn(&direct->sim());
    if(direct->Hash()!=after->Hash())throw std::runtime_error("engine and Env EndTurn differ");
    auto played=root->Clone();played->Step(card);state(c,"after_card",*played);
    if(verify){if(root->Hash()!=hash)throw std::runtime_error("root changed");return;}
    measure(c,"Clone",[&]{auto e=root->Clone();return uint64_t(e->state().player_combat.energy);},n*4,warm,samples);
    measure(c,"Clone_EndTurn",[&]{auto e=root->Clone();e->Step(end);return uint64_t(e->state().player_combat.energy);},n,warm,samples);
    measure(c,"Clone_EngineEndTurn",[&]{auto e=root->Clone();RunFlow::EndTurn(&e->sim());return uint64_t(e->state().player_combat.energy);},n,warm,samples);
    measure(c,"Clone_PlayCard",[&]{auto e=root->Clone();e->Step(card);return uint64_t(e->state().player_combat.energy);},n,warm,samples);
    measure(c,"Predict_EndTurn",[&]{auto e=root->Predict(end);return uint64_t(e->state().player_combat.energy);},n,warm,samples);
    measure(c,"LegalActions",[&]{return root->LegalActions().size();},n*4,warm,samples);
    measure(c,"Observe",[&]{return root->Observe().size();},n*4,warm,samples);
    measure(c,"Hash",[&]{return root->Hash();},n*4,warm,samples);
    if(root->Hash()!=hash)throw std::runtime_error("root changed during measurement");
    JsonWriter w;w.begin_object().field("type","verified").field("case",c.id).field("root_stable",true).end_object();emit(w);
}
int main(int argc,char**argv){try{
    if(argc<3)throw std::runtime_error("usage: env_compare_native matrix.tsv verify|measure|parallel [iterations warmup samples|workers total_ops]");
    std::ifstream f(argv[1]);if(!f)throw std::runtime_error("manifest open failed");std::vector<Case> cases;
    std::string line;while(std::getline(f,line)){std::istringstream s(line);Case c;
        if(s>>c.id>>c.seed>>c.character>>c.asc>>c.encounter>>c.act>>c.advance)cases.push_back(c);}
    if(cases.empty())throw std::runtime_error("empty manifest");
    JsonWriter meta;meta.begin_object().field("type","metadata").field("language","cpp")
      .field("compiler",__VERSION__).field("sizeof_GameState",uint64_t(sizeof(GameState))).field("case_count",int(cases.size())).end_object();emit(meta);
    if(std::string(argv[2])=="parallel"){
        int workers=std::stoi(argv[3]),total=std::stoi(argv[4]);auto root=setup(cases.front());auto hash=root->Hash();
        for(int i=0;i<512;++i){auto e=root->Clone();e->Step({.kind=Action::EndTurn});}
        std::barrier ready(workers+1),start(workers+1);std::vector<std::thread> ts;std::vector<uint64_t> sinks(workers);
        for(int t=0;t<workers;++t)ts.emplace_back([&,t]{auto local=root->Clone();ready.arrive_and_wait();start.arrive_and_wait();
            for(int i=t;i<total;i+=workers){auto e=local->Clone();e->Step({.kind=Action::EndTurn});sinks[t]+=e->state().player_combat.energy;}});
        ready.arrive_and_wait();auto b=Clock::now();start.arrive_and_wait();for(auto& t:ts)t.join();
        double seconds=std::chrono::duration<double>(Clock::now()-b).count();if(root->Hash()!=hash)throw std::runtime_error("parallel source mutated");
        JsonWriter w;w.begin_object().field("type","parallel").field("case",cases.front().id).field("workers",workers)
          .field("operations",total).field("seconds",seconds).field("ops_per_second",total/seconds)
          .field("sink",std::accumulate(sinks.begin(),sinks.end(),uint64_t(0))).end_object();emit(w);return 0;
    }
    bool verify=std::string(argv[2])=="verify";int n=argc>3?std::stoi(argv[3]):64,warm=argc>4?std::stoi(argv[4]):128,samples=argc>5?std::stoi(argv[5]):7;
    int failures=0;for(const auto& c:cases){try{run(c,n,warm,samples,verify);}catch(const std::exception& e){++failures;JsonWriter w;
        w.begin_object().field("type","failure").field("case",c.id).field("message",e.what()).end_object();emit(w);}}
    return failures?2:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
