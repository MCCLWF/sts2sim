// Host resource policy, not a game-rule limit. Configure before loading the
// native library; the API descriptor exposes the effective immutable values.
#pragma once
#include <charconv>
#include <cstdlib>
#include <string>
#include "engine/sim/errors.h"

namespace sts2 {
struct ResourceLimits {
    size_t snapshot_bytes = 128 * 1024 * 1024;
    size_t json_nodes = 8000000;
    size_t restored_storage_bytes = 512 * 1024 * 1024;
};
inline const ResourceLimits& resource_limits() {
    static const ResourceLimits limits = [] {
        ResourceLimits value;
        auto configure = [](const char* name, size_t& destination) {
            const char* configured = std::getenv(name);
            if (!configured) return;
            const std::string text(configured);
            size_t parsed = 0;
            auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), parsed);
            if (error != std::errc{} || end != text.data() + text.size() || !parsed)
                throw CapacityError(std::string("invalid positive resource limit: ") + name);
            destination = parsed;
        };
        configure("STS2_ENV_MAX_SNAPSHOT_BYTES", value.snapshot_bytes);
        configure("STS2_ENV_MAX_JSON_NODES", value.json_nodes);
        configure("STS2_ENV_MAX_RESTORED_STORAGE_BYTES", value.restored_storage_bytes);
        return value;
    }();
    return limits;
}
class SnapshotReadBudget {
public:
    void allocation(size_t count, size_t element_size) {
        const size_t limit = resource_limits().restored_storage_bytes;
        if (count > (limit - used_) / element_size)
            throw CapacityError("snapshot: configured restored storage resource budget exceeded");
        used_ += count * element_size;
    }
private:
    size_t used_ = 0;
};
} // namespace sts2
