// Offline oracle action adapter. This preserves the original CLI replay
// mapping and choice micro-step order; consumers can observe each real Step.
#pragma once
#include <string>
#include <vector>
#include "env/env.h"
#include "engine/sim/errors.h"
namespace sts2::offline {
template <class Step>
void ApplyOracleAction(Env& env, const std::vector<std::string>& a, Step&& step) {
    const std::string& k = a.at(0);
    bool hold_rest = false;
    for (size_t i = 1; i < a.size(); ++i) {
        if (a[i].starts_with("rest_")) {
            if (a[i] != "rest_hold" || i + 1 != a.size())
                throw GameError("replay: invalid rest continuation marker");
            hold_rest = true;
        }
    }
    auto num = [&](size_t i) { return std::stoi(a.at(i)); };
    Decision d = env.decision();
    if (k == "map") {
        step(Action{.kind = Action::MapMove, .coord = MapCoord{static_cast<int8_t>(num(1)), static_cast<int8_t>(num(2))}});
    } else if (k == "play") {
        step(Action{.kind = Action::PlayCard, .index = num(1), .target = num(2)});
    } else if (k == "potion") {
        step(Action{.kind = Action::UsePotion, .index = num(1), .target = num(2)});
    } else if (k == "discard") {
        step(Action{.kind = Action::DiscardPotion, .index = num(1)});
    } else if (k == "end") {
        step(Action{.kind = Action::EndTurn});
    } else if (k == "reward" || k == "proceed") {
        if (d.kind == DecisionKind::Choice && d.request.source == "rewards") {
            // A nested rewards screen: the reward index, or the set size to proceed.
            int32_t answer = k == "reward" ? num(1) : d.request.option_count - 1;
            step(Action{.kind = Action::Answer, .index = answer});
        } else if (k == "reward") {
            step(Action{.kind = Action::TakeReward, .index = num(1)});
        } else {
            step(Action{.kind = Action::Proceed});
        }
    } else if (k == "rest") {
        if (num(1) < 0) throw GameError("replay: the oracle chose no rest site option");
        step(Action{.kind = Action::RestOption, .index = num(1)});
    } else if (k == "relic") {
        step(Action{.kind = Action::TreasurePick, .index = num(1)});
    } else if (k == "relicskip" || k == "leave") {
        step(Action{.kind = Action::Proceed});
    } else if (k == "bundle") {
        if (d.kind != DecisionKind::Choice || d.request.kind != ChoiceKind::Bundle) throw GameError("replay: a bundle choice, but the simulator asks for none");
        step(Action{.kind = Action::Answer, .index = num(1)});
    } else if (k == "shop") {
        step(Action{.kind = Action::ShopBuy, .index = num(1)});
    } else if (k == "event") {
        step(Action{.kind = Action::EventOption, .index = num(1)});
    } else if (k == "choose") {
        if (d.kind != DecisionKind::Choice || d.request.kind != ChoiceKind::Option)
            throw GameError("replay: an option choice, but the simulator asks for none");
        step(Action{.kind = Action::Answer, .index = num(1)});
    } else if (k == "select") {
        if (d.kind != DecisionKind::Choice) throw GameError("replay: a card selection, but the simulator asks for no choice");
        int n = num(1);
        if (d.request.subset) {
            // CardSelectCmd: the count, then the indices in selection order.
            step(Action{.kind = Action::Answer, .index = n});
            for (int i = 0; i < n; ++i) step(Action{.kind = Action::Answer, .index = num(2 + static_cast<size_t>(i))});
        } else {
            // A card reward: the card, or the Skip alternative after the cards.
            int32_t answer = n == 0 ? static_cast<int32_t>(d.request.cards.size()) : num(2);
            step(Action{.kind = Action::Answer, .index = answer});
        }
    } else {
        throw Unported("replay: action `" + k + "`");
    }
    // Legacy OracleRun leaves after one rest option. Completion can follow
    // a nested card choice, reward, or reward-screen Proceed. Keep this
    // policy adapter here so the public Env retains MiniatureTent choices.
    // A frozen trace whose next actual decision is rest has rest_hold;
    // that suppresses only this adapter's automatic exit, never an Env Step.
    const auto& rest = env.state().run.rest_site;
    if (!hold_rest && env.decision().kind == DecisionKind::RestSite && rest.last_chosen >= 0 && rest.proceed_enabled)
        step(Action{.kind = Action::Proceed});
}
} // namespace sts2::offline
