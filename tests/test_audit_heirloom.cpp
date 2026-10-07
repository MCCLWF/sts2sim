#include <algorithm>
#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>

#include "env/env.h"
#include "game/All.h"
#include "game/Commands/CardPileCmd.h"

using namespace sts2;

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct HammerFixture {
    Env env;
    Player player;
    PlayerCombatState pcs;
    Creature enemy;
    CardModel sovereignBlade;
    CardModel discovery;
    CardModel clash;
    CardModel hammer;

    HammerFixture(bool includeDiscovery, int32_t repeatCount)
        : player(&env.sim(), 1) {
        env.ResetCombat(NewRunOptions{.seed = "AUDITHEIRLOOM", .character = CharacterId::REGENT},
                        EncounterId::CULTISTS_NORMAL);
        pcs = player.PlayerCombatState();
        pcs.Hand().Clear();
        pcs.SetEnergy(99);
        CombatState combat(&env.sim());
        enemy = combat.Enemies().front();
        sovereignBlade = combat.CreateCard(CardId::SOVEREIGN_BLADE, player);
        CardPileCmd::Add(sovereignBlade, PileType::Hand);
        if (includeDiscovery) {
            discovery = combat.CreateCard(CardId::DISCOVERY, player);
            CardPileCmd::Add(discovery, PileType::Hand);
        }
        clash = combat.CreateCard(CardId::CLASH, player);
        CardPileCmd::Add(clash, PileType::Hand);
        hammer = combat.CreateCard(CardId::HEIRLOOM_HAMMER, player);
        hammer.DynamicVars().Repeat().SetBaseValue(Dec(repeatCount));
        CardPileCmd::Add(hammer, PileType::Hand);
    }

    Action playHammer() const {
        const int32_t handIndex = pcs.Hand().Cards().index_of(hammer);
        require(handIndex >= 0, "Heirloom Hammer is missing from the fixture hand");
        const auto actions = env.LegalActions();
        auto it = std::find_if(actions.begin(), actions.end(), [&](const Action& action) {
            return action.kind == Action::PlayCard && action.index == handIndex && action.target == enemy.CombatId();
        });
        require(it != actions.end(), "Heirloom Hammer is not a legal targeted play");
        return *it;
    }
};

void require_clone_count(Env& env, CardId originalId, int32_t expected) {
    Player player(&env.sim(), 1);
    int32_t count = 0;
    for (CardModel card : player.PlayerCombatState().Hand().Cards()) {
        CardModel source = card.CloneOf();
        if (source && source.CardId_() == originalId && card.CardId_() == originalId) ++count;
    }
    require(count == expected, "Heirloom Hammer produced the wrong number of clones of the chosen card");
}

std::unique_ptr<Env> choose_from_hammer(HammerFixture& fixture, int32_t choiceIndex, int32_t expectedOptions) {
    auto countChoice = fixture.env.Predict(fixture.playHammer());
    require(countChoice->decision().kind == DecisionKind::Choice,
            "two visual-colorless hand cards must require the source card-selection choice");
    const ChoiceRequest& countRequest = countChoice->decision().request;
    require(countRequest.source == "FromHand" && countRequest.prompt == "count" &&
                countRequest.option_count == expectedOptions && countRequest.min_select == 1 &&
                countRequest.max_select == 1 && !countRequest.can_skip,
            "Heirloom Hammer exposed the wrong FromHand selection contract");

    const auto countAnswers = countChoice->LegalActions();
    require(countAnswers.size() == 1, "fixed one-card selection must offer exactly the count-one answer");
    auto indexChoice = countChoice->Predict(countAnswers.front());
    require(indexChoice->decision().kind == DecisionKind::Choice &&
                indexChoice->decision().request.prompt == "index" &&
                indexChoice->decision().request.option_count == expectedOptions,
            "Heirloom Hammer did not expose the candidate-index selection after the count answer");
    const auto indexAnswers = indexChoice->LegalActions();
    require(static_cast<int32_t>(indexAnswers.size()) == expectedOptions && choiceIndex >= 0 &&
                choiceIndex < static_cast<int32_t>(indexAnswers.size()),
            "Heirloom Hammer exposed the wrong candidate count");
    return indexChoice->Predict(indexAnswers[static_cast<size_t>(choiceIndex)]);
}

