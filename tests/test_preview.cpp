#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "env/preview.h"
#include "env/game_queries.h"
#include "env/snapshot_json.h"
#include "game/All.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/PotionCmd.h"

using namespace sts2;
static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
static std::unique_ptr<Env> combat(EncounterId encounter = EncounterId::CULTISTS_NORMAL) {
    auto env = std::make_unique<Env>();
    env->ResetCombat(NewRunOptions{.seed = "DUAL001", .character = CharacterId::IRONCLAD},
                     encounter);
    return env;
}
static void roundtrip(const Env& source, bool step = true) {
    auto restored = Env::ImportSnapshotJson(source.ExportSnapshotJson());
    require(restored->ExportSnapshotJson() == source.ExportSnapshotJson(), "run snapshot restore changed fields");
    const auto actions = source.LegalActions();
    require(actions == restored->LegalActions(), "restored legal actions changed");
    if (step && !actions.empty())
        require(source.Predict(actions.back())->Hash() == restored->Predict(actions.back())->Hash(),
                "restored run transition diverged");
}

int main() {
    auto root = combat();
    const auto source_hash = root->Hash();
    auto traced = PreviewAction(*root, Action{.kind = Action::EndTurn});
    auto plain = root->Predict(Action{.kind = Action::EndTurn});
    auto untraced = PreviewAction(*root, Action{.kind = Action::EndTurn}, {.collect_trace = false});
    require(traced.successor->Hash() == plain->Hash() && untraced.successor->Hash() == plain->Hash(),
            "observation changed ordinary execution");
    require(root->Hash() == source_hash && root->sim().execution_trace == nullptr,
            "preview changed source or retained an observer");
    require(!traced.trace.events.empty() && untraced.trace.events.empty(), "trace enable contract");
    require(traced.successor->sim().execution_trace == nullptr, "successor retained borrowed trace");
    try {
        PreviewAction(*root, Action{.kind = Action::EndTurn}, {.trace_limit = 1});
        throw std::runtime_error("trace overflow accepted");
    } catch (const TraceLimitExceeded&) { require(root->Hash() == source_hash, "overflow changed source"); }
    const auto preview_json = snapshot_detail::Parser(traced.ExportJson()).parse();
    require(preview_json.at("format").text == "sts2env.preview", "preview JSON format");
    require(!preview_json.at("trace_complete").boolean, "partial trace claims completeness");

    ExportCombatQueries(*root); ExportCombatCalculatedVars(*root); ExportCombatOrbs(*root); ExportCombatPowers(*root);
    require(root->Hash() == source_hash, "query changed source");

    // Contract fixture: source HP is below full, final enemy HP is one. The
    // vanilla victory hook (BurningBlood.cs) heals 6 and offers normal rewards.
    // This checks the prior prediction bypass; it is not an oracle fidelity test.
    auto lethal = combat(EncounterId::FROG_KNIGHT_NORMAL);
    Player player(&lethal->sim(), 1);
    player.Creature().st().current_hp = 40;
    const auto enemies = CombatState(&lethal->sim()).Enemies();
    require(enemies.size() == 1, "single enemy fixture");
    enemies[0].st().current_hp = 1;
    enemies[0].st().block = 0;
    const int target_id = enemies[0].CombatId();
    CardModel strike = CombatState(&lethal->sim()).CreateCard(CardId::STRIKE_IRONCLAD, player);
    CardPileCmd::Add(strike, PileType::Hand);
    const Action attack{.kind = Action::PlayCard, .index = player.PlayerCombatState().Hand().Count() - 1,
                        .target = target_id};
    const auto lethal_hash = lethal->Hash();
    auto win = PreviewAction(*lethal, attack);
    require(win.successor->Hash() == lethal->Predict(attack)->Hash(), "lethal preview bypassed ordinary victory");
    require(win.successor->decision().kind == DecisionKind::Rewards, "preview skipped combat rewards");
    require(Player(&win.successor->sim(), 1).Creature().CurrentHp() == 46, "preview skipped vanilla victory heal");
    require(lethal->Hash() == lethal_hash, "lethal preview changed source");
    require(std::any_of(win.trace.events.begin(), win.trace.events.end(), [&](const auto& event) {
        return event.kind == ExecutionEventKind::DamageResolved && event.target_killed
            && event.creature_combat_id == static_cast<uint32_t>(target_id);
    }), "lethal damage result missing after normal cleanup");
    roundtrip(*win.successor);
    bool reward_choice_tested = false;
    for (const auto& action : win.successor->LegalActions()) {
        if (action.kind != Action::TakeReward) continue;
        auto reward = PreviewAction(*win.successor, action);
        if (reward.successor->decision().kind == DecisionKind::Choice) {
            roundtrip(*reward.successor);
            reward_choice_tested = true;
            break;
        }
    }
    require(reward_choice_tested, "noncombat reward choice restore was not exercised");

    // The potion path also reaches the normal ActionExecutor win check.
    auto potion = combat(EncounterId::FROG_KNIGHT_NORMAL);
    CombatState(&potion->sim()).Enemies()[0].st().current_hp = 1;
    CombatState(&potion->sim()).Enemies()[0].st().block = 0;
    require(PotionCmd::TryToProcure(PotionId::FIRE_POTION, Player(&potion->sim(), 1)).success, "fire potion fixture");
    auto potion_actions = potion->LegalActions();
    auto use = std::find_if(potion_actions.begin(), potion_actions.end(), [](const auto& action) {
        return action.kind == Action::UsePotion;
    });
    require(use != potion_actions.end(), "no potion action");
    auto potion_win = PreviewAction(*potion, *use);
    require(potion_win.successor->decision().kind == DecisionKind::Rewards, "potion victory did not offer rewards");

    // Discovery pauses inside an effect. Answer returns the replayed operation
    // prefix, including exactly one original play-start, not appended duplicates.
    auto choosing = combat();
    Player chooser(&choosing->sim(), 1);
    CardModel discovery = CombatState(&choosing->sim()).CreateCard(CardId::DISCOVERY, chooser);
    CardPileCmd::Add(discovery, PileType::Hand);
    auto choice = PreviewAction(*choosing, Action{.kind = Action::PlayCard,
        .index = chooser.PlayerCombatState().Hand().Count() - 1});
    require(choice.successor->decision().kind == DecisionKind::Choice, "choice fixture did not pause");
    const auto choice_action=Action{.kind=Action::PlayCard,.index=chooser.PlayerCombatState().Hand().Count()-1};
    auto causal_choice=PreviewAction(*choosing,choice_action,{.collect_causal=true});
    require(causal_choice.successor->ExportSnapshotJson()==choice.successor->ExportSnapshotJson(),
        "method values changed choice prefix state");
    bool observed_unwind=false;
    for(const auto& frame:causal_choice.trace.frames) {
        if((frame.kind=="model_method" || frame.kind=="hook") && frame.unwound) {
            observed_unwind=true;
            require(frame.method_values_observed && !frame.method_returned && !frame.return_value
                && frame.parameters_after.empty(),"choice unwind fabricated completion arguments/result");
        }
    }
    require(observed_unwind,"choice method unwind was not observed");
    const auto encoded_choice=snapshot_detail::Parser(causal_choice.ExportJson()).parse();
    for(const auto& frame:encoded_choice.at("frames").array) {
        if(frame.at("unwound").boolean && frame.at("method_values_observed").boolean)
            require(!frame.at("method_returned").boolean && frame.at("return_value").is_null()
                && frame.at("parameters_after").array.empty(),"wire choice unwind fabricated completion");
    }
    auto restored_choice = Env::ImportSnapshotJson(choice.successor->ExportSnapshotJson());
    auto answer = choice.successor->LegalActions().front();
    auto unsupported_choice = choice.successor->Clone();
    const auto unsupported_card = unsupported_choice->state().player_combat.piles[0][0];
    unsupported_choice->state().cards[unsupported_card].id = CardId::BEACON_OF_HOPE;
    unsupported_choice->state().cards[unsupported_card].var_overrides.clear();
    const auto unsupported_hash = unsupported_choice->Hash();
    try {
        PreviewAction(*unsupported_choice, answer);
        throw std::runtime_error("active unported choice model accepted");
    } catch (const Unported&) { require(unsupported_choice->Hash() == unsupported_hash, "unported choice changed root"); }
    try {
        Env::ImportSnapshotJson(unsupported_choice->ExportSnapshotJson());
        throw std::runtime_error("active unported choice imported");
    } catch (const Unported&) { }
    auto answered = PreviewAction(*restored_choice, answer);
    require(answered.choice_replayed, "answer did not advertise replay trace");
    require(answered.successor->Hash() == choice.successor->Predict(answer)->Hash(), "answer replay diverged");
    auto causal_answer=PreviewAction(*restored_choice,answer,{.collect_causal=true});
    require(causal_answer.successor->ExportSnapshotJson()==answered.successor->ExportSnapshotJson(),
        "method values changed answered operation prefix state");
    require(std::any_of(causal_answer.trace.frames.begin(),causal_answer.trace.frames.end(),[](const auto& frame) {
        return frame.operation=="CardModel.OnPlay" && frame.method_returned && frame.return_value
            && frame.return_value->json=="{\"kind\":\"awaited_completion\"}";
    }),"answered Task method omitted awaited completion");
    require(std::count_if(answered.trace.events.begin(), answered.trace.events.end(), [](const auto& event) {
        return event.kind == ExecutionEventKind::CardPlayStarted;
    }) == 1, "answer duplicated play trace");

    // Whole-run restore at real room entry boundaries. Room type is selected
    // explicitly in the fixture, then the ordinary room-entry code runs.
    auto map_root = std::make_unique<Env>();
    new_run(map_root->state(), NewRunOptions{.seed = "RFRESTORE", .character = CharacterId::IRONCLAD});
    map_root->state().run.started_with_neow = false;
    map_root->Step(Action{.kind = Action::StartRun});
    require(map_root->decision().kind == DecisionKind::Map, "map fixture");
    roundtrip(*map_root);
    const Action move = map_root->LegalActions().front();
    const MapPointType room_types[] = {MapPointType::Shop, MapPointType::RestSite, MapPointType::Treasure};
    const DecisionKind decisions[] = {DecisionKind::Shop, DecisionKind::RestSite, DecisionKind::Treasure};
    for (size_t i = 0; i < std::size(room_types); ++i) {
        auto room = map_root->Clone();
        const int point = room->state().run.map.get_point(move.coord.col, move.coord.row);
        require(point >= 0, "room fixture map coordinate");
        room->state().run.map.points[static_cast<size_t>(point)].type = room_types[i];
        room->Step(move);
        require(room->decision().kind == decisions[i], "room entry fixture");
        roundtrip(*room);
        if (decisions[i] == DecisionKind::RestSite) {
            const auto actions = room->LegalActions();
            require(!actions.empty(), "rest choice fixture has no actions");
            auto smith = PreviewAction(*room, actions.back());
            require(smith.successor->decision().kind == DecisionKind::Choice, "smith did not request a card");
            roundtrip(*smith.successor);
        }
    }
    Env event;
    event.Reset(NewRunOptions{.seed = "DUAL001", .character = CharacterId::IRONCLAD});
    require(event.decision().kind == DecisionKind::Event, "Neow event fixture");
    roundtrip(event, false);
    auto loss = combat();
    Player(&loss->sim(), 1).Creature().st().current_hp = 1;
    for (int i = 0; i < 8 && loss->decision().kind == DecisionKind::Combat; ++i)
        loss = loss->Predict(Action{.kind = Action::EndTurn});
    require(loss->decision().kind == DecisionKind::Ended, "loss fixture did not end");
    roundtrip(*loss);
    std::cout << "Generic Preview checks passed: ordinary victory/potion/choice, trace isolation/capacity, run restore.\n";
}
