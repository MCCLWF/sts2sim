// See actor.h.
#include "nosl/actor.h"

#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "content/generated/ids.h"
#include "engine/sim/errors.h"

namespace sts2::nosl {

namespace {

constexpr const char* kBundleMagic = "nosl-policy-bundle";

bool IsBundle(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::string word;
    in >> word;
    return word == kBundleMagic;
}

}  // namespace

Actor::Actor(const std::string& weights, int32_t actor_filter, uint64_t policy_seed, bool greedy)
    : filter_(actor_filter), rng_(policy_seed), greedy_(greedy) {
    if (!IsBundle(weights)) {
        net_ = PolicyNet::Load(weights);
        return;
    }
    const size_t slash = weights.find_last_of('/');
    const std::string directory = slash == std::string::npos ? "" : weights.substr(0, slash + 1);
    std::ifstream in(weights);
    std::string line, key, value;
    std::getline(in, line);
    std::istringstream head(line);
    int version = 0;
    head >> key >> version;
    if (version != 1) throw std::runtime_error("policy bundle of another version: " + weights);
    while (std::getline(in, line)) {
        std::istringstream fields(line);
        if (!(fields >> key >> value)) continue;
        if (key == "combat") net_ = PolicyNet::Load(directory + value);
        else if (key == "base") base_ = PolicyNet::Load(directory + value);
        else if (key == "strategy") strategy_ = PolicyNet::Load(directory + value);
        else if (key == "margin") margin_ = std::stof(value);
        else if (key == "table_delta") table_delta_ = std::stod(value);
        else if (key == "table") {
            std::ifstream rows(directory + value);
            if (!rows) throw std::runtime_error("policy bundle: cannot read table " + value);
            for (std::string row; std::getline(rows, row);) {
                const size_t a = row.find('\t'), b = row.rfind('\t');
                if (a == std::string::npos || a == b) continue;
                table_[row.substr(0, a)][row.substr(a + 1, b - a - 1)] = std::stod(row.substr(b + 1));
            }
        }
        else if (key == "turn_cap") turn_cap_ = std::stoi(value);
        else if (key == "combat_value") combat_value_ = PolicyNet::Load(directory + value);
        else if (key == "combat_margin") combat_margin_ = std::stof(value);
        else if (key == "guards") guards_ = ParseGuards(value);
        else if (key == "guards_low") {
            std::string list;
            fields >> list;
            guards_low_max_ = std::stoi(value);
            guards_low_ = ParseGuards(list);
        }
        else if (key == "readiness_lambda") readiness_lambda_ = std::stod(value);
        else if (key == "readiness_chain") readiness_chain_ = value == "1";
        else if (key == "readiness_table") readiness_table_ = value == "1";
        else if (key == "readiness_scope") {
            std::istringstream parts(value);
            for (std::string part; std::getline(parts, part, ',');) {
                if (part == "cards") scope_cards_ = true;
                else if (part == "upgrade") scope_upgrade_ = true;
                else if (part == "remove") scope_remove_ = true;
                else if (part == "rest") scope_rest_ = true;
                else throw std::runtime_error("policy bundle: unknown readiness scope " + part);
            }
        }
        else if (key == "readiness") {
            std::ifstream rows(directory + value);
            if (!rows) throw std::runtime_error("policy bundle: cannot read readiness " + value);
            auto door = [&](const std::string& name) -> Door& {
                for (Door& d : doors_) if (d.name == name) return d;
                throw std::runtime_error("policy bundle: readiness row for an unknown door " + name);
            };
            for (std::string row; std::getline(rows, row);) {
                std::vector<std::string> f;
                std::istringstream cells(row);
                for (std::string cell; std::getline(cells, cell, '\t');) f.push_back(cell);
                if (f.empty()) continue;
                if (f[0] == "door" && f.size() == 7) {
                    Door d;
                    d.name = f[1]; d.act = std::stoi(f[2]); d.which = f[3]; d.floor = std::stoi(f[4]); d.passes = std::stoi(f[5]);
                    d.hp10 = std::stod(f[6]);
                    doors_.push_back(std::move(d));
                } else if (f[0] == "boss" && f.size() == 3) {
                    door(f[1]).bosses.insert(std::stoi(f[2]));
                } else if (f[0] == "card" && f.size() == 11) {
                    std::array<double, 8> c{};
                    for (size_t i = 0; i < 8; ++i) c[i] = std::stod(f[3 + i]);
                    door(f[1]).card[std::stoi(f[2])] = c;
                } else if (f[0] == "card_boss" && f.size() == 5) {
                    // The boss rows of a door come before these: the mean is over all of its bosses, summed in file order.
                    Door& d = door(f[1]);
                    const int32_t card = std::stoi(f[3]);
                    const double v = std::stod(f[4]);
                    d.card_boss[std::stoi(f[2])][card] = v;
                    d.card_boss_mean[card] += v / static_cast<double>(d.bosses.size());
                } else if (f[0] == "weight" && f.size() == 5) {
                    door_weights_[std::stoi(f[1])][f[2]] = {std::stod(f[3]), std::stod(f[4])};
                } else {
                    throw std::runtime_error("policy bundle: bad readiness row " + row);
                }
            }
        }
        else throw std::runtime_error("policy bundle: unknown entry " + key);
    }
    if (!net_) throw std::runtime_error("policy bundle without a combat network: " + weights);
}

Actor::Decision Actor::Decide(Observation& obs) {
    filter_.Apply(obs);
    if (obs.legal.empty()) throw GameError("actor: a decision without a legal action");
    Decision d;
    d.rows = obs.legal.size();
    d.forced = d.rows == 1;
    const bool outside = obs.global[kGInCombat] == 0;
    // The turn cap: count this actor's decisions within one combat turn.
    if (outside || obs.global[kGTurn] != turn_) {
        turn_ = outside ? -1 : obs.global[kGTurn];
        turn_decisions_ = 0;
    }
    if (!outside && !d.forced && turn_cap_ > 0 && turn_decisions_ >= turn_cap_) {
        for (size_t i = 0; i < obs.legal.size(); ++i) {
            if (obs.legal[i].kind != Action::EndTurn) continue;
            d.row = i;
            d.capped = true;
            break;
        }
    }
    if (!outside) ++turn_decisions_;
    std::vector<float> acting;      // the acting policy's logits, for the guards
    if (d.capped) {
        // nothing to compute: the turn ends
    } else if (!d.forced && outside && strategy_) {
        // The strategic rule: the base policy's most probable action, unless the value network
        // rates another action more than the margin above it.
        std::vector<float> logits, values;
        float unused = 0;
        (base_ ? *base_ : *net_).Forward(obs, logits, d.value);
        size_t base_row = 0, best = 0;
        for (size_t i = 1; i < logits.size(); ++i) if (logits[i] > logits[base_row]) base_row = i;
        strategy_->Forward(obs, values, unused);
        for (size_t i = 1; i < values.size(); ++i) if (values[i] > values[best]) best = i;
        d.replaced = best != base_row && values[best] - values[base_row] > margin_;
        d.row = d.replaced ? best : base_row;
        if (!table_.empty()) {
            d.row = TableRow(obs, values, d.row);
            d.replaced = d.row != base_row;
        }
        if (!doors_.empty()) {
            d.row = ReadinessRow(obs, values, d.row);
            d.replaced = d.row != base_row;
        }
        acting = std::move(logits);
    } else if (!d.forced) {
        std::vector<float> logits;
        (outside && base_ ? *base_ : *net_).Forward(obs, logits, d.value);
        float top = logits[0];
        for (float x : logits) top = std::max(top, x);
        std::vector<double> p(logits.size());
        double total = 0;
        for (size_t i = 0; i < logits.size(); ++i) { p[i] = std::exp(static_cast<double>(logits[i] - top)); total += p[i]; }
        if (greedy_) {
            for (size_t i = 1; i < p.size(); ++i) if (p[i] > p[d.row]) d.row = i;
        } else {
            // One uniform draw per decision; rows are in the observation's canonical order.
            double u = std::uniform_real_distribution<double>(0.0, total)(rng_);
            d.row = p.size() - 1;
            for (size_t i = 0; i < p.size(); ++i) { if (u < p[i]) { d.row = i; break; } u -= p[i]; }
        }
        d.prob = p[d.row] / total;
        if (greedy_ && !outside && combat_value_) {
            // python/nosl/strategy_q.py (Chooser): among the policy's four most probable actions, the one with the
            // highest predicted advantage over the policy's own replaces it when that advantage is above the margin.
            std::vector<size_t> order(logits.size());
            for (size_t i = 0; i < order.size(); ++i) order[i] = i;
            std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return logits[a] > logits[b]; });
            std::vector<float> scores;
            float unused = 0;
            combat_value_->Forward(obs, scores, unused);
            size_t best = order[0];
            float best_advantage = 0;
            for (size_t k = 1; k < order.size() && k < 4; ++k) {
                const float advantage = scores[order[k]] - scores[order[0]];
                if (advantage > best_advantage) { best_advantage = advantage; best = order[k]; }
            }
            if (best_advantage > combat_margin_) d.row = best;
        }
        acting = std::move(logits);
    }
    if ((guards_.any || guards_low_.any) && !acting.empty()) d.row = GuardRow(obs, d.row, acting);
    d.action = obs.legal[d.row];
    filter_.OnAction(d.action);
    if (d.action.kind == Action::EndTurn) turn_decisions_ = 0;
    return d;
}

