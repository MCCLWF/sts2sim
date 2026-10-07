// See observation.h.
#include "nosl/observation.h"

#include <algorithm>
#include <array>
#include <map>
#include <string_view>
#include <tuple>

#include "engine/sim/errors.h"
#include "game/All.h"
#include "game/Entities/Merchant/MerchantInventory.h"
#include "game/Events/EventOption.h"
#include "game/MonsterMoves/Intents/AttackIntent.h"
#include "game/Runs/RunFlow.h"

namespace sts2::nosl {

namespace {

using Token = std::array<int32_t, kTokenWidth>;

int32_t Bucket(std::string_view text) {
    if (text.empty()) return 0;
    uint32_t h = 2166136261u;  // FNV-1a
    for (unsigned char c : text) { h ^= c; h *= 16777619u; }
    return misc::kHashed + static_cast<int32_t>(h % static_cast<uint32_t>(misc::kHashBuckets));
}
int32_t Bucket(uint32_t key_hash) {
    return key_hash == 0 ? 0 : misc::kHashed + static_cast<int32_t>(key_hash % static_cast<uint32_t>(misc::kHashBuckets));
}

struct Builder {
    Observation& out;
    Sim* sim;

    int32_t Push(const Token& t) {
        out.tokens.insert(out.tokens.end(), t.begin(), t.end());
        return static_cast<int32_t>(out.tokens.size() / kTokenWidth) - 1;
    }
    static Token Make(TokenType type) {
        Token t{};
        t[kColType] = static_cast<int32_t>(type);
        return t;
    }
    static void SetCard(Token& t, CardModel card) {
        t[kColCard] = static_cast<int32_t>(card.CardId_());
        if (EnchantmentModel e = card.Enchantment()) t[kColMiscA] = misc::kEnchantment + static_cast<int32_t>(e.EnchantmentId_());
        if (AfflictionModel a = card.Affliction()) t[kColMiscB] = misc::kAffliction + static_cast<int32_t>(a.AfflictionId_());
        t[kColN0] = card.CurrentUpgradeLevel();
        t[kColN3] = static_cast<int32_t>(card.Type());
    }
    // A pile as a multiset: identical cards share one token with a count.
    // The key order makes the emission independent of the pile's order.
    void PushBag(TokenType type, CardPile pile) {
        std::map<std::tuple<int32_t, int32_t, int32_t, int32_t>, std::pair<Token, int32_t>> bag;
        for (CardModel card : pile.Cards()) {
            Token t = Make(type);
            SetCard(t, card);
            auto [it, fresh] = bag.try_emplace({t[kColCard], t[kColN0], t[kColMiscA], t[kColMiscB]}, t, 0);
            ++it->second.second;
        }
        for (auto& [key, entry] : bag) {
            entry.first[kColN1] = entry.second;
            Push(entry.first);
        }
    }
    void SetRelic(Token& t, RelicModel relic) {
        t[kColRelic] = static_cast<int32_t>(relic.RelicId_());
    }
    void SetReward(Token& t, Reward reward) {
        const RewardState& s = reward.st();
        t[kColMiscA] = misc::kRewardType + static_cast<int32_t>(reward.Type());
        if (s.relic) SetRelic(t, RelicModel(sim, s.relic));
        if (s.potion) t[kColPotion] = static_cast<int32_t>(PotionModel(sim, s.potion).PotionId_());
        t[kColN1] = std::max(0, s.gold_amount);
        t[kColN3] = reward.SuccessfullySelected();
    }
};

}  // namespace

void Observe(const Env& env, Observation& out) {
    out.global.assign(kGlobalWidth, 0);
    out.tokens.clear();
    out.actions.clear();
    out.legal.clear();

    std::unique_ptr<Env> scratch = env.Clone();
    Sim* sim = &scratch->sim();
    const GameState& g = scratch->state();
    const RunData& run = g.run;
    const Decision decision = scratch->decision();
    std::vector<Action> legal = scratch->LegalActions();
    Player player(sim, 1);
    PlayerCombatState pcs = player.PlayerCombatState();
    const bool in_combat = g.combat.is_in_progress && static_cast<bool>(pcs);
    Builder b{out, sim};
    auto& G = out.global;

    G[kGSchema] = kObsSchemaVersion;
    G[kGDecision] = static_cast<int32_t>(decision.kind);
    G[kGCharacter] = static_cast<int32_t>(player.Character());
    G[kGAscension] = run.ascension;
    G[kGAct] = run.current_act_index;
    G[kGActFloor] = run.act_floor;
    G[kGTotalFloor] = run.total_floor;
    G[kGHp] = player.Creature().CurrentHp();
    G[kGMaxHp] = player.Creature().MaxHp();
    G[kGBlock] = player.Creature().Block();
    G[kGGold] = player.Gold();
    G[kGInCombat] = in_combat;
    G[kGDeckCount] = static_cast<int32_t>(player.Deck().Cards().size());
    G[kGPotionSlots] = player.MaxPotionCount();
    G[kGRoomType] = run.rooms.empty() ? 0 : static_cast<int32_t>(run.rooms.back());
    if (run.current_act_index >= 0 && run.current_act_index < static_cast<int32_t>(run.acts.size()))
        G[kGBoss] = static_cast<int32_t>(run.acts[static_cast<size_t>(run.current_act_index)].rooms.boss);

    // Relics and potions are on screen at every decision.
    for (RelicModel relic : player.Relics()) {
        Token t = Builder::Make(TokenType::Relic);
        b.SetRelic(t, relic);
        t[kColN0] = relic.ShowCounter() ? relic.DisplayAmount() : -1;
        t[kColN1] = static_cast<int32_t>(relic.Status());
        b.Push(t);
        ++G[kGRelicCount];
    }
    std::array<int32_t, 16> potion_token;
    potion_token.fill(-1);
    for (int32_t slot = 0; slot < player.MaxPotionCount() && slot < 16; ++slot) {
        PotionModel potion = player.GetPotionAtSlotIndex(slot);
        if (!potion) continue;
        Token t = Builder::Make(TokenType::Potion);
        t[kColPotion] = static_cast<int32_t>(potion.PotionId_());
        t[kColN0] = slot;
        potion_token[static_cast<size_t>(slot)] = b.Push(t);
        ++G[kGPotionCount];
    }

    std::vector<int32_t> hand_token;
    std::map<int32_t, int32_t> enemy_token;  // combat id -> token
    if (in_combat) {
        CombatState combat(sim);
        G[kGEnergy] = pcs.Energy();
        G[kGMaxEnergy] = pcs.MaxEnergy();
        G[kGStars] = pcs.Stars();
        G[kGTurn] = pcs.TurnNumber();
        G[kGRound] = combat.RoundNumber();
        G[kGHandCount] = pcs.Hand().Count();
        G[kGDrawCount] = pcs.DrawPile().Count();
        G[kGDiscardCount] = pcs.DiscardPile().Count();
        G[kGExhaustCount] = pcs.ExhaustPile().Count();
        int32_t index = 0;
        for (CardModel card : pcs.Hand().Cards()) {
            Token t = Builder::Make(TokenType::Hand);
            Builder::SetCard(t, card);
            t[kColN1] = card.EnergyCost().GetWithModifiers(CostModifiers::All);
            t[kColN2] = card.GetStarCostWithModifiers();
            t[kColN4] = card.CanPlay();
            t[kColN5] = index++;
            hand_token.push_back(b.Push(t));
        }
        b.PushBag(TokenType::Draw, pcs.DrawPile());
        b.PushBag(TokenType::Discard, pcs.DiscardPile());
        b.PushBag(TokenType::Exhaust, pcs.ExhaustPile());
        auto push_powers = [&](Creature owner, int32_t owner_slot) {
            for (PowerModel power : owner.Powers()) {
                Token t = Builder::Make(TokenType::Power);
                t[kColPower] = static_cast<int32_t>(power.PowerId_());
                t[kColN0] = power.Amount();
                t[kColN1] = owner_slot;
                b.Push(t);
            }
        };
        push_powers(player.Creature(), 0);
        CreatureList targets = combat.PlayerCreatures();
        int32_t slot = 0;
        for (Creature enemy : combat.Enemies()) {
            MonsterModel monster = enemy.Monster();
            Token t = Builder::Make(TokenType::Enemy);
            t[kColMonster] = static_cast<int32_t>(monster.MonsterId_());
            t[kColN0] = enemy.CurrentHp();
            t[kColN1] = enemy.MaxHp();
            t[kColN2] = enemy.Block();
            t[kColN3] = -1;
            for (const Intent& intent : monster.NextMove().Intents()) {
                t[kColN5] |= 1 << static_cast<int32_t>(intent.type);
                if (AttackIntent::IsAttack(intent)) {
                    t[kColN3] = AttackIntent::GetSingleDamage(intent, targets, enemy);
                    t[kColN4] = AttackIntent::Repeats(intent, monster);
                }
            }
            t[kColN6] = slot;
            t[kColN7] = enemy.IsAlive();
            enemy_token[enemy.CombatId()] = b.Push(t);
            push_powers(enemy, ++slot);
        }
    } else {
        b.PushBag(TokenType::Deck, player.Deck());
    }

    // Decision-specific tokens. `option_token` maps the decision's option
    // index to its token for the action table below.
    std::vector<int32_t> option_token;
    std::map<std::pair<int32_t, int32_t>, int32_t> map_token;  // (col, row) of a travelable point
    bool sortable_answers = false;
    switch (decision.kind) {
    case DecisionKind::Map: {
        const ActMap& map = run.map;
        const bool started = !run.visited_map_coords.empty();
        const MapCoord here = started ? run.visited_map_coords.back() : MapCoord{};
        auto cone = [&](int start) {
            std::vector<uint8_t> seen(map.points.size(), 0);
            std::vector<int> stack{start};
            seen[static_cast<size_t>(start)] = 1;
            while (!stack.empty()) {
                int p = stack.back();
                stack.pop_back();
                map.points[static_cast<size_t>(p)].children.for_each([&](uint8_t child) {
                    if (!seen[child]) { seen[child] = 1; stack.push_back(child); }
                });
            }
            return seen;
        };
        std::vector<uint8_t> reachable(map.points.size(), started ? 0 : 1);
        if (started) {
            int p = map.get_point(here.col, here.row);
            if (p >= 0) reachable = cone(p);
        }
        // The points on the map screen, in grid order (ActMap.GetAllMapPoints) and then the boss, the start and the
        // second boss. `map.points` also keeps points the generator made and then pruned; they are not on the map.
        std::vector<size_t> shown;
        for (int col = 0; col < map.cols; ++col)
            for (int row = 0; row < map.rows; ++row)
                if (int p = map.point_at(col, row); p >= 0) shown.push_back(static_cast<size_t>(p));
        for (int special : {static_cast<int>(map.boss), static_cast<int>(map.start), static_cast<int>(map.second_boss)})
            if (special >= 0 && std::find(shown.begin(), shown.end(), static_cast<size_t>(special)) == shown.end())
                shown.push_back(static_cast<size_t>(special));
        for (size_t i : shown) {
            const MapPoint& point = map.points[i];
            Token t = Builder::Make(TokenType::MapNode);
            t[kColMiscA] = misc::kMapPoint + static_cast<int32_t>(point.type);
            t[kColN0] = point.coord.col;
            t[kColN1] = point.coord.row;
            t[kColN2] = std::find(run.visited_map_coords.begin(), run.visited_map_coords.end(), point.coord) != run.visited_map_coords.end();
            t[kColN3] = started && point.coord == here;
            point.children.for_each([&](uint8_t child) {
                int32_t col = map.points[child].coord.col;
                if (col >= 0 && col < 31) t[kColN4] |= 1 << col;
            });
            t[kColN5] = reachable[i];
            b.Push(t);
        }
        for (const Action& a : legal) {
            if (a.kind != Action::MapMove) continue;
            int p = map.get_point(a.coord.col, a.coord.row);
            Token t = Builder::Make(TokenType::MapNext);
            t[kColN0] = a.coord.col;
            t[kColN1] = a.coord.row;
            if (p >= 0) {
                t[kColMiscA] = misc::kMapPoint + static_cast<int32_t>(map.points[static_cast<size_t>(p)].type);
                std::vector<uint8_t> seen = cone(p);
                for (size_t i = 0; i < seen.size(); ++i) {
                    if (!seen[i] || static_cast<int>(i) == p) continue;
                    switch (map.points[i].type) {
                    case MapPointType::Elite: ++t[kColN2]; break;
                    case MapPointType::RestSite: ++t[kColN3]; break;
                    case MapPointType::Shop: ++t[kColN4]; break;
                    case MapPointType::Unknown: ++t[kColN5]; break;
                    case MapPointType::Monster: ++t[kColN6]; break;
                    case MapPointType::Treasure: ++t[kColN7]; break;
                    default: break;
                    }
                }
            } else {
                t[kColMiscA] = misc::kMapPoint + static_cast<int32_t>(MapPointType::Boss);
            }
            map_token[{a.coord.col, a.coord.row}] = b.Push(t);
        }
        break;
    }
    case DecisionKind::Rewards: {
        int32_t index = 0;
        for (RewardRef ref : run.rewards.rewards) {
            Token t = Builder::Make(TokenType::Reward);
            b.SetReward(t, Reward(sim, ref));
            t[kColN0] = index;
            t[kColN2] = RunFlow::RewardTakable(sim, index);
            option_token.push_back(b.Push(t));
            ++index;
        }
        break;
    }
    case DecisionKind::Event: {
        const EventModel event(sim, 1);
        G[kGEvent] = static_cast<int32_t>(event.st().id);
        int32_t index = 0;
        for (const EventOptionState& option : event.st().options) {
            Token t = Builder::Make(TokenType::EventOption);
            t[kColMiscA] = misc::kEvent + static_cast<int32_t>(event.st().id);
            t[kColMiscB] = Bucket(option.text_key);
            if (option.relic) b.SetRelic(t, RelicModel(sim, option.relic));
            t[kColN0] = index++;
            t[kColN1] = option.is_locked;
            t[kColN2] = WillKillPlayer(option, player);
            t[kColN3] = option.is_proceed;
            option_token.push_back(b.Push(t));
        }
        break;
    }
    case DecisionKind::RestSite: {
        int32_t index = 0;
        for (const RunFlow::RestSiteOptionView& option : RunFlow::RestSiteOptions(sim)) {
            Token t = Builder::Make(TokenType::RestOption);
            t[kColMiscA] = misc::kRestOption + static_cast<int32_t>(option.id);
            t[kColN0] = index++;
            t[kColN1] = option.enabled;
            option_token.push_back(b.Push(t));
        }
        break;
    }
    case DecisionKind::Shop: {
        // Every displayed entry is a token, affordable or not. `option_token`
        // is indexed by the ShopBuy action index (the stocked entries).
        const MerchantState& state = run.merchant;
        const auto stocked = RunFlow::ShopOptions(sim);
        option_token.assign(stocked.size(), -1);
        for (const Merchant::EntryRef entry : Merchant::AllEntries(sim)) {
            Token t = Builder::Make(TokenType::Shop);
            t[kColMiscA] = misc::kShopKind + static_cast<int32_t>(entry.kind);
            const bool is_stocked = Merchant::IsStocked(sim, entry);
            t[kColN0] = Merchant::Cost(sim, entry);
            t[kColN1] = is_stocked;
            t[kColN2] = is_stocked && Merchant::EnoughGold(sim, entry);
            switch (entry.kind) {
            case Merchant::EntryRef::CharacterCard:
            case Merchant::EntryRef::ColorlessCard: {
                const MerchantCardEntryState& c = entry.kind == Merchant::EntryRef::CharacterCard
                    ? state.character_cards[entry.index] : state.colorless_cards[entry.index];
                if (c.card) {
                    CardModel card(sim, c.card);
                    t[kColCard] = static_cast<int32_t>(card.CardId_());
                    t[kColN4] = card.CurrentUpgradeLevel();
                }
                t[kColN3] = c.on_sale;
                break;
            }
            case Merchant::EntryRef::Relic:
                if (state.relics[entry.index].relic) b.SetRelic(t, RelicModel(sim, state.relics[entry.index].relic));
                break;
            case Merchant::EntryRef::Potion:
                if (state.potions[entry.index].potion)
                    t[kColPotion] = static_cast<int32_t>(PotionModel(sim, state.potions[entry.index].potion).PotionId_());
                break;
            case Merchant::EntryRef::CardRemoval: break;
            }
            const int32_t token = b.Push(t);
            const auto found = std::find(stocked.begin(), stocked.end(), entry);
            if (found != stocked.end()) option_token[static_cast<size_t>(found - stocked.begin())] = token;
        }
        break;
    }
    case DecisionKind::Treasure: {
        int32_t index = 0;
        for (RelicId id : run.treasure.current_relics) {
            Token t = Builder::Make(TokenType::Treasure);
            t[kColRelic] = static_cast<int32_t>(id);
            t[kColN0] = index++;
            option_token.push_back(b.Push(t));
        }
        break;
    }
    case DecisionKind::Choice: {
        const ChoiceRequest& request = decision.request;
        const bool count_prompt = request.subset && request.prompt == "count";
        G[kGChoiceKind] = 1 + static_cast<int32_t>(request.kind);
        G[kGChoiceMin] = request.min_select;
        G[kGChoiceMax] = request.max_select;
        G[kGChoiceSelected] = static_cast<int32_t>(request.selected.size());
        G[kGChoiceSubset] = request.subset ? (count_prompt ? 2 : 1) : 0;
        G[kGChoicePrompt] = Bucket(request.prompt);
        G[kGChoiceSource] = Bucket(request.source);
        G[kGChoiceCanSkip] = request.can_skip;
        if (request.options.size() != static_cast<size_t>(request.option_count))
            throw GameError("nosl observation: incomplete choice descriptions");
        // A card selection's backing order can follow a hidden pile order, so
        // its option index is not shown and its answers are reordered below.
        sortable_answers = request.kind == ChoiceKind::CardSelect && !count_prompt;
        std::vector<Token> options;
        int32_t index = 0;
        for (const ChoiceOption& option : request.options) {
            Token t = Builder::Make(TokenType::ChoiceOption);
            if (!option.cards.empty()) Builder::SetCard(t, CardModel(sim, option.cards.front()));
            if (option.reward) b.SetReward(t, Reward(sim, option.reward));
            // After SetCard/SetReward: an option's kind and key own the misc columns.
            const int32_t enchantment = t[kColMiscA];
            t[kColMiscA] = misc::kChoiceKind + static_cast<int32_t>(option.kind);
            t[kColMiscB] = option.key.empty() ? enchantment : Bucket(option.key);
            t[kColN2] = option.enabled;
            t[kColN4] = sortable_answers && option.kind == ChoiceOptionKind::Card ? -1 : index;
            t[kColN5] = static_cast<int32_t>(option.cards.size());
            t[kColN6] = std::find(request.selected.begin(), request.selected.end(), index) != request.selected.end();
            options.push_back(t);
            ++index;
        }
        std::vector<size_t> order(options.size());
        for (size_t i = 0; i < order.size(); ++i) order[i] = i;
        if (sortable_answers)
            std::stable_sort(order.begin(), order.end(), [&](size_t x, size_t y) { return options[x] < options[y]; });
        option_token.assign(options.size(), -1);
        for (size_t i : order) option_token[i] = b.Push(options[i]);
        break;
    }
    default: break;
    }

    // The action table. A card selection's option tokens were emitted in
    // content order; its answers follow them, so neither order carries a
    // hidden pile order.
    struct Row { std::array<int32_t, kActionWidth> v; Action action; };
    std::vector<Row> rows;
    auto option = [&](int32_t index) {
        return index >= 0 && static_cast<size_t>(index) < option_token.size() ? option_token[static_cast<size_t>(index)] : -1;
    };
    for (const Action& a : legal) {
        Row row{{static_cast<int32_t>(a.kind), -1, -1, 0}, a};
        auto target = [&] {
            if (a.target < 0) return;
            auto found = enemy_token.find(a.target);
            if (found != enemy_token.end()) row.v[kActTarget] = found->second;
            else row.v[kActNumber] = a.target;
        };
        switch (a.kind) {
        case Action::MapMove: row.v[kActToken] = map_token.at({a.coord.col, a.coord.row}); break;
        case Action::PlayCard:
            if (a.index >= 0 && static_cast<size_t>(a.index) < hand_token.size()) row.v[kActToken] = hand_token[static_cast<size_t>(a.index)];
            target();
            break;
        case Action::UsePotion:
            if (a.index >= 0 && a.index < 16) row.v[kActToken] = potion_token[static_cast<size_t>(a.index)];
            target();
            break;
        case Action::DiscardPotion:
            if (a.index >= 0 && a.index < 16) row.v[kActToken] = potion_token[static_cast<size_t>(a.index)];
            break;
        case Action::TakeReward: case Action::EventOption: case Action::RestOption:
        case Action::ShopBuy: case Action::TreasurePick:
            row.v[kActToken] = option(a.index);
            break;
        case Action::Answer:
            if (G[kGChoiceSubset] == 2) row.v[kActNumber] = a.index;
            else row.v[kActToken] = option(a.index);
            break;
        default: break;
        }
        rows.push_back(row);
    }
    if (sortable_answers)
        std::stable_sort(rows.begin(), rows.end(), [](const Row& x, const Row& y) { return x.v[kActToken] < y.v[kActToken]; });
    for (const Row& row : rows) {
        out.actions.insert(out.actions.end(), row.v.begin(), row.v.end());
        out.legal.push_back(row.action);
    }
    G[kGTokenCount] = static_cast<int32_t>(out.token_count());
    G[kGActionCount] = static_cast<int32_t>(out.action_count());
}

}  // namespace sts2::nosl
