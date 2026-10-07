// nosl_script: an Env trajectory as an action script for `Sts2Oracle run --script`.
//
//   nosl_script --seed S --rows-file FILE --out SCRIPT.jsonl [--ascension 10] [--character SILENT] [--filter N]
//   nosl_script --seed S --env-actions-file FILE --out SCRIPT.jsonl     (raw Env actions: kind index target col row)
//
// FILE holds the sampler's action rows of one run (python/nosl/vec.py's
// history: the row taken at each policy decision), separated by commas or
// whitespace. The run is replayed with the sampler's own semantics: rows
// index the canonical action table of nosl::Observe, and a decision with one
// legal action is taken without a row.
//
// Every Env action becomes the oracle driver's action for the same player
// choice (the inverse of tools/cli/replay_cmd.cpp ApplyAction). The oracle
// driver cannot express everything the Env allows: leaving a rest site
// without resting. The script stops before the
// first such action and the summary says so.
//
// One summary object on stdout:
//   {"status":"complete"|"inexpressible"|"error","env_actions":N,"script_actions":M,
//    "end":"victory"|"death"|"running","reason":"..."}
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "engine/json.h"
#include "engine/sim/errors.h"
#include "game/All.h"
#include "nosl/actor_filter.h"
#include "nosl/observation.h"

using namespace sts2;

namespace {

struct Translator {
    std::vector<std::string> lines;
    std::optional<int32_t> subset_count;  // a card selection being answered index by index
    std::vector<int32_t> subset;

    void Line(JsonWriter& w) { lines.push_back(w.str()); }
    static JsonWriter Open(const char* kind) {
        JsonWriter w;
        w.begin_object().field("event", "action").field("kind", kind);
        return w;
    }
    void Indices(const std::vector<int32_t>& indices) {
        JsonWriter w = Open("card_select");
        w.key("indices").begin_array();
        for (int32_t i : indices) w.value(i);
        w.end_array().end_object();
        Line(w);
    }

    // A potion used or discarded on a screen outside combat: the oracle takes
    // it as that screen's action with a "potion" member. A completed ordinary
    // rest uses the following map screen; remaining MiniatureTent options
    // keep the current rest screen available for the next scripted action.
    std::string OutsideCombat(DecisionKind kind, const RestSiteState& rest, const char* type, const Action& a) {
        const char* screen = nullptr;
        switch (kind) {
        case DecisionKind::Map: screen = "map"; break;
        case DecisionKind::Rewards: screen = "rewards"; break;
        case DecisionKind::Event: screen = "event"; break;
        case DecisionKind::RestSite:
            screen = rest.last_chosen >= 0 && rest.proceed_enabled && rest.options.empty() ? "map" : "rest";
            break;
        case DecisionKind::Shop: screen = "shop"; break;
        case DecisionKind::Treasure: screen = "relic_select"; break;
        default: return "a potion action on a screen the oracle driver has no form for";
        }
        JsonWriter w = Open(screen);
        w.key("potion").begin_object().field("type", type).field("slot", a.index).end_object().end_object();
        Line(w);
        return "";
    }

