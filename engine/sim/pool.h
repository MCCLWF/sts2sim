// Growable object storage. Numeric references remain stable when slots grow;
// cloning copies live owned values rather than borrowing another state's heap.
#pragma once
#include <cassert>
#include <cstdint>
#include <algorithm>
#include <memory>
#include <vector>
#include <limits>
#include <utility>
#include "engine/sim/errors.h"

namespace sts2 {
template<class T, uint16_t N>
class Pool {
    struct Slot { T value{}; bool used = false; };
    static constexpr size_t kSlotsPerBlock = std::max(size_t{1}, std::min(size_t{N}, size_t{8}));
public:
    static constexpr uint32_t max_size() { return INT32_MAX; }
    static constexpr size_t slot_storage_bytes() { return sizeof(Slot); }
    static size_t clone_storage_bytes(uint32_t capacity) {
        if (!capacity) return 0;
        const size_t blocks = (size_t{capacity} + 1 + kSlotsPerBlock - 1) / kSlotsPerBlock;
        return blocks * (kSlotsPerBlock * sizeof(Slot) + sizeof(void*) + 32) + 32;
    }
    Pool() = default;
    Pool(const Pool& source) : extent_(source.extent_), next_(source.next_), count_(source.count_) {
        resize_storage(extent_);
        for (uint32_t i = 1; i < extent_; ++i)
            if (source.slot(i).used) slot(i) = source.slot(i);
    }
    Pool(Pool&& source) noexcept
        : blocks_(std::move(source.blocks_)), extent_(std::exchange(source.extent_, 1)),
          next_(std::exchange(source.next_, 1)), count_(std::exchange(source.count_, 0)) {}
    Pool& operator=(const Pool& source) {
        if (this != &source) { Pool copy(source); *this = std::move(copy); }
        return *this;
    }
    Pool& operator=(Pool&& source) noexcept {
        if (this != &source) {
            blocks_ = std::move(source.blocks_);
            extent_ = std::exchange(source.extent_, 1);
            next_ = std::exchange(source.next_, 1);
            count_ = std::exchange(source.count_, 0);
        }
        return *this;
    }
    // Current addressable storage, not an allocation ceiling.
    uint32_t capacity() const { return extent_ - 1; }
    // Scratch model wrappers may temporarily extend a pool.  They use this
    // after releasing their object to restore the addressable extent that was
    // visible before the wrapper was created.  Only unused trailing slots may
    // be discarded; live references must remain stable.
    bool trim_capacity(uint32_t capacity) {
        if (capacity > this->capacity()) return false;
        for (uint32_t index = capacity + 1; index <= this->capacity(); ++index)
            if (slot(index).used) return false;
        const uint32_t extent = capacity + 1;
        for (uint32_t i = extent; i < extent_; ++i) slot(i) = Slot{};
        resize_storage(extent);
        extent_ = extent;
        if (next_ > capacity + 1) next_ = capacity + 1;
        return true;
    }
    uint32_t size() const { return count_; }
    uint32_t alloc() {
        uint32_t index = next_;
        while (index < extent_ && slot(index).used) ++index;
        if (index > max_size()) throw CapacityError("object reference exceeds original int32 count range");
        if (index == extent_) { resize_storage(extent_ + 1); ++extent_; }
        slot(index).value = T{}; slot(index).used = true;
        next_ = index + 1; ++count_;
        return index;
    }
    void free(uint32_t index) {
        if (!live(index)) throw GameError("freeing an unused pool slot");
        slot(index).used = false; --count_;
        if (index < next_) next_ = index;
    }
    void clear() { blocks_.clear(); extent_ = 1; next_ = 1; count_ = 0; }
    bool live(uint32_t index) const { return index && index < extent_ && slot(index).used; }
    T& operator[](uint32_t index) {
        assert(index < extent_);
        if (blocks_.empty()) blocks_.push_back(std::make_unique<Slot[]>(kSlotsPerBlock));
        return slot(index).value;
    }
    const T& operator[](uint32_t index) const {
        assert(index < extent_);
        if (blocks_.empty()) { static const T empty{}; return empty; }
        return slot(index).value;
    }
    template<class V> void visit_storage(V&& visit) const {
        visit(capacity()); visit(next_); visit(count_);
        for (uint32_t i = 1; i < extent_; ++i) {
            visit(slot(i).used);
            if (slot(i).used) visit(slot(i).value);
        }
    }
    template<class M, class S> void visit_snapshot(M&& metadata, S&& slot) const {
        metadata(next_, count_);
        for (uint32_t i = 1; i < extent_; ++i) if (this->slot(i).used) slot(i, this->slot(i).value);
    }
    template<class F> void load_snapshot(uint32_t capacity, uint32_t next, uint32_t count, F&& fill) {
        if (capacity > max_size() || next < 1 || next > uint64_t{capacity} + 1 || count > capacity)
            throw GameError("snapshot: invalid pool metadata");
        clear(); resize_storage(capacity + 1); extent_ = capacity + 1;
        fill([&](uint32_t index, const T& item) {
            if (!index || index > capacity || slot(index).used) throw GameError("snapshot: invalid pool slot");
            slot(index).value = item; slot(index).used = true; ++count_;
        });
        if (count_ != count) throw GameError("snapshot: pool count mismatch");
        for (uint32_t i = 1; i < next; ++i) if (!slot(i).used) throw GameError("snapshot: pool cursor skips a free slot");
        next_ = next;
    }
private:
    // Small owned blocks amortize clone allocations. Growing the pointer
    // table never moves slot objects. extent_ is the exact logical extent;
    // unused block tails are absent from snapshots/hash/allocation order.
    Slot& slot(uint32_t index) { return blocks_[index / kSlotsPerBlock][index % kSlotsPerBlock]; }
    const Slot& slot(uint32_t index) const { return blocks_[index / kSlotsPerBlock][index % kSlotsPerBlock]; }
    void resize_storage(uint32_t extent) {
        const size_t required = extent > 1 ? (size_t{extent} + kSlotsPerBlock - 1) / kSlotsPerBlock : 0;
        if (required < blocks_.size()) blocks_.resize(required);
        while (blocks_.size() < required) blocks_.push_back(std::make_unique<Slot[]>(kSlotsPerBlock));
    }
    std::vector<std::unique_ptr<Slot[]>> blocks_;
    uint32_t extent_ = 1;
    uint32_t next_ = 1;
    uint32_t count_ = 0;
};
} // namespace sts2