namespace {

// python/nosl/contexts.py (Describer), for the two kinds of decision this actor's rules judge: rest sites and
// nested choices. The strings must be the Python ones: tools/nosl/nosl_actor_check compares the two. False for
// any other decision and for one with a single action.
bool Describe(const Observation& obs, std::string& context, std::vector<std::string>& labels) {
    static const char* const kActionNames[] = {"StartRun", "MapMove", "PlayCard", "EndTurn", "TakeReward", "Proceed", "EventOption",
                                               "RestOption", "ShopBuy", "TreasurePick", "UsePotion", "DiscardPotion", "Answer", "Resume"};
    static const char* const kRest[] = {"Heal", "Smith", "Mend", "Lift", "Dig", "Cook", "Clone", "Hatch", "Kindle"};
    static const char* const kChoiceOption[] = {"Card", "Bundle", "Reward", "Alternative", "Proceed"};
    static const char* const kKnownText[] = {"CardSelectCmd", "CardReward", "rewards", "take a reward, or proceed", "FromSimpleGridForRewards",
                                             "index", "count", "FromDeckGeneric", "FromDeckForEnchantment", "FromDeckForUpgrade",
                                             "FromDeckForTransformation", "FromDeckForRemoval", "FromChooseACardScreen",
                                             "FromChooseABundleScreen", "bundle", "crystal_sphere", "CRYSTAL_SPHERE.minigame"};
    const size_t count = obs.action_count();
    if (count < 2) return false;
    const auto& g = obs.global;
    auto token = [&](size_t row) -> const int32_t* {
        const int32_t t = obs.actions[row * kActionWidth + kActToken];
        return t >= 0 ? &obs.tokens[static_cast<size_t>(t) * kTokenWidth] : nullptr;
    };
    auto action_name = [&](size_t row) { return std::string(kActionNames[obs.actions[row * kActionWidth + kActKind]]); };
    labels.assign(count, std::string());
    if (g[kGDecision] == static_cast<int32_t>(DecisionKind::RestSite)) {
        std::vector<std::string> menu;
        for (size_t row = 0; row < count; ++row) {
            const int32_t* t = token(row);
            labels[row] = action_name(row);
            if (t && labels[row] == "RestOption") {
                const int32_t option = t[kColMiscA] - misc::kRestOption;
                labels[row] = option >= 0 && option < 9 ? kRest[option] : "?";
            }
            menu.push_back(labels[row]);
        }
        std::sort(menu.begin(), menu.end());
        menu.erase(std::unique(menu.begin(), menu.end()), menu.end());
        const double hp = static_cast<double>(g[kGHp]) / std::max(1, g[kGMaxHp]);
        context = "rest A" + std::to_string(g[kGAscension]) + " act" + std::to_string(g[kGAct] + 1) + " " +
                  (hp < 0.5 ? "hp<50%" : hp < 0.8 ? "hp 50-80%" : "hp>=80%") + " offering ";
        for (size_t i = 0; i < menu.size(); ++i) context += (i ? "|" : "") + menu[i];
    } else if (g[kGDecision] == static_cast<int32_t>(DecisionKind::Choice)) {
        auto text = [&](int32_t bucket) {
            for (const char* known : kKnownText) {   // nosl/observation.cpp Bucket(text)
                uint32_t hash = 2166136261u;
                for (const char* c = known; *c; ++c) hash = (hash ^ static_cast<unsigned char>(*c)) * 16777619u;
                if (misc::kHashed + static_cast<int32_t>(hash % static_cast<uint32_t>(misc::kHashBuckets)) == bucket) return std::string(known);
            }
            return "#" + std::to_string(bucket);
        };
        context = "choice " + text(g[kGChoiceSource]) + "/" + text(g[kGChoicePrompt]);
        for (size_t row = 0; row < count; ++row) {
            const int32_t* t = token(row);
            if (action_name(row) != "Answer" || !t) {
                labels[row] = action_name(row) + " " + std::to_string(obs.actions[row * kActionWidth + kActNumber]);
                continue;
            }
            const int32_t option = t[kColMiscA] - misc::kChoiceKind;
            const std::string kind = option >= 0 && option < 5 ? kChoiceOption[option] : "?";
            if (kind != "Card") { labels[row] = kind; continue; }
            if (t[kColCard] > 0 && t[kColCard] < static_cast<int32_t>(kCardIdNames.size())) {
                labels[row] = std::string(kCardIdNames[static_cast<size_t>(t[kColCard])]);
                for (int32_t i = 0; i < t[kColN0]; ++i) labels[row] += "+";
            } else {
                labels[row] = "?";
            }
        }
    } else {
        return false;
    }
    return true;
}

}  // namespace

