// Commands/RewardsCmd.cs; the screen loop is the TestMode path of
// RewardsSet.Offer (the selector awaits the set: the oracle's SelectRewards).
#include "game/Commands/RewardsCmd.h"

namespace sts2::RewardsCmd {

RewardsSet NewCustomSet(Sim* sim) {
    // One set at a time (run.rewards): a set offered while another one is on
    // screen is not ported.
    return RewardsSet::NewCustom(sim, {});
}

void OfferAndAwait(RewardsSet set) {
    if (!set.Offer()) return;
    if (set.IsCompleted()) return;
    Sim* sim = &set.Player().sim();
    for (;;) {
        size_t n = set.Rewards().size();
        ChoiceRequest request;
        request.kind = ChoiceKind::Option;
        request.option_count = static_cast<int32_t>(n) + 1;
        request.source = "rewards";
        request.prompt = "take a reward, or proceed";
        for (size_t i = 0; i < n; ++i)
            request.options.push_back({.kind = ChoiceOptionKind::Reward,
                                       .reward = set.Rewards()[i].Index(),
                                       .enabled = !set.Rewards()[i].SuccessfullySelected()});
        request.options.push_back({.kind = ChoiceOptionKind::Proceed, .enabled = !set.st().disallow_skipping});
        int32_t a = sim->Choose(request);
        if (a == static_cast<int32_t>(n)) {
            if (set) set.Skip();  // SkipLocalRewardsSet, only while rewards are left
            return;
        }
        if (a < 0 || a > static_cast<int32_t>(n)) throw GameError("rewards: answer out of range");
        Reward reward = set.Rewards()[static_cast<size_t>(a)];
        if (reward.SuccessfullySelected()) throw GameError("rewards: the reward was already taken");
        set.SelectReward(reward);
        // SelectLocalReward completes the original await as soon as every
        // reward is selected. Custom screens have no extra Proceed decision.
        if (set.IsCompleted()) return;
    }
}

void OfferCustom(Player player, std::span<const Reward> rewards) {
    Sim* sim = &player.sim();
    bool nested = static_cast<bool>(RewardsSet(sim));
    RewardsSetState saved;
    if (nested) saved = sim->g->run.rewards;
    RewardsSet set = NewCustomSet(sim);
    for (Reward r : rewards) set.Rewards().Add(r);
    OfferAndAwait(set);
    if (nested) sim->g->run.rewards = saved;
}

}  // namespace sts2::RewardsCmd
