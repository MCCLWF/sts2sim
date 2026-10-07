#include <iostream>
#include <stdexcept>

#include "engine/run_setup.h"
#include "engine/sim/state_hash.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Combat/CombatManager.h"
#include "game/Runs/RunManager.h"

using namespace sts2;

namespace {

struct Fixture {
    GameState state;
    Sim sim;
    Player player;
    Creature aeonglass;

    Fixture() : sim(state), player(&sim, 1) {
        new_run(state, NewRunOptions{.seed = "GENERATEDCARDADD", .character = CharacterId::IRONCLAD, .ascension = 0});
        RunManager::SetActInternal(&sim, 2);
        RunManager::EnterMapCoordDebug(&sim, MapCoord{0, 0}, EncounterId::AEONGLASS_BOSS);
        aeonglass = CombatState(&sim).Enemies().front();
        // Aeonglass upgrades every generated Wither once from this point on.
        state.creatures[aeonglass.Index()].monster.extra[1] = 1;
    }

    CardModel new_wither() { return CombatState(&sim).CreateCard(CardId::WITHER, player); }
};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void require_generation_trace(const ExecutionTrace& trace) {
    require(trace.events.size() == 2, "generated card should produce exactly two history events");
    require(trace.events[0].kind == ExecutionEventKind::CardGenerated,
            "generated card history is missing CardGenerated");
    require(trace.events[1].kind == ExecutionEventKind::CardGenerationResolved,
            "generated card history is missing CardGenerationResolved");
}

void test_nonprogressing_single_wrapper_throws_without_effects() {
    Fixture f;
    f.state.combat.is_in_progress = false;
    CardModel card = f.new_wither();
    require(card.st().extra[0] == 0, "fixture Wither was unexpectedly upgraded");

    const uint64_t before = hash_state(f.state);
    ExecutionTrace trace;
    ExecutionTraceScope trace_scope(f.sim, &trace);
    const CardPileAddResults batch = CardPileCmd::AddGeneratedCardsToCombat(
        CardList{card}, PileType::Hand, f.player);
    require(batch.empty(), "batch command should return no result outside an active combat");
    require(hash_state(f.state) == before, "empty batch changed gameplay state or RNG");
    require(trace.events.empty(), "empty batch recorded generated-card history");

    bool threw = false;
    try {
        (void)CardPileCmd::AddGeneratedCardToCombat(card, PileType::Hand, f.player);
    } catch (const GameError&) {
        threw = true;
    }
    require(threw, "single-card wrapper must fail when the batch result is empty");
    require(hash_state(f.state) == before, "failed single-card wrapper changed gameplay state or RNG");
    require(trace.events.empty(), "failed single-card wrapper recorded generation or resolution");
    require(card.st().extra[0] == 0, "failed single-card wrapper ran Aeonglass's generation hook");
    require(!card.Pile(), "failed single-card wrapper placed the card in a pile");
}

void test_active_combat_single_and_batch_semantics() {
    Fixture single;
    Fixture batch;
    CardModel single_card = single.new_wither();
    CardModel batch_card = batch.new_wither();
    ExecutionTrace single_trace;
    ExecutionTrace batch_trace;
    CardPileAddResult single_result;
    CardPileAddResults batch_results;
    {
        ExecutionTraceScope trace_scope(single.sim, &single_trace);
        single_result = CardPileCmd::AddGeneratedCardToCombat(single_card, PileType::Hand, single.player);
    }
    {
        ExecutionTraceScope trace_scope(batch.sim, &batch_trace);
        batch_results = CardPileCmd::AddGeneratedCardsToCombat(
            CardList{batch_card}, PileType::Hand, batch.player);
    }

    require(batch_results.size() == 1, "active-combat batch should return one result");
    const CardPileAddResult& batch_result = batch_results[0];
    require(single_result.success && batch_result.success, "generated card was not successfully added");
    require(single_result.cardAdded.CardId_() == CardId::WITHER &&
            batch_result.cardAdded.CardId_() == CardId::WITHER,
            "generation result does not identify the generated Wither");
    require(single_result.oldPile == batch_result.oldPile &&
            single_result.targetPile == batch_result.targetPile &&
            single_result.targetPile == PileType::Hand,
            "single and batch generation returned different pile semantics");
    require(single_card.Pile().Type() == PileType::Hand && batch_card.Pile().Type() == PileType::Hand,
            "generated card was not placed in the hand");
    require(single_card.st().extra[0] == 1 && batch_card.st().extra[0] == 1,
            "active generation did not run Aeonglass's generated-card hook exactly once");
    require_generation_trace(single_trace);
    require_generation_trace(batch_trace);
    require(hash_state(single.state) == hash_state(batch.state),
            "single-card and one-card batch generation produced different gameplay states");
}

}  // namespace

int main() {
    try {
        test_nonprogressing_single_wrapper_throws_without_effects();
        test_active_combat_single_and_batch_semantics();
        std::cout << "generated card add regression: passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "generated card add regression: " << e.what() << '\n';
        return 1;
    }
}
