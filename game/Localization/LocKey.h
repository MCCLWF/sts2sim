// A localization key (LocString.LocEntryKey; the table is implied by where it
// is used). Only its identity matters to the game logic: state stores a
// deterministic hash, and a process-wide table maps it back to the text key
// for output.
#pragma once

#include <cstdint>
#include <string_view>

namespace sts2 {

struct LocKey {
    uint32_t hash = 0;  // 0 = null
    explicit operator bool() const { return hash != 0; }
    friend bool operator==(LocKey a, LocKey b) { return a.hash == b.hash; }
    std::string_view str() const;  // "" for null or unknown
};

// The key for `text` (FNV-1a), registered for str().
LocKey MakeLocKey(std::string_view text);

}  // namespace sts2
