#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "env/preview.h"
#include "env/operation_json.h"
#include "engine/json.h"
#include "game/All.h"
#include "game/Commands/CreatureCmd.h"
#include "game/Commands/PotionCmd.h"
#include "game/Commands/RelicCmd.h"
#include "game/Combat/CombatManager.h"

using namespace sts2;
static void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static Operation request(const Env& env, Operation::Kind kind) {
    Operation result; result.kind = kind; result.source_hash = env.Hash(); return result;
}
static void source_binding_and_prefix() {
    Env source;
    source.ResetCombat({.seed = "APIOPCONTRACT", .character = CharacterId::IRONCLAD}, EncounterId::CULTISTS_NORMAL);
    auto op = request(source, Operation::Kind::ObtainRelic);
    op.relic_id = static_cast<uint16_t>(RelicId::ASTROLABE);
    const auto source_snapshot = source.ExportSnapshotJson();
    auto preview = PreviewOperation(source, op, {.capture_run_results = true});
    require(source.ExportSnapshotJson() == source_snapshot, "hypothetical relic operation mutated source");
    require(preview.successor->decision().kind == DecisionKind::Choice, "Astrolabe failed to expose native choice");
    require(preview.ExportJson().find("\"operation_status\":\"needs_choice\"") != std::string::npos,
        "pending operation was labeled complete");
    size_t answers = 0;
    while (preview.successor->decision().kind == DecisionKind::Choice) {
        const auto snapshot = preview.successor->ExportSnapshotJson();
        auto restored = Env::ImportSnapshotJson(snapshot);
        require(restored->Hash() == preview.successor->Hash(), "typed pending operation restore changed prefix");
        const auto legal = restored->LegalActions();
        require(!legal.empty(), "native selection has no answers");
        // Astrolabe's ordered selection: legal maximum count, then first indices.
        const auto action = restored->decision().request.prompt == "count" ? legal.back() : legal.front();
        auto resumed = PreviewAction(*restored, action);
        auto expected = preview.successor->Predict(action);
        require(resumed.successor->Hash() == expected->Hash(), "snapshot lost typed operation continuation");
        preview = std::move(resumed);
        require(++answers < 16, "typed selection did not complete");
    }
    require(answers >= 2 && source.ExportSnapshotJson() == source_snapshot, "prefix replay mutated original");
    require(preview.ExportJson().find("\"operation_status\":\"complete\"") != std::string::npos,
        "completed continuation retained pending status");
    auto stale = op; ++stale.source_hash;
    try { source.PredictOperation(stale); throw std::runtime_error("stale refs were accepted"); }
    catch (const InvalidRequest&) { }
    auto duplicate = request(source, Operation::Kind::TransformCards);
    const uint16_t ref = Player(&source.sim(), 1).Deck().Cards().front().Index();
    duplicate.cards = {ref, ref};
    try { source.PredictOperation(duplicate); throw std::runtime_error("duplicate transforms accepted"); }
    catch (const InvalidRequest&) { }
    JsonWriter writer; WriteOperation(writer, op);
    require(ParseOperationJson(writer.str()) == op, "operation protocol lost source context");
}

