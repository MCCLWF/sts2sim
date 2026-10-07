// Compare scoped C++ observations to the independently captured shipped DLL
// trace, including every nested draw and Gaussian retry, not just final RNG.
#include "core/rng.h"
#include "env/snapshot_json.h"
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <vector>

using namespace sts2;
using snapshot_detail::Json;
namespace {
struct Call {
    std::string operation;
    std::vector<RngTraceValue> parameters;
    std::vector<uint32_t> weight_bits;
    RngTraceState before,after;
    RngTraceValue result;
    int32_t parent=-1;
    uint64_t sequence=0,finished_sequence=0;
    bool finished=false;
};
struct Collector {
    std::vector<Call> calls;
    int32_t parent=-1;
    uint64_t sequence=0;
    static uint64_t Begin(void* raw,const void*,std::string_view operation,std::span<const RngTraceValue> args,
        std::span<const float> weights,const RngTraceState& before) {
        auto& self=*static_cast<Collector*>(raw);Call call;
        call.operation=operation;call.parameters.assign(args.begin(),args.end());call.before=before;
        for(auto weight:weights) call.weight_bits.push_back(std::bit_cast<uint32_t>(weight));
        call.parent=self.parent;call.sequence=self.sequence++;
        self.parent=static_cast<int32_t>(self.calls.size());self.calls.push_back(std::move(call));return self.parent;
    }
    static void Finish(void* raw,uint64_t token,RngTraceValue value,const RngTraceState& after) {
        auto& self=*static_cast<Collector*>(raw);auto& call=self.calls.at(token);
        call.after=after;call.result=value;call.finished=true;call.finished_sequence=self.sequence++;self.parent=call.parent;
    }
    static void Unwind(void* raw,uint64_t token) noexcept {
        auto& self=*static_cast<Collector*>(raw);self.parent=self.calls[static_cast<size_t>(token)].parent;
    }
};
RngTraceState state(const Json& j) {
    RngTraceState s;s.counter=j.at("counter").integer<int32_t>();
    if(j.at("state").array.size()!=4) throw std::runtime_error("invalid native RNG state size");
    for(size_t i=0;i<4;i++) s.state[i]=j.at("state").array[i].integer<uint64_t>();
    return s;
}
RngTraceValue value(const Json& j) {
    constexpr const char* types[]={"void","bool","int32","uint32","uint64","float32","float64"};
    RngTraceValue v;v.bits=j.at("bits").integer<uint64_t>();
    for(size_t i=0;i<std::size(types);i++) if(j.at("type").text==types[i]) {v.type=static_cast<RngValueType>(i);return v;}
    throw std::runtime_error("invalid native RNG value type");
}
template<class T> T parameter(const Json& j,size_t i) {
    auto bits=j.at("parameters").array.at(i).at("bits").integer<uint64_t>();
    if constexpr(std::is_same_v<T,float>) return std::bit_cast<float>(static_cast<uint32_t>(bits));
    else if constexpr(std::is_same_v<T,double>) return std::bit_cast<double>(bits);
    else if constexpr(std::is_same_v<T,int32_t>) return std::bit_cast<int32_t>(static_cast<uint32_t>(bits));
    else return static_cast<T>(bits);
}
std::vector<int> execute(Rng& rng,const Json& call) {
    const auto& op=call.at("operation").text;
    auto i=[&](size_t n){return parameter<int32_t>(call,n);};
    auto u=[&](size_t n){return parameter<uint32_t>(call,n);};
    auto l=[&](size_t n){return parameter<uint64_t>(call,n);};
    auto f=[&](size_t n){return parameter<float>(call,n);};
    auto d=[&](size_t n){return parameter<double>(call,n);};
    if(op=="NextBool") rng.next_bool();
    else if(op=="NextInt(max)") rng.next_int(i(0));
    else if(op=="NextInt(min,max)") rng.next_int(i(0),i(1));
    else if(op=="NextUnsignedInt(max)") rng.next_unsigned_int(u(0));
    else if(op=="NextUnsignedInt(min,max)") rng.next_unsigned_int(u(0),u(1));
    else if(op=="NextUnsignedLong") rng.next_unsigned_long();
    else if(op=="NextUnsignedLong(max)") rng.next_unsigned_long(l(0));
    else if(op=="NextUnsignedLong(min,max)") rng.next_unsigned_long(l(0),l(1));
    else if(op=="NextFloat(max)") rng.next_float(f(0));
    else if(op=="NextFloat(min,max)") rng.next_float(f(0),f(1));
    else if(op=="NextDouble") rng.next_double();
    else if(op=="NextDouble(min,max)") rng.next_double(d(0),d(1));
    else if(op=="NextGaussianFloat") rng.next_gaussian_float(f(0),f(1),f(2),f(3));
    else if(op=="NextGaussianDouble") rng.next_gaussian_double(d(0),d(1),d(2),d(3));
    else if(op=="NextGaussianInt") rng.next_gaussian_int(i(0),i(1),i(2),i(3));
    else if(op=="NextItem") rng.next_item_index(i(0));
    else if(op=="WeightedNextItem") {
        std::vector<float> weights;
        for(const auto& v:call.at("weight_bits").array) weights.push_back(std::bit_cast<float>(v.integer<uint32_t>()));
        rng.weighted_next_index(weights);
    } else if(op=="Shuffle") {
        std::vector<int> items(static_cast<size_t>(i(0)));std::iota(items.begin(),items.end(),0);
        rng.Shuffle(items);return items;
    } else throw std::runtime_error("unknown native operation: "+op);
    return {};
}
void check(const Call& actual,const Json& expected,size_t row,size_t index) {
    auto require=[&](bool ok,const char* field) {
        if(!ok) throw std::runtime_error("RNG divergence row="+std::to_string(row)+" call="+std::to_string(index)+" field="+field);
    };
    require(actual.operation==expected.at("operation").text,"operation");
    require(actual.parent==expected.at("parent_call").integer<int32_t>(),"parent_call");
    require(actual.sequence==expected.at("sequence").integer<uint64_t>(),"sequence");
    require(actual.finished_sequence==expected.at("finished_sequence").integer<uint64_t>(),"finished_sequence");
    require(actual.finished && expected.at("finished").boolean,"finished");
    require(actual.before==state(expected.at("before")),"before");
    require(actual.after==state(expected.at("after")),"after");
    require(actual.result==value(expected.at("result")),"result");
    const auto& params=expected.at("parameters").array;require(actual.parameters.size()==params.size(),"parameter count");
    for(size_t i=0;i<params.size();i++) require(actual.parameters[i]==value(params[i]),"parameter value/type");
    const auto& weights=expected.at("weight_bits").array;require(actual.weight_bits.size()==weights.size(),"weight count");
    for(size_t i=0;i<weights.size();i++) require(actual.weight_bits[i]==weights[i].integer<uint32_t>(),"weight bits");
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("usage: rng_execution_checks <native NDJSON>");
        std::ifstream input(argv[1]);if(!input) throw std::runtime_error("cannot open native RNG capture");
        size_t rows=0,total=0,roots=0,shuffles=0;std::string line;
        while(std::getline(input,line)) {
            const auto native=snapshot_detail::Parser(line).parse();
            if(native.at("format").text!="sts2oracle.rng_execution" || native.at("version").integer<int>()!=1
                || native.at("oracle_test_mode").boolean) throw std::runtime_error("unqualified native RNG input");
            Collector observed;RngObserver observer{&observed,Collector::Begin,Collector::Finish,Collector::Unwind};
            Rng rng(native.at("seed").integer<uint64_t>());
            std::map<size_t,std::vector<int>> permutations;
            {
                RngObserverScope scope(&observer);
                const auto& expected=native.at("calls").array;
                for(size_t i=0;i<expected.size();i++) if(expected[i].at("parent_call").integer<int32_t>()==-1) {
                    auto items=execute(rng,expected[i]);roots++;
                    if(expected[i].at("operation").text=="Shuffle") permutations.emplace(i,std::move(items));
                }
                if(observed.calls.size()!=expected.size()) throw std::runtime_error("native call count differs");
                for(size_t i=0;i<expected.size();i++) check(observed.calls[i],expected[i],rows,i);
            }
            if(active_rng_observer || observed.parent!=-1) throw std::runtime_error("borrowed RNG observer retained");
            for(const auto& entry:native.at("shuffles").array) {
                const auto& items=permutations.at(entry.at("call").integer<size_t>());
                if(items.size()!=entry.at("items").array.size()) throw std::runtime_error("shuffle size differs");
                for(size_t i=0;i<items.size();i++) if(items[i]!=entry.at("items").array[i].integer<int>())
                    throw std::runtime_error("shuffle permutation differs");
                shuffles++;
            }
            total+=observed.calls.size();rows++;
        }
        if(!input.eof() || !rows) throw std::runtime_error("incomplete/empty native RNG capture");
        std::cout<<"PASS native_rng rows="<<rows<<" roots="<<roots<<" calls="<<total<<" shuffles="<<shuffles<<'\n';
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