// nosl.strategy_q.table_override.
Actor::GuardSet Actor::ParseGuards(const std::string& list) {
    GuardSet s;
    std::istringstream items(list);
    std::string item;
    while (std::getline(items, item, ',')) {
        const size_t eq = item.find('=');
        const std::string name = item.substr(0, eq);
        if (name == "nodiscard") s.nodiscard = true;
        else if (name == "nobuyfull") s.nobuyfull = true;
        else if (name == "nopotionbuy") s.nopotionbuy = true;
        else if (name == "takerewards") s.takerewards = true;
        else if (name == "removefirst") s.removefirst = true;
        else if (name == "hold" && eq != std::string::npos) s.hold = std::stod(item.substr(eq + 1));
        else if (name == "smith" && eq != std::string::npos) s.smith = std::stod(item.substr(eq + 1));
        else throw std::runtime_error("policy bundle: unknown guard " + item);
        s.any = true;
    }
    return s;
}

size_t Actor::GuardRow(const Observation& obs, size_t chosen, const std::vector<float>& scores) const {
    // python/nosl/guards.py Guards.one(), statement for statement.
    constexpr int32_t kRewards = 2, kRest = 4, kShop = 5, kChoice = 7;                 // the decision kinds (contexts.py DECISIONS)
    constexpr int32_t kRoomElite = 2, kRoomBoss = 3, kRoomShop = 5;
    constexpr int32_t kGoldReward = misc::kRewardType + 2, kPotionReward = misc::kRewardType + 3, kRelicReward = misc::kRewardType + 4;
    constexpr int32_t kShopPotion = misc::kShopKind + 3, kShopRemoval = misc::kShopKind + 4;
    const size_t count = obs.action_count();
    if (count < 2) return chosen;
    const auto& g = obs.global;
    auto kind = [&](size_t row) { return obs.actions[row * kActionWidth + kActKind]; };
    auto token = [&](size_t row) -> const int32_t* {
        const int32_t t = obs.actions[row * kActionWidth + kActToken];
        return t >= 0 ? &obs.tokens[static_cast<size_t>(t) * kTokenWidth] : nullptr;
    };
    auto misc_of = [&](size_t row) { const int32_t* t = token(row); return t ? t[kColMiscA] : -1; };
    const int32_t decision = g[kGDecision], chosen_kind = kind(chosen);
    const bool full = g[kGPotionCount] >= g[kGPotionSlots];
    const GuardSet& s = g[kGAscension] <= guards_low_max_ ? guards_low_ : guards_;
    if (s.smith >= 0 && decision == kRest && static_cast<double>(g[kGHp]) >= s.smith * static_cast<double>(std::max(1, g[kGMaxHp])) &&
        !(chosen_kind == Action::RestOption && misc_of(chosen) == misc::kRestOption + 1)) {
        for (size_t row = 0; row < count; ++row) {
            const int32_t* t = token(row);
            if (kind(row) == Action::RestOption && t && t[kColMiscA] == misc::kRestOption + 1 && t[kColN1] != 0) return row;
        }
    }
    if (s.removefirst) {
        const int32_t strike = static_cast<int32_t>(CardId::STRIKE_SILENT), defend = static_cast<int32_t>(CardId::DEFEND_SILENT);
        if (decision == kShop && (chosen_kind != Action::ShopBuy || misc_of(chosen) != kShopRemoval)) {
            bool basic = false;
            for (size_t t = 0; t < obs.token_count() && !basic; ++t) {
                const int32_t* row = &obs.tokens[t * kTokenWidth];
                basic = row[kColType] == static_cast<int32_t>(TokenType::Deck) && (row[kColCard] == strike || row[kColCard] == defend) && row[kColN0] == 0;
            }
            if (basic)
                for (size_t row = 0; row < count; ++row)
                    if (kind(row) == Action::ShopBuy && misc_of(row) == kShopRemoval) return row;
        }
        uint32_t hash = 2166136261u;                                        // nosl/observation.cpp Bucket("FromDeckGeneric")
        for (const char* c = "FromDeckGeneric"; *c; ++c) hash = (hash ^ static_cast<uint8_t>(*c)) * 16777619u;
        const int32_t generic = misc::kHashed + static_cast<int32_t>(hash % static_cast<uint32_t>(misc::kHashBuckets));
        if (decision == kChoice && chosen_kind == Action::Answer && g[kGRoomType] == kRoomShop && g[kGChoiceSource] == generic) {
            for (int32_t card : {strike, defend}) {
                for (size_t row = 0; row < count; ++row) {
                    const int32_t* t = token(row);
                    if (kind(row) != Action::Answer || !t || t[kColCard] != card || t[kColN0] != 0) continue;
                    const int32_t* mine = token(chosen);
                    return !mine || mine[kColCard] != card || mine[kColN0] != 0 ? row : chosen;
                }
            }
        }
    }
    if (s.takerewards && decision == kRewards && chosen_kind == Action::Proceed) {
        for (int32_t wanted : {kGoldReward, kRelicReward, kPotionReward}) {
            if (wanted == kPotionReward && full) continue;
            for (size_t row = 0; row < count; ++row)
                if (kind(row) == Action::TakeReward && misc_of(row) == wanted) return row;
        }
    }
    std::vector<char> banned;
    auto ban = [&](int32_t what, bool potion_buys) {
        if (banned.empty()) banned.assign(count, 0);
        for (size_t row = 0; row < count; ++row)
            if (potion_buys ? kind(row) == Action::ShopBuy && misc_of(row) == kShopPotion : kind(row) == what) banned[row] = 1;
    };
    if (s.nodiscard && chosen_kind == Action::DiscardPotion) {
        ban(Action::DiscardPotion, false);
    } else if (chosen_kind == Action::ShopBuy && misc_of(chosen) == kShopPotion && (s.nopotionbuy || (s.nobuyfull && full))) {
        ban(0, true);
        if (s.nodiscard) ban(Action::DiscardPotion, false);
    } else if (s.hold >= 0 && chosen_kind == Action::UsePotion && g[kGRoomType] != kRoomElite && g[kGRoomType] != kRoomBoss && !full &&
               static_cast<double>(g[kGHp]) >= s.hold * static_cast<double>(std::max(1, g[kGMaxHp]))) {
        ban(Action::UsePotion, false);
    }
    if (banned.empty()) return chosen;
    size_t best = count;
    for (size_t row = 0; row < count; ++row)
        if (!banned[row] && (best == count || scores[row] > scores[best])) best = row;
    return best == count ? chosen : best;
}