    // Empty: translated (possibly to nothing). Otherwise why the oracle cannot take it.
    std::string Add(Env& env, const Action& a) {
        const Decision d = env.decision();
        const RestSiteState& rest = env.state().run.rest_site;
        auto target = [&](JsonWriter& w) {
            w.key("target");
            if (a.target < 0) w.null(); else w.value(a.target);
        };
        switch (a.kind) {
        case Action::MapMove: {
            JsonWriter w = Open("map");
            w.key("coord").begin_array().value(static_cast<int32_t>(a.coord.col)).value(static_cast<int32_t>(a.coord.row)).end_array().end_object();
            Line(w);
            return "";
        }
        case Action::PlayCard: {
            JsonWriter w = Open("combat");
            w.field("type", "play").field("hand_index", a.index);
            target(w);
            w.end_object();
            Line(w);
            return "";
        }
        case Action::EndTurn: {
            JsonWriter w = Open("combat");
            w.field("type", "end_turn").end_object();
            Line(w);
            return "";
        }
        case Action::UsePotion: {
            if (d.kind != DecisionKind::Combat) return OutsideCombat(d.kind, rest, "use", a);
            JsonWriter w = Open("combat");
            w.field("type", "use_potion").field("slot", a.index);
            target(w);
            w.end_object();
            Line(w);
            return "";
        }
        case Action::DiscardPotion: {
            if (d.kind != DecisionKind::Combat) return OutsideCombat(d.kind, rest, "discard", a);
            JsonWriter w = Open("combat");
            w.field("type", "discard_potion").field("slot", a.index).end_object();
            Line(w);
            return "";
        }
        case Action::TakeReward: {
            JsonWriter w = Open("rewards");
            w.field("index", a.index).end_object();
            Line(w);
            return "";
        }
        case Action::Proceed: {
            if (d.kind == DecisionKind::RestSite) {
                // The oracle driver leaves by itself after its one rest option.
                if (rest.last_chosen >= 0 && rest.proceed_enabled) return "";
                return "leaving a rest site without resting";
            }
            JsonWriter w = Open(d.kind == DecisionKind::Rewards ? "rewards" : d.kind == DecisionKind::Shop ? "shop" : "relic_select");
            w.field(d.kind == DecisionKind::Rewards ? "proceed" : d.kind == DecisionKind::Shop ? "leave" : "skip", true).end_object();
            Line(w);
            return "";
        }
        case Action::EventOption: {
            JsonWriter w = Open("event");
            w.field("index", a.index).end_object();
            Line(w);
            return "";
        }
        case Action::RestOption: {
            JsonWriter w = Open("rest");
            w.field("index", a.index).end_object();
            Line(w);
            return "";
        }
        case Action::ShopBuy: {
            JsonWriter w = Open("shop");
            w.field("index", a.index).end_object();
            Line(w);
            return "";
        }
        case Action::TreasurePick: {
            JsonWriter w = Open("relic_select");
            w.field("index", a.index).end_object();
            Line(w);
            return "";
        }
        case Action::Answer: {
            const ChoiceRequest& r = d.request;
            if (r.source == "rewards") {
                JsonWriter w = Open("rewards");
                if (a.index == r.option_count - 1) w.field("proceed", true); else w.field("index", a.index);
                w.end_object();
                Line(w);
                return "";
            }
            if (r.kind == ChoiceKind::Bundle || r.kind == ChoiceKind::Option) {
                JsonWriter w = Open(r.kind == ChoiceKind::Bundle ? "bundle_select" : "choice");
                w.field("index", a.index).end_object();
                Line(w);
                return "";
            }
            if (!r.subset) { Indices({a.index}); return ""; }
            // CardSelectCmd subset: the count, then each index; one oracle action in all.
            if (r.prompt == "count") {
                subset_count = a.index;
                subset.clear();
            } else {
                if (!subset_count) return "a subset index without its count";
                subset.push_back(a.index);
            }
            if (static_cast<int32_t>(subset.size()) == *subset_count) {
                Indices(subset);
                subset_count.reset();
            }
            return "";
        }
        default: return "an action the oracle driver has no form for";
        }
    }
};

}  // namespace

