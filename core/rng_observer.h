// Optional, scoped execution diagnostics; no observer is stored in Rng/POD.
#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string_view>
#include <type_traits>

namespace sts2 {
enum class RngValueType : uint8_t { Void, Boolean, Int32, UInt32, UInt64, Float32, Float64 };
struct RngTraceValue {
    RngValueType type = RngValueType::Void;
    uint64_t bits = 0;
    RngTraceValue() = default;
    template<class T> explicit RngTraceValue(T value) {
        if constexpr (std::is_same_v<T,bool>) {type=RngValueType::Boolean;bits=value;}
        else if constexpr (std::is_same_v<T,int32_t>) {type=RngValueType::Int32;bits=static_cast<uint32_t>(value);}
        else if constexpr (std::is_same_v<T,uint32_t>) {type=RngValueType::UInt32;bits=value;}
        else if constexpr (std::is_same_v<T,uint64_t>) {type=RngValueType::UInt64;bits=value;}
        else if constexpr (std::is_same_v<T,float>) {type=RngValueType::Float32;bits=std::bit_cast<uint32_t>(value);}
        else if constexpr (std::is_same_v<T,double>) {type=RngValueType::Float64;bits=std::bit_cast<uint64_t>(value);}
        else static_assert(std::is_same_v<T,void>,"unsupported RNG trace scalar");
    }
    friend bool operator==(const RngTraceValue&,const RngTraceValue&)=default;
};
struct RngTraceState {
    std::array<uint64_t,4> state{};
    int32_t counter=0;
    template<class T> static RngTraceState Capture(const T& rng) {
        return {{rng.random.s0,rng.random.s1,rng.random.s2,rng.random.s3},rng.counter};
    }
    friend bool operator==(const RngTraceState&,const RngTraceState&)=default;
};
struct RngObserver {
    void* context=nullptr;
    uint64_t (*begin)(void*,const void*,std::string_view,std::span<const RngTraceValue>,
        std::span<const float>,const RngTraceState&)=nullptr;
    void (*finish)(void*,uint64_t,RngTraceValue,const RngTraceState&)=nullptr;
    void (*unwind)(void*,uint64_t) noexcept=nullptr;
};
// This is borrowed observation context, never a global random source or
// gameplay state. Nested and concurrent owned predictions restore/isolate it.
inline thread_local const RngObserver* active_rng_observer=nullptr;
class RngObserverScope {
public:
    explicit RngObserverScope(const RngObserver* observer):previous_(active_rng_observer) {active_rng_observer=observer;}
    ~RngObserverScope() {active_rng_observer=previous_;}
    RngObserverScope(const RngObserverScope&)=delete;
    RngObserverScope& operator=(const RngObserverScope&)=delete;
private:
    const RngObserver* previous_;
};
class RngTraceCall {
public:
    template<class T> RngTraceCall(const T& rng,std::string_view operation,
        std::initializer_list<RngTraceValue> parameters={},std::span<const float> weights={})
        :observer_(active_rng_observer) {
        if(observer_) token_=observer_->begin(observer_->context,&rng,operation,
            {parameters.begin(),parameters.size()},weights,RngTraceState::Capture(rng));
    }
    ~RngTraceCall() {if(observer_ && !finished_) observer_->unwind(observer_->context,token_);}
    template<class V,class T> V Finish(V value,const T& rng) {
        FinishValue(RngTraceValue(value),rng);return value;
    }
    template<class T> void FinishVoid(const T& rng) {FinishValue({},rng);}
    RngTraceCall(const RngTraceCall&)=delete;
    RngTraceCall& operator=(const RngTraceCall&)=delete;
private:
    template<class T> void FinishValue(RngTraceValue value,const T& rng) {
        if(observer_) observer_->finish(observer_->context,token_,value,RngTraceState::Capture(rng));
        finished_=true;
    }
    const RngObserver* observer_;
    uint64_t token_=0;
    bool finished_=false;
};
}
