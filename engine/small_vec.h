// Owning List<T> storage: a small inline prefix, then a contiguous allocation.
// N is an inline-size hint, never a game rule or maximum element count.
#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <span>
#include <type_traits>
#include <utility>

#include "engine/sim/errors.h"

namespace sts2 {

template<class T, size_t N>
class SmallVec {
    static constexpr size_t kInline = std::max(size_t{1}, std::min(N, size_t{16}));
public:
    using value_type = T;
    static constexpr size_t max_size() { return static_cast<size_t>(INT32_MAX); }

    SmallVec() = default;
    SmallVec(std::initializer_list<T> values) { for (const auto& value : values) push_back(value); }
    SmallVec(const SmallVec& source) {
        reserve(source.size());
        for (const auto& value : source) data()[size_++] = value;
    }
    SmallVec(SmallVec&& source) noexcept(std::is_nothrow_move_assignable_v<T>) {
        move_from(std::move(source));
    }
    SmallVec& operator=(const SmallVec& source) {
        if (this != &source) { SmallVec copy(source); *this = std::move(copy); }
        return *this;
    }
    SmallVec& operator=(SmallVec&& source) noexcept(std::is_nothrow_move_assignable_v<T>) {
        if (this != &source) { clear(); overflow_.reset(); capacity_ = kInline; move_from(std::move(source)); }
        return *this;
    }

    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }
    bool empty() const { return size_ == 0; }
    bool full() const { return size_ == capacity_; }
    T* data() { return overflow_ ? overflow_.get() : inline_.data(); }
    const T* data() const { return overflow_ ? overflow_.get() : inline_.data(); }
    T* begin() { return data(); }
    T* end() { return data() + size_; }
    const T* begin() const { return data(); }
    const T* end() const { return data() + size_; }
    T& operator[](size_t index) { assert(index < size_); return data()[index]; }
    const T& operator[](size_t index) const { assert(index < size_); return data()[index]; }
    T& front() { return (*this)[0]; }
    const T& front() const { return (*this)[0]; }
    T& back() { return (*this)[size_ - 1]; }
    const T& back() const { return (*this)[size_ - 1]; }
    std::span<T> span() { return {data(), size_}; }
    std::span<const T> span() const { return {data(), size_}; }
    operator std::span<T>() { return span(); }
    operator std::span<const T>() const { return span(); }

    void reserve(size_t requested) {
        if (requested <= capacity_) return;
        if (requested > max_size()) throw CapacityError("List count exceeds original int32 range");
        const size_t doubled = capacity_ > max_size() / 2 ? max_size() : std::max(size_t{1}, capacity_ * 2);
        const size_t grown = std::max(requested, doubled);
        auto allocation = std::make_unique<T[]>(grown);
        for (size_t i = 0; i < size_; ++i) allocation[i] = std::move_if_noexcept(data()[i]);
        // Moved inline owners must not retain hidden heap allocations.
        if (!overflow_) for (size_t i = 0; i < size_; ++i) inline_[i] = T{};
        overflow_ = std::move(allocation);
        capacity_ = grown;
    }
    void push_back(const T& value) {
        // A caller may append a value from this list while growing it.
        T copy(value);
        reserve(size_ + 1);
        data()[size_++] = std::move(copy);
    }
    void pop_back() { assert(size_); data()[--size_] = T{}; }
    void clear() { while (size_) pop_back(); }
    void resize(size_t count) {
        reserve(count);
        while (size_ > count) pop_back();
        while (size_ < count) data()[size_++] = T{};
    }
    void insert(size_t index, const T& value) {
        assert(index <= size_);
        T copy(value);
        reserve(size_ + 1);
        for (size_t i = size_; i > index; --i) data()[i] = std::move(data()[i - 1]);
        data()[index] = std::move(copy);
        ++size_;
    }
    void erase_at(size_t index) {
        assert(index < size_);
        for (size_t i = index; i + 1 < size_; ++i) data()[i] = std::move(data()[i + 1]);
        pop_back();
    }
    bool remove(const T& value) {
        const int index = index_of(value);
        if (index < 0) return false;
        erase_at(static_cast<size_t>(index));
        return true;
    }
    template<class Pred> size_t remove_all(Pred pred) {
        size_t write = 0, original = size_;
        for (size_t read = 0; read < original; ++read)
            if (!pred(data()[read])) {
                if (write != read) data()[write] = std::move(data()[read]);
                ++write;
            }
        while (size_ > write) pop_back();
        return original - write;
    }
    bool contains(const T& value) const { return index_of(value) >= 0; }
    int index_of(const T& value) const {
        for (size_t i = 0; i < size_; ++i) if (data()[i] == value) return static_cast<int>(i);
        return -1;
    }
    friend bool operator==(const SmallVec& a, const SmallVec& b) {
        return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
    }
private:
    void move_from(SmallVec&& source) {
        size_ = source.size_;
        if (source.overflow_) {
            overflow_ = std::move(source.overflow_); capacity_ = source.capacity_;
            source.size_ = 0;
        } else {
            for (size_t i = 0; i < size_; ++i) inline_[i] = std::move(source.inline_[i]);
            source.clear();
        }
        source.capacity_ = kInline;
    }
    std::array<T, kInline> inline_{};
    std::unique_ptr<T[]> overflow_;
    size_t capacity_ = kInline;
    size_t size_ = 0;
};

} // namespace sts2
