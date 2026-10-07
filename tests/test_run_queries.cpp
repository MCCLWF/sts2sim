// Public API contracts. These fixtures exercise ordinary execution and owned
// projections; vanilla fidelity is checked independently by L3/L4 oracle tests.
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include "env/preview.h"
#include "env/snapshot_json.h"
#include "game/All.h"
#include "game/Commands/CardPileCmd.h"
#include "game/Commands/RelicCmd.h"

using namespace sts2;
using snapshot_detail::Json;
static void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
static Json query(const Env& env, bool future = false) {
    const auto hash = env.Hash();
    auto value = snapshot_detail::Parser(ExportRunQueries(env, {.include_future = future})).parse();
    require(env.Hash() == hash, "query changed source");
    require(value.at("state_hash").integer<uint64_t>() == hash, "query hash provenance");
    require(value.at("legal_actions").array.size() == env.LegalActions().size(), "legal action count");
    require(!value.at("is_agent_observation").boolean, "consumer query claims agent observation");
    require(value.at("future").is_null() != future, "future visibility flag");
    return value;
}
static void restored_answers(const Env& env) {
    auto restored = Env::ImportSnapshotJson(env.ExportSnapshotJson());
    require(query(env).at("choice").at("options").array.size() ==
            query(*restored).at("choice").at("options").array.size(), "choice descriptions lost on restore");
    const auto actions = env.LegalActions();
    require(actions == restored->LegalActions(), "restored choice legal answers");
    require(env.Predict(actions.front())->Hash() == restored->Predict(actions.front())->Hash(), "restored answer diverged");
}
static std::unique_ptr<Env> map_root() {
    auto env = std::make_unique<Env>();
    new_run(env->state(), {.seed = "RFRESULTS", .character = CharacterId::IRONCLAD});
    env->state().run.started_with_neow = false;
    env->Step({.kind = Action::StartRun});
    return env;
}
static std::unique_ptr<Env> room(MapPointType type) {
    auto env = map_root();
    const auto move = env->LegalActions().front();
    const int point = env->state().run.map.get_point(move.coord.col, move.coord.row);
    require(point >= 0, "room map fixture");
    env->state().run.map.points[point].type = type;
    env->Step(move);
    return env;
}
static PreviewResult preview(const Env& env, Action action) {
    const auto hash = env.Hash();
    auto result = PreviewAction(env, action, {.collect_trace = false, .capture_run_results = true});
    const auto json = snapshot_detail::Parser(result.ExportJson()).parse();
    require(json.at("version").integer<int>() == 2, "result envelope version");
    require(json.at("run_results").at("before").at("state_hash").integer<uint64_t>() == hash, "before provenance");
    require(json.at("run_results").at("after").at("state_hash").integer<uint64_t>() == result.successor->Hash(), "after provenance");
    require(result.successor->Hash() == env.Predict(action)->Hash(), "result capture changed execution");
    require(env.Hash() == hash, "result capture changed source");
    return result;
}
int main() {
    auto map = map_root();
    query(*map); query(*map, true);
    Env event; event.Reset({.seed = "DUAL001", .character = CharacterId::IRONCLAD});
    require(!query(event).at("event").at("options").array.empty(), "event facts missing");

    auto merchant = map_root();
    RelicCmd::Obtain(RelicId::THE_COURIER, Player(&merchant->sim(), 1));
    merchant->state().run.player.gold = 10000;
    auto move = merchant->LegalActions().front();
    merchant->state().run.map.points[merchant->state().run.map.get_point(move.coord.col, move.coord.row)].type = MapPointType::Shop;
    merchant->Step(move);
    auto shop = query(*merchant).at("shop");
    auto buy = std::find_if(shop.at("entries").array.begin(), shop.at("entries").array.end(), [](const auto& entry) {
        return entry.at("kind").text == "character_card" && entry.at("buyable").boolean;
    });
    require(buy != shop.at("entries").array.end(), "shop purchase fixture");
    const int index = buy->at("action_index").template integer<int>();
    auto bought = preview(*merchant, {.kind = Action::ShopBuy, .index = index});
    const auto after_shop = query(*bought.successor).at("shop").at("entries").array;
    require(after_shop.front().at("stocked").boolean, "Courier restock missing in results");
    require(query(*bought.successor).at("player").at("gold").integer<int>() ==
            merchant->state().run.player.gold - buy->at("cost").template integer<int>(), "shop price/result mapping");

    for (const auto id : {RelicId::NEW_LEAF, RelicId::SCROLL_BOXES, RelicId::CAULDRON}) {
        auto treasure = room(MapPointType::Treasure);
        treasure->state().run.treasure.current_relics.clear();
        treasure->state().run.treasure.current_relics.push_back(id);
        auto picked = preview(*treasure, {.kind = Action::TreasurePick, .index = 0});
        require(picked.successor->decision().kind == DecisionKind::Choice, "pickup choice missing");
        const auto data = query(*picked.successor).at("choice");
        const auto& options = data.at("options").array;
        require(!options.empty(), "empty choice description");
        if (id == RelicId::SCROLL_BOXES) {
            require(data.at("kind").text == "bundle" && options.size() == 2, "bundle identity");
            for (const auto& option : options) require(option.at("kind").text == "bundle" && option.at("cards").array.size() == 3, "bundle boundaries lost");
        } else if (id == RelicId::CAULDRON) {
            require(options.back().at("kind").text == "proceed", "nested reward proceed missing");
            for (size_t i = 0; i + 1 < options.size(); ++i)
                require(options[i].at("kind").text == "reward" && !options[i].at("reward").is_null(), "nested reward identity missing");
        } else require(options.front().at("kind").text == "card", "transform candidate identity");
        restored_answers(*picked.successor);
        const auto before_deck = query(*picked.successor).at("player").at("deck").array;
        const int transform_ref = id == RelicId::NEW_LEAF
            ? options.front().at("cards").array.front().at("card_ref").integer<int>() : 0;
        auto answered = preview(*picked.successor, picked.successor->LegalActions().front());
        for (int i = 0; i < 8 && answered.successor->decision().kind == DecisionKind::Choice; ++i) {
            const auto actions = answered.successor->LegalActions();
            const auto slots = query(*answered.successor).at("player").at("potions").array;
            const bool full = std::all_of(slots.begin(), slots.end(), [](const auto& p) { return !p.at("item").is_null(); });
            // Cauldron offers five potions and the default player has three
            // slots. The fixture takes three, then chooses the actual Proceed.
            const auto action = id == RelicId::CAULDRON && full ? actions.back() : actions.front();
            answered = preview(*answered.successor, action);
        }
        require(answered.successor->decision().kind != DecisionKind::Choice, "pickup choices did not finish");
        const auto after = query(*answered.successor).at("player");
        const auto& after_deck = after.at("deck").array;
        if (id == RelicId::NEW_LEAF) {
            require(after_deck.size() == before_deck.size(), "transform result deck size");
            require(std::none_of(after_deck.begin(), after_deck.end(), [&](const auto& c) {
                return c.at("card_ref").template integer<int>() == transform_ref;
            }), "transform result retained original deck card");
        } else if (id == RelicId::SCROLL_BOXES) {
            require(after_deck.size() == before_deck.size() + options.front().at("cards").array.size(),
                    "bundle pickup result missing cards");
        } else {
            require(std::all_of(after.at("potions").array.begin(), after.at("potions").array.end(),
                    [](const auto& p) { return !p.at("item").is_null(); }), "nested reward result missing potions");
        }
    }
    auto rest = room(MapPointType::RestSite);
    require(!query(*rest).at("rest_site").at("options").array.empty(), "rest facts missing");
    auto smith = preview(*rest, rest->LegalActions().back());
    require(query(*smith.successor).at("choice").at("answer_semantics").text == "count", "subset count semantics");
    restored_answers(*smith.successor);

    auto combat = std::make_unique<Env>();
    combat->ResetCombat({.seed = "DUAL001", .character = CharacterId::IRONCLAD}, EncounterId::FROG_KNIGHT_NORMAL);
    Player player(&combat->sim(), 1);
    RelicCmd::Obtain(RelicId::DRIFTWOOD, player);
    auto enemy = CombatState(&combat->sim()).Enemies().front();
    enemy.st().current_hp = 1; enemy.st().block = 0;
    auto strike = CombatState(&combat->sim()).CreateCard(CardId::STRIKE_IRONCLAD, player);
    CardPileCmd::Add(strike, PileType::Hand);
    auto win = preview(*combat, {.kind = Action::PlayCard, .index = player.PlayerCombatState().Hand().Count() - 1, .target = enemy.CombatId()});
    auto completed = win.successor->Clone();
    for (int i = 0; i < 16; ++i) {
        if (completed->decision().kind == DecisionKind::Choice) {
            completed = completed->Predict(completed->LegalActions().front());
            continue;
        }
        const auto actions = completed->LegalActions();
        const auto take = std::find_if(actions.begin(), actions.end(), [](const auto& a) { return a.kind == Action::TakeReward; });
        if (take == actions.end()) break;
        completed = completed->Predict(*take);
    }
    require(completed->decision().kind == DecisionKind::Rewards && !completed->state().run.rewards.active,
            "completed rewards screen fixture");
    require(!query(*completed).at("rewards").is_null(), "completed reward screen lost visible entries");
    bool reroll_found = false;
    for (auto a : win.successor->LegalActions()) {
        if (a.kind != Action::TakeReward) continue;
        auto selected = win.successor->Predict(a);
        if (selected->decision().kind != DecisionKind::Choice) continue;
        const auto options = query(*selected).at("choice").at("options").array;
        auto reroll = std::find_if(options.begin(), options.end(), [](const auto& o) { return o.at("key").text == "REROLL"; });
        if (reroll == options.end()) continue;
        require(options[options.size() - 2].at("key").text == "Skip", "original Skip key/order");
        auto rerolled = preview(*selected, {.kind = Action::Answer, .index = static_cast<int32_t>(reroll - options.begin())});
        require(rerolled.successor->decision().kind == DecisionKind::Choice, "reroll did not offer new cards");
        // CardReward.OnSelect retains backend callback indices, while the
        // live screen refresh removes REROLL. Keep the index descriptive but
        // unavailable in both fresh actions and restored prefixes.
        const auto refreshed = query(*rerolled.successor).at("choice").at("options").array;
        const int32_t reroll_index = static_cast<int32_t>(reroll - options.begin());
        require(!refreshed[static_cast<size_t>(reroll_index)].at("enabled").boolean,
            "already-used reroll retained a UI button");
        const Action again{.kind = Action::Answer, .index = reroll_index};
        const auto legal = rerolled.successor->LegalActions();
        require(std::find(legal.begin(), legal.end(), again) == legal.end(), "second reroll remained legal");
        try { rerolled.successor->Predict(again); throw std::runtime_error("second reroll accepted"); }
        catch (const InvalidRequest&) { }
        const auto revised = query(*rerolled.successor).at("rewards").at("entries").array;
        require(std::any_of(revised.begin(), revised.end(), [](const auto& o) {
            const auto& r = o.at("reward");
            return r.at("has_been_rerolled").boolean && !r.at("can_reroll").boolean;
        }), "reroll reward state missing in results");
        restored_answers(*rerolled.successor);
        reroll_found = true; break;
    }
    require(reroll_found, "Driftwood card reward alternative not exercised");
    std::cout << "Run-query contracts passed: purity/visibility, owned results, Courier, transform/bundle/nested reward/subset choices, Driftwood reroll, restore.\n";
}
