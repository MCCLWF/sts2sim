#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include "env/combat_prediction.h"
#include "env/snapshot_json.h"
#include "game/All.h"
#include "game/Combat/CombatManager.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/PotionCmd.h"
#include "game/Commands/PowerCmd.h"
#include "game/Commands/RelicCmd.h"

using namespace sts2;

static void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}
template<class F> static void rejects(F&& operation, const char* reason) {
    try { operation(); }
    catch (const GameError&) { return; }
    throw std::runtime_error(reason);
}
template<class F> static void rejects_with(F&& operation, std::string_view diagnostic, const char* reason) {
    try { operation(); }
    catch (const GameError& error) {
        if (std::string_view(error.what()).find(diagnostic) != std::string_view::npos) return;
        throw;
    }
    throw std::runtime_error(reason);
}
static void reset(Env& env) {
    env.ResetCombat(NewRunOptions{.seed = "DUAL001", .character = CharacterId::IRONCLAD},
        EncounterId::CULTISTS_NORMAL);
}
static int add_card(Env& env, CardId id) {
    CardModel card = CombatState(&env.sim()).CreateCard(id, Player(&env.sim(), 1));
    CardPileCmd::Add(card, PileType::Hand);
    return Player(&env.sim(), 1).PlayerCombatState().Hand().Count() - 1;
}
static const GameState& state(const CombatPredictionSession& session) {
    return *session.environment().sim().g;
}
static std::string replace(std::string text, std::string_view from, std::string_view to) {
    const auto pos = text.find(from);
    require(pos != std::string::npos, "malformed fixture field missing");
    text.replace(pos, from.size(), to);
    return text;
}

