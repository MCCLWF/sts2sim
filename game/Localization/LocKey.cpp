// See LocKey.h.
#include "game/Localization/LocKey.h"

#include <mutex>
#include <string>
#include <unordered_map>

#include "engine/sim/errors.h"

namespace sts2 {

namespace {
struct Table {
    std::mutex mu;
    std::unordered_map<uint32_t, std::string> keys;
};
Table& table() {
    static Table t;
    return t;
}
}  // namespace

LocKey MakeLocKey(std::string_view text) {
    uint32_t h = 2166136261u;
    for (char c : text) {
        h ^= static_cast<uint8_t>(c);
        h *= 16777619u;
    }
    if (h == 0) h = 1;
    Table& t = table();
    std::lock_guard<std::mutex> lock(t.mu);
    auto [it, inserted] = t.keys.try_emplace(h, text);
    if (!inserted && it->second != text) throw GameError("LocKey hash collision: " + std::string(text));
    return LocKey{h};
}

std::string_view LocKey::str() const {
    if (hash == 0) return {};
    Table& t = table();
    std::lock_guard<std::mutex> lock(t.mu);
    auto it = t.keys.find(hash);
    return it == t.keys.end() ? std::string_view{} : std::string_view(it->second);
}

}  // namespace sts2
