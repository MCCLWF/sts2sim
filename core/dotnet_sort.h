// Port of .NET's introspective sort (System.Collections.Generic.
// GenericArraySortHelper<T>, used by List<T>.Sort() for IComparable<T>).
//
// The game sorts before shuffling (ListExtensions.StableShuffle) and
// CardModel.CompareTo returns 0 for distinct cards with the same id and upgrade
// level, so the placement of ties must match .NET exactly. std::sort does not.
#pragma once

#include <bit>
#include <cstdint>
#include <span>
#include <utility>

namespace sts2 {

namespace dotnet_sort_detail {

inline constexpr int kIntrosortSizeThreshold = 16;

// cmp(a, b) returns <0, 0, >0 like IComparable<T>.CompareTo.
template <class T, class Cmp>
void swap_if_greater(std::span<T> keys, Cmp& cmp, size_t i, size_t j) {
    if (cmp(keys[i], keys[j]) > 0) std::swap(keys[i], keys[j]);
}

template <class T, class Cmp>
void insertion_sort(std::span<T> keys, Cmp& cmp) {
    for (size_t i = 0; i + 1 < keys.size(); ++i) {
        T t = keys[i + 1];
        ptrdiff_t j = static_cast<ptrdiff_t>(i);
        while (j >= 0 && cmp(t, keys[static_cast<size_t>(j)]) < 0) {
            keys[static_cast<size_t>(j + 1)] = keys[static_cast<size_t>(j)];
            --j;
        }
        keys[static_cast<size_t>(j + 1)] = t;
    }
}

template <class T, class Cmp>
void down_heap(std::span<T> keys, size_t i, size_t n, Cmp& cmp) {
    T d = keys[i - 1];
    while (i <= n / 2) {
        size_t child = 2 * i;
        if (child < n && cmp(keys[child - 1], keys[child]) < 0) ++child;
        if (!(cmp(d, keys[child - 1]) < 0)) break;
        keys[i - 1] = keys[child - 1];
        i = child;
    }
    keys[i - 1] = d;
}

template <class T, class Cmp>
void heap_sort(std::span<T> keys, Cmp& cmp) {
    size_t n = keys.size();
    for (size_t i = n / 2; i >= 1; --i) down_heap(keys, i, n, cmp);
    for (size_t i = n; i > 1; --i) {
        std::swap(keys[0], keys[i - 1]);
        down_heap(keys, 1, i - 1, cmp);
    }
}

template <class T, class Cmp>
size_t pick_pivot_and_partition(std::span<T> keys, Cmp& cmp) {
    size_t hi = keys.size() - 1;
    size_t middle = hi >> 1;
    swap_if_greater(keys, cmp, 0, middle);
    swap_if_greater(keys, cmp, 0, hi);
    swap_if_greater(keys, cmp, middle, hi);

    T pivot = keys[middle];
    std::swap(keys[middle], keys[hi - 1]);
    size_t left = 0, right = hi - 1;
    while (left < right) {
        while (left < hi - 1 && cmp(pivot, keys[++left]) > 0) {
        }
        while (right > 0 && cmp(pivot, keys[--right]) < 0) {
        }
        if (left >= right) break;
        std::swap(keys[left], keys[right]);
    }
    if (left != hi - 1) std::swap(keys[left], keys[hi - 1]);
    return left;
}

template <class T, class Cmp>
void intro_sort(std::span<T> keys, int depth_limit, Cmp& cmp) {
    size_t partition_size = keys.size();
    while (partition_size > 1) {
        if (partition_size <= kIntrosortSizeThreshold) {
            if (partition_size == 2) {
                swap_if_greater(keys, cmp, 0, 1);
                return;
            }
            if (partition_size == 3) {
                swap_if_greater(keys, cmp, 0, 1);
                swap_if_greater(keys, cmp, 0, 2);
                swap_if_greater(keys, cmp, 1, 2);
                return;
            }
            insertion_sort(keys.first(partition_size), cmp);
            return;
        }
        if (depth_limit == 0) {
            heap_sort(keys.first(partition_size), cmp);
            return;
        }
        --depth_limit;
        size_t p = pick_pivot_and_partition(keys.first(partition_size), cmp);
        intro_sort(keys.subspan(p + 1, partition_size - (p + 1)), depth_limit, cmp);
        partition_size = p;
    }
}

}  // namespace dotnet_sort_detail

// Equivalent of List<T>.Sort() / Array.Sort<T>() with an IComparable<T> order.
template <class T, class Cmp>
void dotnet_sort(std::span<T> keys, Cmp cmp) {
    if (keys.size() > 1) {
        int depth = 2 * (std::bit_width(static_cast<uint32_t>(keys.size())) - 1 + 1);
        dotnet_sort_detail::intro_sort(keys, depth, cmp);
    }
}

}  // namespace sts2
