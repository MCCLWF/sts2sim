// See actor.h.
#include "nosl/actor.h"

#include <cmath>

#include "engine/sim/errors.h"

namespace sts2::nosl {

Actor::Actor(const std::string& weights, int32_t actor_filter, uint64_t policy_seed, bool greedy)
    : filter_(actor_filter), net_(PolicyNet::Load(weights)), rng_(policy_seed), greedy_(greedy) {}

Actor::Decision Actor::Decide(Observation& obs) {
    filter_.Apply(obs);
    if (obs.legal.empty()) throw GameError("actor: a decision without a legal action");
    Decision d;
    d.rows = obs.legal.size();
    d.forced = d.rows == 1;
    if (!d.forced) {
        std::vector<float> logits;
        net_->Forward(obs, logits, d.value);
        float top = logits[0];
        for (float x : logits) top = std::max(top, x);
        std::vector<double> p(logits.size());
        double total = 0;
        for (size_t i = 0; i < logits.size(); ++i) { p[i] = std::exp(static_cast<double>(logits[i] - top)); total += p[i]; }
        if (greedy_) {
            for (size_t i = 1; i < p.size(); ++i) if (p[i] > p[d.row]) d.row = i;
        } else {
            // One uniform draw per decision; rows are in the observation's canonical order.
            double u = std::uniform_real_distribution<double>(0.0, total)(rng_);
            d.row = p.size() - 1;
            for (size_t i = 0; i < p.size(); ++i) { if (u < p[i]) { d.row = i; break; } u -= p[i]; }
        }
        d.prob = p[d.row] / total;
    }
    d.action = obs.legal[d.row];
    filter_.OnAction(d.action);
    return d;
}

}  // namespace sts2::nosl