int main(int argc, char** argv) {
    const std::filesystem::path fixture_dir = argc > 1 ? argv[1] : "";
    auto write_fixture = [&](const char* name, const std::string& snapshot) {
        if (fixture_dir.empty()) return;
        std::filesystem::create_directories(fixture_dir);
        std::ofstream file(fixture_dir / name);
        file << snapshot;
        require(file.good(), "failed to write prediction fixture");
    };
    Env env;
    reset(env);
    // Exact ordinary v2 state remains the inner state until a terminal boundary.
    auto root = CombatPredictionSession::FromGameSnapshot(env.ExportSnapshotJson());
    const auto root_hash = root->Hash();
    const auto root_snapshot = root->ExportSnapshotJson();
    write_fixture("root.json", env.ExportSnapshotJson());
    write_fixture("active.json", root_snapshot);
    if (!fixture_dir.empty()) {
        auto unported = env.Clone();
        const CardRef ref = unported->state().player_combat.piles[0].front();
        unported->state().cards[ref].id = CardId::SPLASH;
        unported->state().cards[ref].var_overrides.clear();
        write_fixture("unported_root.json", unported->ExportSnapshotJson());
    }
    require(root->environment().Hash() == env.Hash(), "opening changed game state");
    require(root->Hash() != env.Hash(), "prediction hash lacks its domain");
    {
        // CardModel.OnPlayWrapper records entering each actual OnPlay call.
        // The trace holds game facts; its consumer decides search risks.
        auto game = env.Clone();
        const int hand_index = add_card(*game, CardId::RAMPAGE);
        auto effect_root = CombatPredictionSession::FromGameSnapshot(game->ExportSnapshotJson());
        const std::string source = effect_root->ExportSnapshotJson();
        const Action action{.kind = Action::PlayCard, .index = hand_index, .target = 1};
        auto played = effect_root->Predict(action);
        require(played->environment().Hash() == game->Predict(action)->Hash()
            && effect_root->ExportSnapshotJson() == source, "effect facts changed gameplay/source");
        const std::string snapshot = played->ExportSnapshotJson();
        const auto document = snapshot_detail::Parser(snapshot).parse();
        const auto& events = document.at("history").at("events").array;
        require(events.size() == 5 && events[0].array[0].integer<int>() == 0
            && events[1].array[0].integer<int>() == 9 && events[1].array.size() == 20
            && events[1].array[1].integer<int>() == events[0].array[1].integer<int>()
            && events[1].array[18].array.size() == 10
            && events[2].array[0].integer<int>() == 2 && events[3].array[0].integer<int>() == 3
            && events[4].array[0].integer<int>() == 1, "Rampage execution fact order/identity differs");
        const size_t begin = snapshot.find("[9,", snapshot.find("\"history\":{\"events\":"));
        require(begin != std::string::npos, "effect facts not serialized");
        size_t end = begin;
        int depth = 0;
        do {
            if (snapshot[end] == '[') ++depth;
            if (snapshot[end] == ']') --depth;
            ++end;
        } while (depth != 0 && end < snapshot.size());
        const std::string effect_event = snapshot.substr(begin, end - begin);
        require(CombatPredictionSession::ImportSnapshotJson(snapshot)->ExportSnapshotJson() == snapshot
            && played->Clone()->ExportSnapshotJson() == snapshot, "effect facts clone/restore differs");
        rejects_with([&] { CombatPredictionSession::ImportSnapshotJson(replace(snapshot, effect_event,
            "[10" + effect_event.substr(2))); }, "unknown history event", "unknown effect fact accepted");
        const size_t target_type = effect_event.rfind(",2]");
        require(target_type != std::string::npos, "Rampage target type is not AnyEnemy");
        rejects_with([&] { CombatPredictionSession::ImportSnapshotJson(replace(snapshot, effect_event,
            effect_event.substr(0, target_type) + ",1]" + effect_event.substr(target_type + 3))); },
            "hash mismatch", "target type omitted from hash");
        rejects_with([&] { CombatPredictionSession::ImportSnapshotJson(replace(snapshot, effect_event,
            "[9,0,0,0,0,0,0,0,0,-1,0,false,0,false,false,false,[],null,null,null,1]")); },
            "invalid history event", "legacy risk event accepted as a game fact");
        write_fixture("card_effect.json", snapshot);
    }
    {
        auto unknown_formula = env.Clone();
        add_card(*unknown_formula, CardId::PRECISE_CUT);
        const CardModel unknown = Player(&unknown_formula->sim(), 1).PlayerCombatState().Hand().Cards().back();
        const auto before = unknown_formula->ExportSnapshotJson();
        bool unsupported = false;
        try { unknown.DynamicVars().CalculatedDamage().Calculate(Creature{}); }
        catch (const Unported& error) { unsupported = std::string_view(error.what()).find("formula not ported") != std::string_view::npos; }
        require(unsupported && unknown_formula->ExportSnapshotJson() == before
            && root->ExportSnapshotJson() == root_snapshot, "unknown calculated formula accepted or changed source/sibling");
    }
    require(root->Clone()->ExportSnapshotJson() == root_snapshot, "clone differs");
    require(CombatPredictionSession::ImportSnapshotJson(root_snapshot)->ExportSnapshotJson() == root_snapshot,
        "active envelope restore differs");
    rejects_with([&] { CombatPredictionSession::ImportSnapshotJson(replace(root_snapshot,
        "\"power_events\":[]", "\"power_events\":[[3,1,1,1,1,0,1,false]]")); },
        "unknown power event", "unknown power mutation accepted");
    rejects_with([&] { CombatPredictionSession::ImportSnapshotJson(replace(root_snapshot,
        "\"power_events\":[]", "\"power_events\":[[2,1,1,1,1,1,0,true]]")); },
        "inconsistent power event", "removal with changed amount accepted");
    rejects_with([&] { CombatPredictionSession::ImportSnapshotJson(replace(root_snapshot,
        "\"counters\":[0,0,0,0,0,0]", "\"counters\":[1,0,0,0,0,0]")); },
        "history counters mismatch", "forged history counters accepted");
    auto next = root->Predict(Action{.kind = Action::EndTurn});
    require(!next->terminal(), "first cultist ritual unexpectedly terminal");
    require(next->environment().Hash() == env.Predict(Action{.kind = Action::EndTurn})->Hash(),
        "ordinary nonterminal transition changed");
    require(root->Hash() == root_hash && root->ExportSnapshotJson() == root_snapshot, "predict changed source");
    require(root->environment().sim().combat_prediction_boundary == nullptr
        && next->environment().sim().combat_prediction_boundary == nullptr, "scope escaped execution");
    rejects([&] { root->Predict(Action{.kind = Action::Proceed}); }, "noncombat action accepted");
    rejects([&] { root->Predict(Action{.kind = Action::PlayCard, .index = 999}); }, "illegal action accepted");
    require(root->Hash() == root_hash, "failed prediction changed source");
    rejects_with([&] { CombatPredictionSession::ImportSnapshotJson(replace(root_snapshot,
        "\"machine\":{\"exists\":true", "\"machine\":{\"exists\":false")); },
        "inactive monster machine attached to combat", "active monster accepted an absent machine");

    // Source-backed expectations: BurningBlood.cs heals 6 only after victory;
    // vanilla CombatManager.EndCombatInternal clears player block/piles. The
    // fixed CombatSolver prediction boundary deliberately stops before them.
    reset(env);
    Player player(&env.sim(), 1);
    player.Creature().st().current_hp = 40;
    player.Creature().st().block = 7;
    for (Creature enemy : CombatState(&env.sim()).Enemies()) enemy.st().current_hp = 1;
    const Action lethal{.kind = Action::PlayCard, .index = add_card(env, CardId::THUNDERCLAP)};
    auto lethal_root = CombatPredictionSession::FromGameSnapshot(env.ExportSnapshotJson());
    const auto lethal_source_hash = lethal_root->Hash();
    auto victory = lethal_root->Predict(lethal);
    require(victory->terminal() == std::optional(CombatPredictionTerminalStamp{1, CombatPredictionOutcome::Victory}),
        "victory stamp differs");
    const auto victory_history = snapshot_detail::Parser(victory->ExportSnapshotJson()).parse().at("history");
    const auto& victory_events = victory_history.at("events").array;
    require(victory_events.size() == 6
        && victory_events[0].array[0].integer<int>() == 0
        && victory_events[1].array[0].integer<int>() == 9
        && victory_events[2].array[0].integer<int>() == 2
        && victory_events[3].array[0].integer<int>() == 2
        && victory_events[4].array[0].integer<int>() == 3
        && victory_events[5].array[0].integer<int>() == 1,
        "attack facts differ from Started/EffectStarted/Damage/Damage/Attacked/Finished");
    require(victory_history.at("counters").array[0].integer<int>() == 1,
        "finished play was not counted from its history entry");
    require(victory_events[2].array[19].array[0].integer<int>() == 1
        && victory_events[2].array[19].array[1].text == "THUNDERCLAP"
        && victory_events[3].array[19].array[0].integer<int>() == 1,
        "prediction damage lost its card source");
    require(victory_events[4].array[10].integer<int>() == 2
        && victory_events[4].array[16].array.size() == 2
        && victory_events[4].array[16].array[0].array[3].integer<int>() == 1
        && victory_events[4].array[16].array[1].array[3].integer<int>() == 1,
        "attack history lost its ordered hit results");
    Player projected(&victory->environment().sim(), 1);
    require(projected.Creature().CurrentHp() == 40 && projected.Creature().Block() == 7,
        "prediction ran after-combat hooks or cleanup");
    require(state(*victory).run.phase == RunPhase::Combat && !state(*victory).combat.is_in_progress
        && !state(*victory).combat.pending_loss, "terminal inner contract differs");
    require(victory->LegalActions().empty(), "terminal has legal actions");
    rejects([&] { victory->Predict(Action{.kind = Action::EndTurn}); }, "terminal advanced");
    const auto victory_snapshot = victory->ExportSnapshotJson();
    rejects_with([&] { CombatPredictionSession::ImportSnapshotJson(replace(victory_snapshot,
        "[1,\"THUNDERCLAP\"]", "[9,\"THUNDERCLAP\"]")); },
        "invalid damage source kind", "invalid damage source kind accepted");
    rejects_with([&] { CombatPredictionSession::ImportSnapshotJson(replace(victory_snapshot,
        "[1,\"THUNDERCLAP\"]", "[1,\"STRIKE_IRONCLAD\"]")); },
        "hash mismatch", "changed damage source omitted from prediction hash");
    write_fixture("victory.json", victory_snapshot);
    auto restored_victory = CombatPredictionSession::ImportSnapshotJson(victory_snapshot);
    require(restored_victory->ExportSnapshotJson() == victory_snapshot, "terminal restore differs");
    rejects_with([&] { CombatPredictionSession::ImportSnapshotJson(replace(victory_snapshot,
        "\"definition\":1", "\"definition\":65535")); },
        "monster machine registry binding", "removed monster accepted an unknown retained layout");
    rejects_with([&] { CombatPredictionSession::ImportSnapshotJson(replace(victory_snapshot,
        "\"next_move\":{\"state\":0,\"stunned\":false,\"stun_move\":0",
        "\"next_move\":{\"state\":0,\"stunned\":false,\"stun_move\":65535")); },
        "unknown stun callback", "removed monster accepted an unknown retained callback");
    rejects_with([&] { CombatPredictionSession::ImportSnapshotJson(replace(victory_snapshot,
        "\"next_move\":{\"state\":0", "\"next_move\":{\"state\":99")); },
        "monster state index", "removed monster accepted an invalid retained move");
    rejects([&] { Env::ImportSnapshotJson(victory->environment().ExportSnapshotJson()); },
        "ordinary Env imported an inactive prediction");
    rejects([&] { CombatPredictionSession::FromGameSnapshot(victory->environment().ExportSnapshotJson()); },
        "opening accepted an inactive snapshot without metadata");
    rejects([&] { CombatPredictionSession::ImportSnapshotJson(replace(victory_snapshot,
        "\"player_turn\":1", "\"player_turn\":2")); }, "terminal turn mismatch accepted");
    rejects([&] { CombatPredictionSession::ImportSnapshotJson(replace(victory_snapshot,
        "\"replay_terminal_base\":null", "\"replay_terminal_base\":{\"terminal\":null}")); },
        "terminal replay base accepted");
    auto ordinary_victory = env.Predict(lethal);
    require(Player(&ordinary_victory->sim(), 1).Creature().CurrentHp() == 46,
        "ordinary Burning Blood victory behavior changed");
    require(Player(&ordinary_victory->sim(), 1).Creature().Block() == 0, "ordinary cleanup changed");
    require(ordinary_victory->state().run.phase != RunPhase::Combat, "ordinary reward transition changed");
    require(lethal_root->Hash() == lethal_source_hash, "terminal transition changed its source");

    // Pending loss locks the outcome, including when HP has already recovered.
    // CheckWinCondition in the decompiled game handles PendingLoss before victory.
    reset(env);
    env.state().combat.pending_loss = true;
    auto locked = CombatPredictionSession::FromGameSnapshot(env.ExportSnapshotJson());
    auto defeat = locked->Predict(Action{.kind = Action::EndTurn});
    require(defeat->terminal()->outcome == CombatPredictionOutcome::Defeat
        && defeat->terminal()->player_turn == 1, "pending loss did not lock defeat");
    require(Player(&defeat->environment().sim(), 1).Creature().CurrentHp() > 0,
        "recovered-HP fixture lost HP");
    require(state(*defeat).run.phase == RunPhase::Combat && !state(*defeat).combat.pending_loss,
        "prediction loss became a run end");
    require(CombatPredictionSession::ImportSnapshotJson(defeat->ExportSnapshotJson())->Hash() == defeat->Hash(),
        "defeat restore differs");
    write_fixture("defeat.json", defeat->ExportSnapshotJson());

    // Enemy-turn lethal uses the actual player TurnNumber, not round_number
    // or a wrapper counter, and returns through the existing turn safe point.
    reset(env);
    env.Step(Action{.kind = Action::EndTurn});
    Player(&env.sim(), 1).Creature().st().current_hp = 1;
    auto enemy_root = CombatPredictionSession::FromGameSnapshot(env.ExportSnapshotJson());
    auto enemy_defeat = enemy_root->Predict(Action{.kind = Action::EndTurn});
    require(enemy_defeat->terminal() == std::optional(CombatPredictionTerminalStamp{2, CombatPredictionOutcome::Defeat}),
        "enemy lethal stamped the wrong turn");
    require(state(*enemy_defeat).combat.current_side == CombatSide::Enemy, "enemy terminal side was rewritten");
    require(CombatPredictionSession::ImportSnapshotJson(enemy_defeat->ExportSnapshotJson())->Hash() == enemy_defeat->Hash(),
        "enemy terminal side could not restore");

    // Demise's game AfterSideTurnEnd damage ends combat after enemy moves.
    // CombatSolver returns at that safe point before preparing another round.
    reset(env);
    PlayerChoiceContext power_context;
    for (Creature enemy : CombatState(&env.sim()).Enemies()) {
        enemy.st().current_hp = 1;
        PowerCmd::Apply(PowerId::DEMISE_POWER, power_context, enemy, Dec(1),
            Player(&env.sim(), 1).Creature(), CardModel{});
    }
    auto enemy_end_root = CombatPredictionSession::FromGameSnapshot(env.ExportSnapshotJson());
    auto enemy_end_victory = enemy_end_root->Predict(Action{.kind = Action::EndTurn});
    require(enemy_end_victory->terminal() == std::optional(CombatPredictionTerminalStamp{1, CombatPredictionOutcome::Victory}),
        "enemy end lethal stamped the wrong turn");
    require(state(*enemy_end_victory).combat.current_side == CombatSide::Enemy
        && state(*enemy_end_victory).player_combat.turn_number == 1
        && state(*enemy_end_victory).combat.round_number == 1, "terminal enemy end advanced the next round");
    require(CombatPredictionSession::ImportSnapshotJson(enemy_end_victory->ExportSnapshotJson())->Hash()
        == enemy_end_victory->Hash(), "enemy end terminal could not restore");

    // A potion also reaches the vanilla ActionExecutor post-action safe point.
    reset(env);
    for (Creature enemy : CombatState(&env.sim()).Enemies()) enemy.st().current_hp = 1;
    require(PotionCmd::TryToProcure(PotionId::EXPLOSIVE_AMPOULE, Player(&env.sim(), 1)).success, "potion fixture failed");
    auto potion_root = CombatPredictionSession::FromGameSnapshot(env.ExportSnapshotJson());
    const auto potion_actions = potion_root->LegalActions();
    const auto potion = std::find_if(potion_actions.begin(), potion_actions.end(),
        [](const Action& action) { return action.kind == Action::UsePotion; });
    require(potion != potion_actions.end(), "potion action missing");
    require(potion_root->Predict(*potion)->terminal()->outcome == CombatPredictionOutcome::Victory,
        "potion missed its terminal safe point");

    // The scope metadata and stationary game base both survive choice cloning
    // and import. Answer branches own their scopes and leave siblings unchanged.
    reset(env);
    const Action discovery{.kind = Action::PlayCard, .index = add_card(env, CardId::DISCOVERY)};
    require(PotionCmd::TryToProcure(PotionId::FLEX_POTION, Player(&env.sim(), 1)).success,
        "choice power facts fixture failed");
    auto before_potion = CombatPredictionSession::FromGameSnapshot(env.ExportSnapshotJson());
    const auto power_actions = before_potion->LegalActions();
    const auto power_action = std::find_if(power_actions.begin(), power_actions.end(),
        [](const Action& action) { return action.kind == Action::UsePotion; });
    require(power_action != power_actions.end(), "choice power potion action missing");
    auto choice_root = before_potion->Predict(*power_action);
    const auto root_facts = snapshot_detail::Parser(choice_root->ExportSnapshotJson()).parse().at("power_events").array;
    require(!root_facts.empty(), "power potion did not emit facts");
    auto choice = choice_root->Predict(discovery);
    require(choice->environment().decision().kind == DecisionKind::Choice, "Discovery did not ask a choice");
    const auto choice_snapshot = choice->ExportSnapshotJson();
    write_fixture("choice.json", choice_snapshot);
    const auto envelope = snapshot_detail::Parser(choice_snapshot).parse();
    require(envelope.at("replay_terminal_base").at("terminal").is_null(), "choice boundary base missing");
    require(envelope.at("power_events").array.size() == root_facts.size()
        && envelope.at("replay_terminal_base").at("power_events").array.size() == root_facts.size(),
        "choice lost prior power facts or its replay base");
    require(envelope.at("history").at("events").array.size() == 2
        && envelope.at("replay_terminal_base").at("history").at("events").array.empty(),
        "choice history did not preserve partial action and clean action base");
    auto choice_clone = choice->Clone();
    auto restored_choice = CombatPredictionSession::ImportSnapshotJson(choice_snapshot);
    rejects_with([&] { CombatPredictionSession::FromGameSnapshot(choice->environment().ExportSnapshotJson()); },
        "pending game choice has no policy history action base", "history-free choice import accepted");
    const auto answer = choice->LegalActions().front();
    auto resolved = choice->Predict(answer);
    require(snapshot_detail::Parser(resolved->ExportSnapshotJson()).parse().at("power_events").array.size() == root_facts.size(),
        "answer replay duplicated or lost prior power facts");
    write_fixture("resolved.json", resolved->ExportSnapshotJson());
    require(restored_choice->Predict(answer)->Hash() == resolved->Hash()
        && choice_clone->Predict(answer)->Hash() == resolved->Hash(), "choice rollback diverged");
    const auto resolved_history = snapshot_detail::Parser(resolved->ExportSnapshotJson()).parse().at("history");
    require(resolved_history.at("events").array.size() == 3
        && resolved_history.at("counters").array[0].integer<int>() == 1,
        "answer replay duplicated or lost policy events");
    require(choice->ExportSnapshotJson() == choice_snapshot, "choice answer changed source");
    require(resolved->environment().sim().combat_prediction_boundary == nullptr, "answer retained a scope pointer");
    require(CombatPredictionSession::ImportSnapshotJson(resolved->ExportSnapshotJson())->Hash() == resolved->Hash(),
        "completed answer prefix did not restore");
    rejects([&] { CombatPredictionSession::ImportSnapshotJson(replace(choice_snapshot,
        "\"replay_terminal_base\":{\"terminal\":null,\"history\":", "\"replay_terminal_base\":null,\"history\":")); },
        "missing boundary replay metadata accepted");
    rejects([&] { CombatPredictionSession::ImportSnapshotJson(replace(root_snapshot,
        "\"version\":7", "\"version\":8")); }, "unknown envelope version accepted");
    rejects([&] { CombatPredictionSession::ImportSnapshotJson(replace(root_snapshot,
        "\"version\":7", "\"version\":6")); }, "old envelope version accepted");
    rejects([&] { CombatPredictionSession::ImportSnapshotJson(replace(root_snapshot,
        "\"terminal\":null", "\"terminal\":{\"player_turn\":1,\"outcome\":\"unknown\"}")); },
        "unknown outcome accepted");

    // BurningPact's chosen exhaust invokes CharonsAshes' game damage hook.
    // Answer must replay under its owned prediction scope through the lethal
    // effect; ordinary replay would run BurningBlood and the reward transition.
    reset(env);
    Player(&env.sim(), 1).Creature().st().current_hp = 40;
    RelicCmd::Obtain(RelicId::CHARONS_ASHES, Player(&env.sim(), 1));
    for (Creature enemy : CombatState(&env.sim()).Enemies()) enemy.st().current_hp = 1;
    const Action pact{.kind = Action::PlayCard, .index = add_card(env, CardId::BURNING_PACT)};
    // The card is part of the frozen root, just as any other live hand card.
    auto exhaust_root = CombatPredictionSession::FromGameSnapshot(env.ExportSnapshotJson());
    auto exhaust_choice = exhaust_root->Predict(pact);
    require(exhaust_choice->environment().decision().kind == DecisionKind::Choice,
        "BurningPact did not ask a choice");
    const auto exhaust_snapshot = exhaust_choice->ExportSnapshotJson();
    write_fixture("choice_lethal.json", exhaust_snapshot);
    auto exhaust_restored = CombatPredictionSession::ImportSnapshotJson(exhaust_snapshot);
    auto exhaust_clone = exhaust_choice->Clone();
    const auto exhaust_answer = exhaust_choice->LegalActions().front();
    auto exhaust_selected = exhaust_choice->Predict(exhaust_answer);
    require(exhaust_selected->environment().decision().kind == DecisionKind::Choice
        && exhaust_selected->environment().decision().request.prompt == "index",
        "BurningPact did not request its chosen index");
    const auto exhaust_index = exhaust_selected->LegalActions().front();
    const auto selected_snapshot = exhaust_selected->ExportSnapshotJson();
    write_fixture("choice_lethal_selected.json", selected_snapshot);
    auto selected_restored = CombatPredictionSession::ImportSnapshotJson(selected_snapshot);
    auto exhaust_victory = exhaust_selected->Predict(exhaust_index);
    require(exhaust_victory->terminal() == std::optional(CombatPredictionTerminalStamp{1, CombatPredictionOutcome::Victory})
        && Player(&exhaust_victory->environment().sim(), 1).Creature().CurrentHp() == 40
        && state(*exhaust_victory).run.phase == RunPhase::Combat, "choice lethal escaped its prediction scope");
    require(exhaust_restored->Predict(exhaust_answer)->Predict(exhaust_index)->Hash() == exhaust_victory->Hash()
        && exhaust_clone->Predict(exhaust_answer)->Predict(exhaust_index)->Hash() == exhaust_victory->Hash()
        && selected_restored->Predict(exhaust_index)->Hash() == exhaust_victory->Hash(),
        "restored lethal choice replay diverged");
    require(exhaust_choice->ExportSnapshotJson() == exhaust_snapshot
        && exhaust_selected->ExportSnapshotJson() == selected_snapshot, "lethal answer changed its choice source");
    require(CombatPredictionSession::ImportSnapshotJson(exhaust_victory->ExportSnapshotJson())->Hash() == exhaust_victory->Hash(),
        "lethal choice terminal could not restore");
    write_fixture("choice_victory.json", exhaust_victory->ExportSnapshotJson());

    std::cout << "Combat prediction checks passed: terminal scope, ordinary isolation, turn stamps, potion safe point, owned branches and choice replay.\n";
}