size_t Actor::TableRow(const Observation& obs, const std::vector<float>& values, size_t chosen) const {
    std::string context;
    std::vector<std::string> labels;
    if (!Describe(obs, context, labels)) return chosen;
    const size_t count = labels.size();
    const auto entries = table_.find(context);
    if (entries == table_.end()) return chosen;
    std::vector<double> worth(count, 0.0);
    double top = 0;
    for (size_t row = 0; row < count; ++row) {
        const auto found = entries->second.find(labels[row]);
        if (found != entries->second.end()) worth[row] = found->second;
        if (row == 0 || worth[row] > top) top = worth[row];
    }
    if (top - worth[chosen] <= table_delta_) return chosen;
    size_t best = count;
    for (size_t row = 0; row < count; ++row) {
        if (worth[row] != top) continue;
        if (best == count || values[row] > values[best]) best = row;
    }
    return best;
}

// nosl.readiness (Readiness.ahead, Readiness.deltas, readiness_override), with the same arithmetic in the same order.
size_t Actor::ReadinessRow(const Observation& obs, const std::vector<float>& values, size_t chosen) const {
    std::string context;
    std::vector<std::string> labels;
    if (!Describe(obs, context, labels)) return chosen;
    const size_t count = labels.size();
    const auto& g = obs.global;
    enum class Kind { Cards, Upgrade, Remove, Rest } kind;
    if (g[kGDecision] == static_cast<int32_t>(DecisionKind::RestSite)) kind = Kind::Rest;
    else if (context == "choice CardSelectCmd/CardReward") kind = Kind::Cards;
    else if (context == "choice FromDeckForUpgrade/index") kind = Kind::Upgrade;
    else if (context == "choice FromDeckGeneric/index") kind = Kind::Remove;
    else return chosen;
    if (kind == Kind::Rest ? !scope_rest_ : kind == Kind::Cards ? !scope_cards_ : kind == Kind::Upgrade ? !scope_upgrade_ : !scope_remove_)
        return chosen;
    if (kind == Kind::Remove && g[kGRoomType] != static_cast<int32_t>(RoomType::Shop)) return chosen;

    // The doors ahead: (door, weight, boss or -1 when the door's boss is not the one on show).
    struct Ahead { const Door* door; double weight; int32_t boss; };
    std::vector<Ahead> doors;
    const int32_t act = g[kGAct], floor = g[kGTotalFloor], boss = g[kGBoss];
    if (!readiness_chain_) {
        for (const Door& d : doors_)
            if (d.act == act && d.which == "last" && d.bosses.count(boss)) { doors.push_back({&d, 1.0, boss}); break; }
    } else {
        const auto weights = door_weights_.find(g[kGAscension]);
        if (weights == door_weights_.end()) return chosen;
        double reach = 1.0;
        for (const Door& d : doors_) {
            if (d.act < act || (d.act == act && d.which == "first" && floor >= d.passes - 1)) continue;
            const auto [p, gain] = weights->second.at(d.name);
            reach *= p;
            doors.push_back({&d, reach * (1.0 - p) * gain, d.act == act && d.bosses.count(boss) ? boss : -1});
        }
    }
    if (doors.empty()) return chosen;

    std::vector<int32_t> order;                                          // deck cards in token order
    std::unordered_map<int32_t, std::pair<int32_t, int32_t>> deck;       // card -> (copies, upgraded copies)
    for (size_t t = 0; t < obs.token_count(); ++t) {
        const int32_t* token = &obs.tokens[t * kTokenWidth];
        if (token[kColType] != static_cast<int32_t>(TokenType::Deck)) continue;
        auto [entry, fresh] = deck.try_emplace(token[kColCard], 0, 0);
        if (fresh) order.push_back(token[kColCard]);
        entry->second.first += token[kColN1];
        if (token[kColN0] > 0) entry->second.second += token[kColN1];
    }
    auto held = [&](int32_t card) {
        const auto found = deck.find(card);
        return found == deck.end() ? std::pair<int32_t, int32_t>{0, 0} : found->second;
    };
    auto with_boss = [](const Ahead& a, int32_t card) {
        if (a.boss < 0) {
            const auto mean = a.door->card_boss_mean.find(card);
            return mean == a.door->card_boss_mean.end() ? 0.0 : mean->second;
        }
        const auto of_boss = a.door->card_boss.find(a.boss);
        if (of_boss == a.door->card_boss.end()) return 0.0;
        const auto value = of_boss->second.find(card);
        return value == of_boss->second.end() ? 0.0 : value->second;
    };
    auto copy = [&](int32_t card, bool upgraded) {
        const auto [n, u] = held(card);
        double total = 0.0;
        for (const Ahead& a : doors) {
            const auto c = a.door->card.find(card);
            if (c == a.door->card.end()) continue;
            double value = n < 5 ? c->second[static_cast<size_t>(n)] : 0.0;
            if (upgraded) value += u < 3 ? c->second[5 + static_cast<size_t>(u)] : 0.0;
            if (n == 0) value += with_boss(a, card);
            total += a.weight * value;
        }
        return total;
    };
    auto upgrade = [&](int32_t card) {
        const auto [n, u] = held(card);
        (void)n;
        double total = 0.0;
        for (const Ahead& a : doors) {
            const auto c = a.door->card.find(card);
            if (c != a.door->card.end() && u < 3) total += a.weight * c->second[5 + static_cast<size_t>(u)];
        }
        return total;
    };
    auto removal = [&](int32_t card, bool upgraded) {
        const auto [n, u] = held(card);
        double total = 0.0;
        for (const Ahead& a : doors) {
            const auto c = a.door->card.find(card);
            if (c == a.door->card.end() || n == 0) continue;
            double value = n <= 5 ? -c->second[static_cast<size_t>(n - 1)] : 0.0;
            if (upgraded && u >= 1 && u <= 3) value -= c->second[5 + static_cast<size_t>(u - 1)];
            if (n == 1) value -= with_boss(a, card);
            total += a.weight * value;
        }
        return total;
    };

    const double nan = std::nan("");
    std::vector<double> dr(count, nan);
    bool any = false;
    for (size_t row = 0; row < count; ++row) {
        const int32_t action = obs.actions[row * kActionWidth + kActKind];
        const int32_t t = obs.actions[row * kActionWidth + kActToken];
        if (t < 0) continue;
        const int32_t* token = &obs.tokens[static_cast<size_t>(t) * kTokenWidth];
        if (kind == Kind::Rest) {
            if (action != 7) continue;                                    // Action::RestOption
            const int32_t option = token[kColMiscA] - misc::kRestOption;
            if (option == 0) {                                            // Heal: 30% of the maximum, capped by what is missing
                const double missing = 1.0 - static_cast<double>(g[kGHp]) / std::max(1, g[kGMaxHp]);
                double sum = 0.0;
                for (const Ahead& a : doors) sum += a.weight * a.door->hp10;
                dr[row] = sum * 10 * std::min(0.3, missing);
            } else if (option == 1) {                                     // Smith: the best upgrade the deck offers
                double best = 0.0;
                for (int32_t card : order) {
                    const auto [n, u] = held(card);
                    if (n > u) best = std::max(best, upgrade(card));
                }
                dr[row] = best;
            } else {
                continue;
            }
        } else {
            if (action != 12) continue;                                   // Action::Answer
            if (token[kColMiscA] - misc::kChoiceKind != 0) {              // not a card: skipping a reward
                if (kind != Kind::Cards) continue;
                dr[row] = 0.0;
            } else {
                const int32_t card = token[kColCard];
                const bool upgraded = token[kColN0] > 0;
                dr[row] = kind == Kind::Cards ? copy(card, upgraded) : kind == Kind::Upgrade ? upgrade(card) : removal(card, upgraded);
            }
        }
        any = true;
    }
    if (!any) return chosen;

    const auto entries = readiness_table_ ? table_.find(context) : table_.end();
    const double lam = readiness_table_ ? readiness_lambda_ : 1.0;
    std::vector<double> worth(count, nan);
    double top = nan;
    for (size_t row = 0; row < count; ++row) {
        if (std::isnan(dr[row])) continue;
        double of_table = 0.0;
        if (entries != table_.end()) {
            const auto found = entries->second.find(labels[row]);
            if (found != entries->second.end()) of_table = found->second;
        }
        worth[row] = lam * dr[row] + of_table;
        if (std::isnan(top) || worth[row] > top) top = worth[row];
    }
    if (std::isnan(worth[chosen])) return chosen;
    if (top - worth[chosen] <= table_delta_) return chosen;
    size_t best = count;
    for (size_t row = 0; row < count; ++row) {
        if (worth[row] != top) continue;
        if (best == count || values[row] > values[best]) best = row;
    }
    return best;
}

}  // namespace sts2::nosl