static void combat_boundaries() {
    Env source;
    source.ResetCombat({.seed = "APIBOUNDARY", .character = CharacterId::IRONCLAD}, EncounterId::CULTISTS_NORMAL);
    auto op = request(source, Operation::Kind::Action);
    op.action = {.kind = Action::EndTurn}; op.stop_mask = 1;
    const auto before = source.ExportSnapshotJson();
    auto preview = PreviewOperation(source, op, {.capture_run_results = true});
    auto& paused = *preview.successor;
    require(paused.decision().kind == DecisionKind::Boundary
        && paused.decision().boundary->kind == ExecutionBoundaryKind::EnemyPhaseDone,
        "requested enemy phase did not suspend");
    require(preview.ExportJson().find("\"operation_status\":\"paused\"") != std::string::npos,
        "boundary operation was labeled complete");
    require(paused.state().combat.current_side == CombatSide::Enemy, "boundary already switched to player");
    require(paused.state().player_combat.turn_number == source.state().player_combat.turn_number,
        "boundary already started next player turn");
    require(paused.LegalActions() == std::vector<Action>{Action{.kind = Action::Resume}},
        "mid-operation boundary offered ordinary game actions");
    auto restored = Env::ImportSnapshotJson(paused.ExportSnapshotJson());
    require(restored->Hash() == paused.Hash(), "boundary snapshot failed to restore");
    auto successor = restored->Predict({.kind = Action::Resume});
    require(successor->Hash() == source.Predict(op.action)->Hash(), "resumed end turn differed from ordinary execution");
    require(source.ExportSnapshotJson() == before, "boundary observation mutated source");
    auto tampered = paused.ExportSnapshotJson();
    const auto ordinal = tampered.find("\"ordinal\":1");
    require(ordinal != std::string::npos, "boundary ordinal missing");
    tampered.replace(ordinal, std::string("\"ordinal\":1").size(), "\"ordinal\":2");
    try { Env::ImportSnapshotJson(tampered); throw std::runtime_error("invented boundary accepted"); }
    catch (const GameError&) { }

    // The vanilla CheckWinCondition safe point occurs after the last attack,
    // before EndCombatInternal clears the roster or creates room rewards.
    Env winning;
    winning.ResetCombat({.seed = "APISAFEOUTCOME", .character = CharacterId::IRONCLAD}, EncounterId::CULTISTS_NORMAL);
    const auto enemies = CombatState(&winning.sim()).Enemies();
    for (Creature enemy : enemies) enemy.st().current_hp = 1;
    // Retain one live enemy; native death cleanup is exercised by the card.
    for (size_t i = 1; i < enemies.size(); ++i) CreatureCmd::Kill(enemies[i]);
    const auto actions = winning.LegalActions();
    Action attack{}; bool found = false;
    const auto hand = Player(&winning.sim(), 1).PlayerCombatState().Hand().Cards();
    for (const auto& action : actions) {
        if (action.kind == Action::PlayCard && action.target >= 0
            && hand[action.index].Type() == CardType::Attack) { attack = action; found = true; break; }
    }
    require(found, "safe outcome fixture has no attack");
    op = request(winning, Operation::Kind::Action); op.action = attack; op.stop_mask = 2;
    auto outcome = PreviewOperation(winning, op);
    require(outcome.successor->decision().boundary
        && outcome.successor->decision().boundary->kind == ExecutionBoundaryKind::SafeOutcome,
        "winning attack passed requested safe outcome");
    const auto outcome_enemies = CombatState(&outcome.successor->sim()).Enemies();
    require(outcome.successor->state().combat.is_in_progress
        && std::all_of(outcome_enemies.begin(), outcome_enemies.end(), [](Creature c) { return c.IsDead(); }),
        "victory cleanup happened before outcome boundary");
    auto outcome_restored = Env::ImportSnapshotJson(outcome.successor->ExportSnapshotJson());
    auto completed = outcome_restored->Predict({.kind = Action::Resume});
    require(completed->Hash() == winning.Predict(attack)->Hash(), "resumed victory did not reproduce native settlement");
    auto rewards = request(*outcome.successor, Operation::Kind::GenerateRewards);
    rewards.room = static_cast<uint8_t>(outcome.successor->state().run.rooms.back());
    require(outcome.successor->PredictOperation(rewards)->Hash() == completed->Hash(),
        "reward operation bypassed original winning action settlement");
}

static void automatic_potion_discard() {
    Env source;
    source.ResetCombat({.seed = "APIAUTOPOTION", .character = CharacterId::IRONCLAD}, EncounterId::CULTISTS_NORMAL);
    Player player(&source.sim(), 1);
    PotionCmd::TryToProcure(PotionId::FAIRY_IN_A_BOTTLE, player);
    const auto legal = source.LegalActions();
    const auto discard = std::find_if(legal.begin(), legal.end(), [](Action a) { return a.kind == Action::DiscardPotion; });
    require(discard != legal.end(), "NPotionPopup automatic Discard button was omitted");
    require(std::none_of(legal.begin(), legal.end(), [](Action a) { return a.kind == Action::UsePotion; }),
        "automatic potion gained a manual Use action");
    auto result = source.Predict(*discard);
    require(Player(&result->sim(), 1).Potions().empty(), "Discard did not execute original automatic potion removal");
}

