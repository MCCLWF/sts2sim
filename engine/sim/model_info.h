// Reflection data of a model (which framework members its C# class
// overrides), looked up by kind and dense id.
#pragma once

#include <array>
#include <cstdint>

#include "content/generated/tables.h"

namespace sts2 {

inline const ModelInfo* model_info(ModelKind kind, uint16_t index) {
    struct Table {
        std::array<uint32_t, static_cast<size_t>(ModelKind::Count) + 1> start{};
        Table() {
            auto infos = all_model_infos();
            std::array<uint32_t, static_cast<size_t>(ModelKind::Count)> count{};
            for (const ModelInfo& m : infos) ++count[static_cast<size_t>(m.kind)];
            for (size_t k = 0; k < count.size(); ++k) start[k + 1] = start[k] + count[k];
        }
    };
    static const Table table;
    size_t k = static_cast<size_t>(kind);
    uint32_t i = table.start[k] + index - 1;  // dense ids start at 1
    if (index == 0 || i >= table.start[k + 1]) return nullptr;
    return &all_model_infos()[i];
}

}  // namespace sts2
