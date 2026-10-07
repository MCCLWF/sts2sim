#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "search/combatsolver/env_state.h"
#include "game/All.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/CreatureCmd.h"

using namespace sts2;
using namespace sts2::combatsolver;
namespace {
size_t transitions = 0;
size_t choiceAnswers = 0;
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

// The reference is ordinary Env execution. This tests only the new consumer's
// stopping/continuation contract, and makes no new claim about game-rule fidelity.
void CompareContinuation(CombatSearchState actual, std::unique_ptr<Env> expected, bool lastAnswer = false) {
    size_t answers = 0;
    while (actual.DecisionPoint().kind == DecisionKind::Choice) {
        Require(expected->decision().kind == DecisionKind::Choice, "search exposed a different choice boundary");
        auto legal = actual.LegalActions();
        Require(!legal.empty() && legal == expected->LegalActions(), "search changed choice indices or legality");
        // Capture a typed choice through the same public root path, then branch.
        const auto before = actual.ExportSnapshotJson();
        auto publicClone = actual.CloneEnv();
        auto captured = CombatSearchState::Capture(*publicClone);
        const auto action = lastAnswer ? legal.back() : legal.front();
        actual = captured.Predict(action);
        expected = expected->Predict(action);
        Require(publicClone->ExportSnapshotJson() == before, "choice capture/branch changed its public source");
        Require(++answers < 64, "selection did not finish");
        ++choiceAnswers;
    }
    if (actual.Outcome() != CombatOutcome::Ongoing) {
        Require(actual.LegalActions().empty(), "search offered Resume as a terminal search edge");
        const auto before = actual.ExportSnapshotJson();
        auto settled = actual.SettleOutcome();
        Require(settled->Hash() == expected->Hash(), "search outcome Resume changed vanilla settlement");
        Require(actual.ExportSnapshotJson() == before, "settlement changed the frozen search terminal");
    } else {
        Require(actual.NativeHash() == expected->Hash(), "search continuation differs from ordinary Env");
    }
}

void CompareAction(const Env& source, const Action& action, bool lastAnswer = false) {
    const auto before = source.ExportSnapshotJson();
    auto search = CombatSearchState::Capture(source);
    Require(search.NativeHash() == source.Hash(), "root capture changed native state");
    const auto capturedBefore = search.ExportSnapshotJson();
    auto sibling = search.Fork();
    CompareContinuation(search.Predict(action), source.Predict(action), lastAnswer);
    Require(source.ExportSnapshotJson() == before && search.ExportSnapshotJson() == capturedBefore
        && sibling.ExportSnapshotJson() == capturedBefore, "one branch modified a root or sibling");
    ++transitions;
}

Action FindCardAction(const Env& source, CardId id) {
    auto inspect = source.Clone();
    const auto cards = Player(&inspect->sim(), 1).PlayerCombatState().Hand().Cards();
    for (const auto& action : source.LegalActions())
        if (action.kind == Action::PlayCard && cards[action.index].CardId_() == id) return action;
    throw std::runtime_error("fixture lacks requested legal card");
}

void InitialBranches() {
    for (auto character : {CharacterId::IRONCLAD, CharacterId::SILENT, CharacterId::DEFECT,
                           CharacterId::NECROBINDER, CharacterId::REGENT}) {
        Env source;
        source.ResetCombat({.seed = "CSPORTBRANCH", .character = character}, EncounterId::CULTISTS_NORMAL);
        auto before = source.ExportSnapshotJson();
        for (const auto& action : source.LegalActions()) CompareAction(source, action);
        Require(source.ExportSnapshotJson() == before, "legal/action sweep changed source");
    }
}

void VictoryAndPendingLoss() {
    Env winning;
    winning.ResetCombat({.seed = "APISAFEOUTCOME", .character = CharacterId::IRONCLAD}, EncounterId::CULTISTS_NORMAL);
    Player(&winning.sim(), 1).Creature().st().current_hp = 7;
    const auto enemies = CombatState(&winning.sim()).Enemies();
    for (auto enemy : enemies) enemy.st().current_hp = 1;
    for (size_t i = 1; i < enemies.size(); ++i) CreatureCmd::Kill(enemies[i]);
    const auto hand = Player(&winning.sim(), 1).PlayerCombatState().Hand().Cards();
    const auto legal = winning.LegalActions();
    const auto attack = std::find_if(legal.begin(), legal.end(), [&](auto action) {
        return action.kind == Action::PlayCard && action.target >= 0 && hand[action.index].Type() == CardType::Attack;
    });
    Require(attack != legal.end(), "victory fixture has no attack");
    auto terminal = CombatSearchState::Capture(winning).Predict(*attack);
    Require(terminal.Outcome() == CombatOutcome::Victory, "safe victory was not recognized");
    Require(terminal.Facts().PlayerHp == 7, "victory facts included post-combat relic healing");
    auto settled = terminal.SettleOutcome();
    Require(Player(&settled->sim(), 1).Creature().CurrentHp() > terminal.Facts().PlayerHp,
        "victory fixture did not distinguish pre/post settlement HP");
    CompareAction(winning, *attack);
    try { terminal.Predict({.kind = Action::Resume}); throw std::runtime_error("terminal expansion accepted"); }
    catch (const std::invalid_argument&) { }

    Env losing;
    losing.ResetCombat({.seed = "CSPENDINGLOSS", .character = CharacterId::IRONCLAD}, EncounterId::CULTISTS_NORMAL);
    // This injected original-state field exercises the pending-loss safe point
    // independently of HP: a later heal must not relabel it as a victory.
    losing.state().combat.pending_loss = true;
    Require(Player(&losing.sim(), 1).Creature().CurrentHp() > 0, "pending loss fixture already has zero HP");
    auto defeat = CombatSearchState::Capture(losing).Predict({.kind = Action::EndTurn});
    Require(defeat.Outcome() == CombatOutcome::Defeat && defeat.Facts().PlayerHp > 0,
        "positive HP overrode the pending-loss terminal");
    CompareAction(losing, {.kind = Action::EndTurn});
}

void NestedChoices() {
    Env source;
    source.ResetCombat({.seed = "CSSURVIVOR", .character = CharacterId::SILENT}, EncounterId::CULTISTS_NORMAL);
    auto survivor = CombatState(&source.sim()).CreateCard(CardId::SURVIVOR, Player(&source.sim(), 1));
    CardPileCmd::Add(survivor, PileType::Hand);
    const auto action = FindCardAction(source, CardId::SURVIVOR);
    CompareAction(source, action, false);
    CompareAction(source, action, true);

    Env toasty;
    toasty.ResetCombat({.seed = "APIPAUSECHOICE", .character = CharacterId::IRONCLAD}, EncounterId::CULTISTS_NORMAL);
    Operation relic;
    relic.kind = Operation::Kind::ObtainRelic; relic.source_hash = toasty.Hash();
    relic.relic_id = static_cast<uint16_t>(RelicId::TOASTY_MITTENS);
    auto root = toasty.PredictOperation(relic);
    CompareAction(*root, {.kind = Action::EndTurn}, false);
    CompareAction(*root, {.kind = Action::EndTurn}, true);

    auto search = CombatSearchState::Capture(source);
    const auto before = search.ExportSnapshotJson();
    try { search.Predict({.kind = Action::PlayCard, .index = INT32_MAX}); throw std::runtime_error("illegal branch accepted"); }
    catch (const InvalidRequest&) { }
    Require(search.ExportSnapshotJson() == before, "rejected branch modified source");
}
}  // namespace

int main() {
    try {
        InitialBranches(); VictoryAndPendingLoss(); NestedChoices();
        Require(choiceAnswers > 0, "no continuation choices were tested");
        std::cout << "CombatSolver Env adapter: " << transitions << " actions, " << choiceAnswers
            << " choice answers; independent roots and vanilla settlement matched\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
