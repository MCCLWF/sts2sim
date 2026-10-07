// See actor_filter.h.
#include "nosl/actor_filter.h"

#include <algorithm>

#include "core/xxhash64.h"

namespace sts2::nosl {

void ActorFilter::Reset() {
    floor_ = -1;
    current_ = 0;
    last_action_.clear();
    banned_.clear();
}

void ActorFilter::Apply(Observation& obs) {
    if (version_ == kActorFilterNone) return;
    if (obs.global[kGTotalFloor] != floor_) {  // a new floor: nothing from the last one can repeat
        floor_ = obs.global[kGTotalFloor];
        last_action_.clear();
        banned_.clear();
    }
    uint64_t h = xxhash64(obs.global.data(), obs.global.size() * sizeof(int32_t), 0);
    h = xxhash64(obs.tokens.data(), obs.tokens.size() * sizeof(int32_t), h);
    h = xxhash64(obs.actions.data(), obs.actions.size() * sizeof(int32_t), h);
    current_ = h;
    // Back at an observation an action was already taken from: that action undid itself.
    if (auto seen = last_action_.find(h); seen != last_action_.end()) {
        std::vector<Action>& list = banned_[h];
        if (std::find(list.begin(), list.end(), seen->second) == list.end()) list.push_back(seen->second);
    }
    const auto banned = banned_.find(h);
    const bool slots_full = obs.global[kGPotionCount] >= obs.global[kGPotionSlots];
    std::vector<size_t> keep;
    for (size_t i = 0; i < obs.legal.size(); ++i) {
        const Action& a = obs.legal[i];
        if (a.kind == Action::DiscardPotion && !slots_full) continue;
        if (banned != banned_.end() && std::find(banned->second.begin(), banned->second.end(), a) != banned->second.end()) continue;
        keep.push_back(i);
    }
    if (keep.empty() || keep.size() == obs.legal.size()) return;
    std::vector<Action> legal;
    std::vector<int32_t> rows;
    for (size_t i : keep) {
        legal.push_back(obs.legal[i]);
        rows.insert(rows.end(), obs.actions.begin() + static_cast<std::ptrdiff_t>(i * kActionWidth),
                    obs.actions.begin() + static_cast<std::ptrdiff_t>((i + 1) * kActionWidth));
    }
    obs.legal = std::move(legal);
    obs.actions = std::move(rows);
    obs.global[kGActionCount] = static_cast<int32_t>(obs.legal.size());
}

void ActorFilter::OnAction(const Action& action) {
    if (version_ != kActorFilterNone) last_action_[current_] = action;
}

}  // namespace sts2::nosl
