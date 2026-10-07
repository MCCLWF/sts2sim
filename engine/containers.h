// Owning growable lists and bounded map HashSets. Clone uses value copying.
#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <type_traits>

#include "engine/sim/errors.h"
#include "engine/small_vec.h"

namespace sts2 {

// Compatibility name: N is an inline-storage hint, not a maximum.
template<class T, size_t N>
using StaticVec = SmallVec<T, N>;

// System.Collections.Generic.HashSet<T> enumeration semantics: elements are
// enumerated in slot order, a removed slot goes onto a LIFO free list and is
// reused by the next Add. Several game algorithms iterate HashSets (map node
// parents/children, ...), so this order is observable through the RNG.
template <class T, size_t N>
class DotnetSet {
    static_assert(std::is_trivially_copyable_v<T>);

public:
    constexpr size_t size() const { return count_ - free_count_; }
    constexpr bool empty() const { return size() == 0; }

    constexpr bool contains(const T& v) const { return find(v) >= 0; }

    // HashSet.Add: returns false if already present.
    constexpr bool add(const T& v) {
        if (find(v) >= 0) return false;
        int index;
        if (free_count_ > 0) {
            index = free_list_;
            free_list_ = next_free_[static_cast<size_t>(index)];
            --free_count_;
        } else {
            if (static_cast<size_t>(count_) >= N) throw CapacityError("DotnetSet capacity exceeded");
            index = count_++;
        }
        slots_[static_cast<size_t>(index)] = v;
        used_[static_cast<size_t>(index)] = true;
        return true;
    }

    constexpr bool remove(const T& v) {
        int i = find(v);
        if (i < 0) return false;
        used_[static_cast<size_t>(i)] = false;
        next_free_[static_cast<size_t>(i)] = free_count_ > 0 ? free_list_ : -1;
        free_list_ = i;
        ++free_count_;
        return true;
    }

    constexpr void clear() { *this = DotnetSet{}; }

    // Enumerable.First().
    constexpr T first() const {
        for (int i = 0; i < count_; ++i) {
            if (used_[static_cast<size_t>(i)]) return slots_[static_cast<size_t>(i)];
        }
        assert(false && "First() on empty set");
        return T{};
    }

    template <class F>
    constexpr void for_each(F&& f) const {
        for (int i = 0; i < count_; ++i) {
            if (used_[static_cast<size_t>(i)]) f(slots_[static_cast<size_t>(i)]);
        }
    }

    template <class Pred>
    constexpr bool any(Pred pred) const {
        for (int i = 0; i < count_; ++i) {
            if (used_[static_cast<size_t>(i)] && pred(slots_[static_cast<size_t>(i)])) return true;
        }
        return false;
    }

    // Snapshot in enumeration order (for loops that mutate the set, like the
    // game's `.ToList()` before modifying).
    StaticVec<T, N> to_vec() const {
        StaticVec<T, N> out;
        for_each([&](const T& v) { out.push_back(v); });
        return out;
    }

    template<class V> void visit_storage(V&& visit) const {
        visit(count_);
        visit(free_list_);
        visit(free_count_);
        for (int i = 0; i < count_; ++i) {
            const auto slot = static_cast<size_t>(i);
            visit(used_[slot]);
            if (used_[slot]) visit(slots_[slot]);
            else visit(next_free_[slot]);
        }
    }

    // Removed slots retain their links because subsequent insertion order is observable.
    template<class M, class S> void visit_snapshot(M&& metadata, S&& slot) const {
        metadata(count_, free_list_, free_count_);
        for (int i = 0; i < count_; ++i) {
            const auto index = static_cast<size_t>(i);
            slot(i, used_[index], slots_[index], next_free_[index]);
        }
    }

    template<class F> void load_snapshot(int count, int free_list, int free_count, F&& fill) {
        clear();
        if (count < 0 || static_cast<size_t>(count) > N || free_count < 0 || free_count > count)
            throw GameError("snapshot: invalid set counts");
        count_ = count; free_list_ = free_list; free_count_ = free_count;
        std::array<bool, N> seen{};
        fill([&](int index, bool used, const T& value, int8_t next_free) {
            if (index < 0 || index >= count || seen[index]) throw GameError("snapshot: invalid set slot");
            seen[index] = true; used_[index] = used; slots_[index] = value; next_free_[index] = next_free;
        });
        int unused = 0;
        for (int i = 0; i < count; ++i) {
            if (!seen[i]) throw GameError("snapshot: missing set slot");
            if (!used_[i]) ++unused;
            else for (int j = 0; j < i; ++j)
                if (used_[j] && slots_[j] == slots_[i]) throw GameError("snapshot: duplicate set value");
        }
        if (unused != free_count) throw GameError("snapshot: set free count mismatch");
        seen.fill(false);
        int cursor = free_list;
        for (int n = 0; n < free_count; ++n) {
            if (cursor < 0 || cursor >= count || used_[cursor] || seen[cursor])
                throw GameError("snapshot: invalid set free chain");
            seen[cursor] = true; cursor = next_free_[cursor];
        }
        if (cursor != -1) throw GameError("snapshot: unterminated set free chain");
    }

private:
    constexpr int find(const T& v) const {
        for (int i = 0; i < count_; ++i) {
            if (used_[static_cast<size_t>(i)] && slots_[static_cast<size_t>(i)] == v) return i;
        }
        return -1;
    }

    std::array<T, N> slots_{};
    std::array<int8_t, N> next_free_{};
    std::array<bool, N> used_{};
    int count_ = 0;
    int free_list_ = -1;
    int free_count_ = 0;
};

}  // namespace sts2
