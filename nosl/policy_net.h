// The actor-critic's forward pass without any ML runtime, for deployment:
// the same weights python/nosl/model.py trains (written by nosl.export),
// one observation at a time, float32 on the CPU. tools/nosl/check_policy_net.py
// compares it with the PyTorch model.
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "nosl/observation.h"

namespace sts2::nosl {

class PolicyNet {
public:
    // Throws std::runtime_error on a file of another format, schema or shape.
    static std::unique_ptr<PolicyNet> Load(const std::string& path);
    ~PolicyNet();

    // One logit per action row of `obs`, and the value estimate.
    void Forward(const Observation& obs, std::vector<float>& logits, float& value) const;

private:
    struct Weights;
    explicit PolicyNet(std::unique_ptr<Weights> weights);
    std::unique_ptr<Weights> w_;
};

}  // namespace sts2::nosl