void test_visual_card_pool_matches_all_v0110_overrides() {
    Env env;
    env.ResetCombat(NewRunOptions{.seed = "AUDITVISUALPOOL", .character = CharacterId::REGENT},
                    EncounterId::CULTISTS_NORMAL);
    Player player(&env.sim(), 1);
    CombatState combat(&env.sim());
    const struct OverrideExpectation { CardId card; CardPoolId visualPool; } cases[] = {
        {CardId::CALTROPS, CardPoolId::SILENT_CARD_POOL},
        {CardId::DISTRACTION, CardPoolId::SILENT_CARD_POOL},
        {CardId::OUTMANEUVER, CardPoolId::SILENT_CARD_POOL},
        {CardId::CLASH, CardPoolId::IRONCLAD_CARD_POOL},
        {CardId::DUAL_WIELD, CardPoolId::IRONCLAD_CARD_POOL},
        {CardId::ENTRENCH, CardPoolId::IRONCLAD_CARD_POOL},
        {CardId::HELLO_WORLD, CardPoolId::DEFECT_CARD_POOL},
        {CardId::REBOUND, CardPoolId::DEFECT_CARD_POOL},
        {CardId::RIP_AND_TEAR, CardPoolId::DEFECT_CARD_POOL},
        {CardId::STACK, CardPoolId::DEFECT_CARD_POOL},
    };
    for (const OverrideExpectation& expected : cases) {
        CardModel card = combat.CreateCard(expected.card, player);
        require(card.VisualCardPool() == expected.visualPool,
                "a CardModel VisualCardPool override differs from the v0.111.0 source");
    }

    CardModel blade = combat.CreateCard(CardId::SOVEREIGN_BLADE, player);
    CardModel discovery = combat.CreateCard(CardId::DISCOVERY, player);
    CardModel eventCard = combat.CreateCard(CardId::ABUNDANCE, player);
    CardModel clash = combat.CreateCard(CardId::CLASH, player);
    require(blade.Pool() == CardPoolId::TOKEN_CARD_POOL && blade.VisualCardPoolIsColorless() &&
                discovery.Pool() == CardPoolId::COLORLESS_CARD_POOL && discovery.VisualCardPoolIsColorless() &&
                eventCard.Pool() == CardPoolId::EVENT_CARD_POOL && eventCard.VisualCardPoolIsColorless() &&
                clash.Pool() == CardPoolId::EVENT_CARD_POOL &&
                clash.VisualCardPool() == CardPoolId::IRONCLAD_CARD_POOL && !clash.VisualCardPoolIsColorless(),
            "colorless eligibility must use the visual pool, including token and overridden event cards");
}

void test_hammer_selection_and_clone_repeat_semantics() {
    {
        HammerFixture fixture(/*includeDiscovery=*/true, /*repeatCount=*/1);
        auto selectedSovereignBlade = choose_from_hammer(fixture, /*choiceIndex=*/0, /*expectedOptions=*/2);
        require_clone_count(*selectedSovereignBlade, CardId::SOVEREIGN_BLADE, 1);

        auto selectedDiscovery = choose_from_hammer(fixture, /*choiceIndex=*/1, /*expectedOptions=*/2);
        require_clone_count(*selectedDiscovery, CardId::DISCOVERY, 1);
    }
    {
        HammerFixture fixture(/*includeDiscovery=*/true, /*repeatCount=*/2);
        auto selectedSovereignBlade = choose_from_hammer(fixture, /*choiceIndex=*/0, /*expectedOptions=*/2);
        require_clone_count(*selectedSovereignBlade, CardId::SOVEREIGN_BLADE, 2);
    }
    {
        // The A10 divergence has only Sovereign Blade among the visual-colorless
        // cards. FromHand auto-selects that sole candidate, then Hammer clones it.
        HammerFixture fixture(/*includeDiscovery=*/false, /*repeatCount=*/1);
        auto completed = fixture.env.Predict(fixture.playHammer());
        require(completed->decision().kind == DecisionKind::Combat,
                "a sole eligible Heirloom Hammer target must resolve without a selector prompt");
        require_clone_count(*completed, CardId::SOVEREIGN_BLADE, 1);
    }
}

}  // namespace

int main() {
    test_visual_card_pool_matches_all_v0110_overrides();
    test_hammer_selection_and_clone_repeat_semantics();
    std::cout << "Heirloom Hammer audit passed\n";
}
