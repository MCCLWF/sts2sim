#include <iostream>
#include <stdexcept>
#include "engine/containers.h"
#include "engine/sim/pool.h"

using namespace sts2;

static void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    try {
        StaticVec<int, 2> list{7, 8};
        list.push_back(list.front()); // The argument aliases storage being grown.
        require(list[2] == 7, "self-append lost its value at growth");
        for (int i = 3; i < 70000; ++i) list.push_back(i);
        list.insert(1, list.back());
        require(list[1] == 69999 && list[2] == 8, "insert lost order while growing");
        list.erase_at(1);
        auto copy = list;
        copy[0] = -1;
        require(list[0] == 7 && copy.size() == list.size(), "list copy shared mutable storage");
        auto moved = std::move(copy);
        require(copy.empty() && moved.back() == 69999, "overflow list move corrupted storage");
        StaticVec<StaticVec<int, 2>, 2> nested;
        for (int i = 0; i < 40; ++i) nested.push_back(list);
        auto nested_copy = nested;
        nested_copy[25][50000] = -2;
        require(nested[25][50000] == 50000, "nested state copy shared an allocation");
        nested_copy.remove_all([](const auto& value) { return value[50000] == -2; });
        require(nested_copy.size() == 39 && nested.size() == 40, "remove_all corrupted nested owners");

        Pool<StaticVec<int, 2>, 4> pool;
        const uint32_t first = pool.alloc();
        pool[first] = {1, 2, 3};
        auto* stable = &pool[first];
        uint32_t last = first;
        for (int i = 1; i < 70000; ++i) last = pool.alloc();
        require(last == 70000 && pool.size() == 70000 && stable == &pool[first],
                "pool growth truncated a reference or moved a live object");
        pool.free(2); pool.free(40000);
        require(pool.alloc() == 2 && pool.alloc() == 40000, "pool changed lowest-free allocation order");
        auto pool_copy = pool;
        pool_copy[first][0] = 99;
        require(pool[first][0] == 1, "pool clone shared a nested allocation");
        Pool<StaticVec<int, 2>, 4> restored;
        pool.visit_snapshot([&](auto next, auto count) {
            restored.load_snapshot(pool.capacity(), next, count, [&](auto add) {
                pool.visit_snapshot([](auto, auto) {}, [&](auto ref, const auto& item) { add(ref, item); });
            });
        }, [](auto, const auto&) {});
        require(restored[first] == pool[first] && restored.alloc() == pool.alloc(),
                "dynamic pool restore changed values or next allocation");
        // Trim/reuse across block boundaries preserves the exact addressable
        // extent and clears removed owners. A rejected trim preserves live
        // references; moves transfer ownership without moving slot values.
        Pool<StaticVec<int, 2>, 4> trimmed;
        for (int i = 0; i < 17; ++i) {
            const auto ref = trimmed.alloc(); trimmed[ref] = {i, i + 1, i + 2};
        }
        auto* kept = &trimmed[1];
        require(!trimmed.trim_capacity(8) && kept == &trimmed[1], "trim removed a live slot");
        for (uint32_t i = 9; i <= 17; ++i) trimmed.free(i);
        require(trimmed.trim_capacity(8) && trimmed.capacity() == 8 && trimmed.alloc() == 9,
                "trim changed logical extent or next allocation");
        require(trimmed[9].empty() && kept == &trimmed[1], "trim/reuse retained an old owner or moved a live slot");
        auto pool_moved = std::move(trimmed);
        require(kept == &pool_moved[1] && trimmed.capacity() == 0 && trimmed.size() == 0 && trimmed.alloc() == 1,
                "pool move invalidated live addresses or its empty source");
        Pool<StaticVec<int, 2>, 4> assigned;
        assigned = std::move(pool_moved);
        require(kept == &assigned[1] && pool_moved.alloc() == 1, "move assignment invalidated storage");
        assigned.clear();
        require(assigned.capacity() == 0 && assigned.alloc() == 1 && assigned[1].empty(),
                "clear retained extent or owned values");
        try { list.reserve(list.max_size() + 1); throw std::runtime_error("out-of-range list count accepted"); }
        catch (const CapacityError&) {}
        std::cout << "dynamic storage: 70000 references, nested clones, alias growth and restore passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "dynamic storage: " << error.what() << '\n';
        return 1;
    }
}