static void choice_after_boundary() {
    Env source;
    source.ResetCombat({.seed = "APIPAUSECHOICE", .character = CharacterId::IRONCLAD}, EncounterId::CULTISTS_NORMAL);
    auto relic = request(source, Operation::Kind::ObtainRelic);
    relic.relic_id = static_cast<uint16_t>(RelicId::TOASTY_MITTENS);
    auto root = source.PredictOperation(relic);
    auto turn = request(*root, Operation::Kind::Action);
    turn.action = {.kind = Action::EndTurn}; turn.stop_mask = 1;
    auto paused = root->PredictOperation(turn);
    require(paused->decision().kind == DecisionKind::Boundary, "turn did not stop before player start");
    auto restored = Env::ImportSnapshotJson(paused->ExportSnapshotJson());
    auto resume = request(*restored, Operation::Kind::Action);
    resume.action = {.kind = Action::Resume};
    auto resumed = PreviewOperation(*restored, resume);
    require(resumed.continuation_of == turn && resumed.choice_replayed,
        "typed Resume lost the original boundary request");
    auto branch = std::move(resumed.successor);
    auto ordinary = root->Predict(turn.action);
    require(branch->decision().kind == DecisionKind::Choice, "turn-start relic did not expose later choice");
    while (branch->decision().kind == DecisionKind::Choice) {
        branch = Env::ImportSnapshotJson(branch->ExportSnapshotJson());
        const auto legal = branch->LegalActions();
        const auto answer = branch->decision().request.prompt == "count" ? legal.back() : legal.front();
        auto typed_answer = request(*branch, Operation::Kind::Action);
        typed_answer.action = answer;
        auto preview = PreviewOperation(*branch, typed_answer);
        require(preview.continuation_of == turn, "typed Answer dropped the retained boundary policy");
        branch = std::move(preview.successor);
        ordinary = ordinary->Predict(answer);
    }
    require(branch->Hash() == ordinary->Hash(), "boundary skip was lost through later choice replay");
}

static void typed_action_choices() {
    Env source;
    source.ResetCombat({.seed = "APITYPEDACTIONCHOICE", .character = CharacterId::IRONCLAD}, EncounterId::CULTISTS_NORMAL);
    auto relic = request(source, Operation::Kind::ObtainRelic);
    relic.relic_id = static_cast<uint16_t>(RelicId::TOASTY_MITTENS);
    auto root = source.PredictOperation(relic);
    auto turn = request(*root, Operation::Kind::Action);
    turn.action = {.kind = Action::EndTurn};
    const auto before = root->ExportSnapshotJson();
    auto branch = PreviewOperation(*root, turn).successor;
    auto ordinary = root->Predict(turn.action);
    require(branch->decision().kind == DecisionKind::Choice, "typed action did not reach turn-start choice");
    size_t answers = 0;
    while (branch->decision().kind == DecisionKind::Choice) {
        require(branch->PendingOperation() == turn, "typed action lost original request at a choice");
        auto restored = Env::ImportSnapshotJson(branch->ExportSnapshotJson());
        require(restored->PendingOperation() == turn, "snapshot lost typed action request");
        const auto legal = restored->LegalActions();
        const auto action = restored->decision().request.prompt == "count" ? legal.back() : legal.front();
        auto preview = PreviewAction(*restored, action);
        require(preview.continuation_of == turn && preview.choice_replayed,
            "ordinary Answer omitted its typed action continuation");
        auto typed_answer = request(*restored, Operation::Kind::Action);
        typed_answer.action = action;
        auto typed_preview = PreviewOperation(*restored, typed_answer);
        require(typed_preview.operation == typed_answer && typed_preview.continuation_of == turn
            && typed_preview.choice_replayed, "typed Answer replaced the suspended root request");
        require(typed_preview.successor->ExportSnapshotJson() == preview.successor->ExportSnapshotJson(),
            "typed and ordinary Answer produced different continuations");
        require(restored->PredictOperation(typed_answer)->ExportSnapshotJson() == preview.successor->ExportSnapshotJson(),
            "typed Answer execution differed from its preview");
        ordinary = ordinary->Predict(action);
        branch = std::move(preview.successor);
        require(++answers < 16, "typed action choice did not finish");
    }
    require(branch->Hash() == ordinary->Hash() && !branch->PendingOperation(),
        "completed typed action differed from ordinary action or retained a pending request");
    require(root->ExportSnapshotJson() == before, "typed action continuation mutated the source");
}

int main() {
    try { source_binding_and_prefix(); combat_boundaries(); automatic_potion_discard(); choice_after_boundary(); typed_action_choices();
        std::cout << "Typed operation, source identity, native prefix and boundary contracts passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
