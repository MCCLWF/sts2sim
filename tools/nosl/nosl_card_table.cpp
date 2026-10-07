// nosl_card_table: what is printed on each card, as a table the actor-critic
// can look up by card id and upgrade level.
//
//   nosl_card_table --out FILE.json
//
// The observation carries a card as its id and upgrade level. The numbers on
// the card face (cost, damage, block, how many cards it draws, the powers it
// applies) are public static rules, the same for every run, so they are
// supplied to the model as a constant table instead of being left to be
// learned from the id alone. Values come from the generated content tables
// (content/generated/tables.h), level 0 and level 1; a card without an
// upgrade repeats level 0. Changes a single card instance picked up during a
// run (a var override) are not in this table.
//
// Output: {"version":1,"levels":2,"numbers":[names],"flags":[names],
//          "cards":[{"id":N,"entry":"...","levels":[{"numbers":[...],"flags":[...],"powers":[p0,p1]},...]},...]}
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "content/generated/ids.h"
#include "content/generated/tables.h"
#include "engine/json.h"

using namespace sts2;

namespace {

const char* const kNumbers[] = {"energy_cost", "costs_x", "star_cost", "damage", "block", "repeat", "cards", "energy",
                                "heal", "hp_loss", "other", "power0_amount", "power1_amount", "power_count"};
constexpr int kTypes = 7, kRarities = 11, kTargets = 10, kKeywords = 8, kTags = 6;

}  // namespace

int main(int argc, char** argv) {
    std::map<std::string, std::string> opt;
    for (int i = 1; i + 1 < argc; i += 2) opt[std::string(argv[i]).substr(2)] = argv[i + 1];
    if (!opt.count("out")) { std::cerr << "nosl_card_table: --out is required\n"; return 2; }
    JsonWriter w;
    w.begin_object().field("version", 1).field("levels", 2);
    w.key("numbers").begin_array();
    for (const char* n : kNumbers) w.value(n);
    w.end_array().key("flags").begin_array();
    for (int i = 0; i < kTypes; ++i) w.value("type" + std::to_string(i));
    for (int i = 0; i < kRarities; ++i) w.value("rarity" + std::to_string(i));
    for (int i = 0; i < kTargets; ++i) w.value("target" + std::to_string(i));
    for (int i = 0; i < kKeywords; ++i) w.value("keyword" + std::to_string(i));
    for (int i = 0; i < kTags; ++i) w.value("tag" + std::to_string(i));
    w.end_array().key("cards").begin_array();
    for (size_t id = 1; id < kCardIdNames.size(); ++id) {
        const CardData& card = data(static_cast<CardId>(id));
        w.begin_object().field("id", static_cast<int32_t>(id)).field("entry", card.entry).key("levels").begin_array();
        for (size_t level = 0; level < 2; ++level) {
            const CardLevel& l = card.levels[std::min(level, card.levels.size() - 1)];
            double damage = 0, block = 0, repeat = 0, cards = 0, energy = 0, heal = 0, hp_loss = 0, other = 0;
            std::vector<std::pair<int32_t, double>> powers;
            for (size_t v = 0; v < card.vars.size(); ++v) {
                const double value = l.vars[v].to_double();
                switch (card.vars[v].type) {
                case VarType::DamageVar: case VarType::CalculatedDamageVar: case VarType::OstyDamageVar: damage += value; break;
                case VarType::BlockVar: case VarType::CalculatedBlockVar: block += value; break;
                case VarType::RepeatVar: repeat += value; break;
                case VarType::CardsVar: cards += value; break;
                case VarType::EnergyVar: energy += value; break;
                case VarType::HealVar: heal += value; break;
                case VarType::HpLossVar: hp_loss += value; break;
                case VarType::PowerVar: powers.emplace_back(static_cast<int32_t>(card.vars[v].power), value); break;
                case VarType::StringVar: break;
                default: other += value; break;
                }
            }
            w.begin_object().key("numbers").begin_array();
            w.value(static_cast<double>(l.energy_cost)).value(card.costs_x ? 1.0 : 0.0).value(static_cast<double>(card.star_cost))
                .value(damage).value(block).value(repeat).value(cards).value(energy).value(heal).value(hp_loss).value(other)
                .value(powers.size() > 0 ? powers[0].second : 0.0).value(powers.size() > 1 ? powers[1].second : 0.0)
                .value(static_cast<double>(powers.size()));
            w.end_array().key("flags").begin_array();
            auto one_hot = [&](int value, int size) { for (int i = 0; i < size; ++i) w.value(i == value ? 1 : 0); };
            auto bits = [&](uint64_t mask, int size) { for (int i = 0; i < size; ++i) w.value(static_cast<int32_t>(mask >> i & 1)); };
            one_hot(static_cast<int>(card.type), kTypes);
            one_hot(static_cast<int>(card.rarity), kRarities);
            one_hot(static_cast<int>(card.target), kTargets);
            bits(l.keywords, kKeywords);
            bits(card.tags, kTags);
            w.end_array().key("powers").begin_array()
                .value(powers.size() > 0 ? powers[0].first : 0).value(powers.size() > 1 ? powers[1].first : 0).end_array().end_object();
        }
        w.end_array().end_object();
    }
    w.end_array().end_object();
    std::ofstream out(opt["out"]);
    if (!out) { std::cerr << "nosl_card_table: cannot write " << opt["out"] << '\n'; return 2; }
    out << w.str() << '\n';
    std::cout << "{\"cards\":" << kCardIdNames.size() - 1 << "}" << std::endl;
    return 0;
}
