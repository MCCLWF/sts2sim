// Stable identities for immutable native code. These bindings are emitted
// with each compiled callback, including branches not taken by a live monster.
#pragma once
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <source_location>
#include <vector>
#include "core/dec.h"
#include "game/fwd.h"
#include "game/Models/signature_types.h"

namespace sts2 {
using MoveFn = void (*)(MonsterModel self, const CreatureList& targets);
using CondFn = bool (*)(MonsterModel self);
using WeightFn = float (*)(MonsterModel self);
using IntentDamageFn = Dec (*)(MonsterModel self);
using IntentRepeatFn = int32_t (*)(MonsterModel self);
struct EventOptionState;
using EventOptionFn = void (*)(EventModel self, const EventOptionState& option);
using WillKillPlayerFn = bool (*)(Player player, Dec value);
enum class NativeCallbackKind { Move, Condition, Weight, IntentDamage, IntentRepeat, EventOption, WillKillPlayer };
struct NativeCallbackRecord {
    std::string symbol;
    std::string owner;
    NativeCallbackKind kind;
    MoveFn move = nullptr;
    CondFn condition = nullptr;
    WeightFn weight = nullptr;
    IntentDamageFn intent_damage = nullptr;
    IntentRepeatFn intent_repeat = nullptr;
    EventOptionFn event_option = nullptr;
    WillKillPlayerFn will_kill_player = nullptr;
};
void register_native_callback(NativeCallbackRecord record);
NativeCallbackRecord native_callback(std::string_view symbol, NativeCallbackKind kind);
std::vector<NativeCallbackRecord> native_callbacks();
std::string native_callback_symbol(MoveFn fn);
std::string native_callback_symbol(CondFn fn);
std::string native_callback_symbol(WeightFn fn);
std::string native_callback_symbol(IntentDamageFn fn);
std::string native_callback_symbol(IntentRepeatFn fn);
std::string native_callback_symbol(EventOptionFn fn);
std::string native_callback_symbol(WillKillPlayerFn fn);

template<size_t N> struct CallbackString {
    char text[N];
    constexpr CallbackString(const char (&value)[N]) { for (size_t i=0; i<N; ++i) text[i]=value[i]; }
    constexpr std::string_view view() const { return {text, N-1}; }
};
template<NativeCallbackKind Kind> struct CallbackPointer;
template<> struct CallbackPointer<NativeCallbackKind::Move> { using type = MoveFn; };
template<> struct CallbackPointer<NativeCallbackKind::Condition> { using type = CondFn; };
template<> struct CallbackPointer<NativeCallbackKind::Weight> { using type = WeightFn; };
template<> struct CallbackPointer<NativeCallbackKind::IntentDamage> { using type = IntentDamageFn; };
template<> struct CallbackPointer<NativeCallbackKind::IntentRepeat> { using type = IntentRepeatFn; };
template<> struct CallbackPointer<NativeCallbackKind::EventOption> { using type = EventOptionFn; };
template<> struct CallbackPointer<NativeCallbackKind::WillKillPlayer> { using type = WillKillPlayerFn; };

template<NativeCallbackKind Kind, auto Fn, CallbackString Symbol, CallbackString Owner, unsigned Column>
struct NativeCallbackBinding {
    using Pointer = typename CallbackPointer<Kind>::type;
    struct Registration {
        Pointer fn = Fn;
        Registration() {
            NativeCallbackRecord record{.symbol=std::string(Symbol.view())+":"+std::to_string(Column), .owner=std::string(Owner.view()), .kind=Kind};
            if constexpr (Kind == NativeCallbackKind::Move) record.move=fn;
            else if constexpr (Kind == NativeCallbackKind::Condition) record.condition=fn;
            else if constexpr (Kind == NativeCallbackKind::Weight) record.weight=fn;
            else if constexpr (Kind == NativeCallbackKind::IntentDamage) record.intent_damage=fn;
            else if constexpr (Kind == NativeCallbackKind::IntentRepeat) record.intent_repeat=fn;
            else if constexpr (Kind == NativeCallbackKind::EventOption) record.event_option=fn;
            else record.will_kill_player=fn;
            register_native_callback(std::move(record));
        }
    };
    // ODR-used static registration, initialized when the shared object loads.
    // get() also enforces initialization for implementations that defer it.
    inline static const Registration registration{};
    static Pointer get() { return registration.fn; }
};
} // namespace sts2

#define STS2_CALLBACK_STRING_INNER(x) #x
#define STS2_CALLBACK_STRING(x) STS2_CALLBACK_STRING_INNER(x)
#define STS2_NATIVE_CALLBACK(Kind, Cls, Fn, Description) \
    ::sts2::NativeCallbackBinding<::sts2::NativeCallbackKind::Kind, Fn, \
        ::sts2::CallbackString{__FILE_NAME__ ":" STS2_CALLBACK_STRING(__LINE__) ":" #Kind ":" Description}, \
        ::sts2::CallbackString{#Cls}, std::source_location::current().column()>::get()

#define SIM_INTENT_DAMAGE(Cls, Expr) \
    STS2_NATIVE_CALLBACK(IntentDamage, Cls, ([](::sts2::MonsterModel m) -> ::sts2::Dec { \
        Cls self(m); return ::sts2::Dec(Expr); }), #Expr)
#define SIM_INTENT_REPEAT(Cls, Expr) \
    STS2_NATIVE_CALLBACK(IntentRepeat, Cls, ([](::sts2::MonsterModel m) -> int32_t { \
        Cls self(m); return (Expr); }), #Expr)