int main(int argc, char** argv) {
    std::map<std::string, std::string> opt;
    for (int i = 1; i + 1 < argc; i += 2) opt[std::string(argv[i]).substr(2)] = argv[i + 1];
    Translator translator;
    int64_t env_actions = 0;
    std::string status = "complete", reason, end = "running";
    try {
        if (!opt.count("seed") || !opt.count("out") || opt.count("rows-file") == opt.count("env-actions-file"))
            throw std::invalid_argument("--seed, --out and one of --rows-file / --env-actions-file are required");
        CharacterId character = CharacterId::SILENT;
        if (opt.count("character")) {
            character = CharacterId::NONE;
            for (size_t i = 1; i < kCharacterIdNames.size(); ++i)
                if (kCharacterIdNames[i] == opt["character"]) character = static_cast<CharacterId>(i);
            if (character == CharacterId::NONE) throw std::invalid_argument("unknown character");
        }
        const bool raw = opt.count("env-actions-file") != 0;  // "kind index target col row" per action, nothing forced
        std::ifstream file(raw ? opt["env-actions-file"] : opt["rows-file"]);
        if (!file) throw std::runtime_error("cannot read the action file");
        std::string text((std::istreambuf_iterator<char>(file)), {});
        for (char& c : text) if (c == ',' || c == '[' || c == ']') c = ' ';
        std::istringstream in(text);
        std::vector<int32_t> rows;
        for (int32_t row; in >> row;) rows.push_back(row);

        Env env;
        env.Reset(NewRunOptions{.seed = opt["seed"], .character = character,
                                .ascension = opt.count("ascension") ? std::stoi(opt["ascension"]) : 10});
        auto take = [&](const Action& action) {
            std::string why = translator.Add(env, action);
            if (!why.empty()) { status = "inexpressible"; reason = why; return false; }
            env.Step(action);
            ++env_actions;
            return true;
        };
        // As the sampler does (nosl/batch_api.cpp Settle): the actor's table is
        // the observation pruned by the actor filter, and a decision left with
        // one action is taken without a row.
        nosl::ActorFilter filter(opt.count("filter") ? std::stoi(opt["filter"]) : nosl::kActorFilterNone);
        nosl::Observation obs;
        auto forced = [&] {
            while (env.decision().kind != DecisionKind::Ended) {
                nosl::Observe(env, obs);
                filter.Apply(obs);
                if (obs.legal.size() != 1) return true;
                filter.OnAction(obs.legal[0]);
                if (!take(obs.legal[0])) return false;
            }
            return true;
        };
        if (raw) {
            if (rows.size() % 5 != 0) throw std::invalid_argument("env actions need five numbers each");
            bool ok = true;
            for (size_t i = 0; ok && i < rows.size(); i += 5) {
                Action action{.kind = static_cast<Action::Kind>(rows[i]),
                              .coord = MapCoord{static_cast<int8_t>(rows[i + 3]), static_cast<int8_t>(rows[i + 4])},
                              .index = rows[i + 1], .target = rows[i + 2]};
                ok = take(action);
            }
            rows.clear();
        }
        bool ok = raw || forced();
        for (size_t i = 0; ok && i < rows.size(); ++i) {
            if (env.decision().kind == DecisionKind::Ended) throw GameError("the run ended before its last action row");
            // `obs` is the pruned table forced() stopped at.
            if (rows[i] < 0 || static_cast<size_t>(rows[i]) >= obs.legal.size()) throw GameError("action row out of range");
            const Action action = obs.legal[static_cast<size_t>(rows[i])];
            filter.OnAction(action);
            ok = take(action) && forced();
        }
        if (env.decision().kind == DecisionKind::Ended) end = env.state().run.is_victory ? "victory" : "death";
        if (status == "complete" && translator.subset_count) { status = "inexpressible"; reason = "the rows end inside a card selection"; }
    } catch (const std::exception& e) {
        status = "error";
        reason = e.what();
    }
    if (opt.count("out")) {
        std::ofstream out(opt["out"]);
        for (const std::string& line : translator.lines) out << line << '\n';
    }
    JsonWriter w;
    w.begin_object().field("status", status).field("env_actions", env_actions)
        .field("script_actions", static_cast<int64_t>(translator.lines.size())).field("end", end).field("reason", reason).end_object();
    std::cout << w.str() << std::endl;
    return status == "error" ? 1 : 0;
}
