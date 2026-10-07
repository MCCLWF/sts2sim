// Offline labels from a qualified original-game action trace. All model
// fields are public Observe tables. A script decision may contain several
// genuine Env steps (subset count followed by ordered card choices).
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include "engine/json.h"
#include "game/All.h"
#include "nosl/actor_filter.h"
#include "tools/cli/oracle_actions.h"

using namespace sts2;
using namespace sts2::nosl;

int main(int argc, char** argv) {
    try {
        std::map<std::string, std::string> opt;
        for (int i = 1; i + 1 < argc; i += 2) opt[std::string(argv[i]).substr(2)] = argv[i + 1];
        if (argc % 2 == 0 || !opt.count("seed") || !opt.count("out"))
            throw std::invalid_argument("--seed S --out labels.jsonl [--begin-script N] [--filter 1]");
        if (opt["seed"].starts_with("NOSLDEV") || opt["seed"].starts_with("NOSLTEST"))
            throw std::invalid_argument("teacher export refuses evaluation seeds");
        const int begin = opt.count("begin-script") ? std::stoi(opt["begin-script"]) : 0;
        const int version = opt.count("filter") ? std::stoi(opt["filter"]) : kActorFilterV1;
        if (begin < 0 || (version != kActorFilterNone && version != kActorFilterV1))
            throw std::invalid_argument("bad begin/filter");
        std::vector<std::vector<std::string>> script;
        for (std::string line; std::getline(std::cin, line);) {
            std::istringstream in(line);
            std::vector<std::string> words;
            for (std::string word; in >> word;) words.push_back(word);
            if (!words.empty()) script.push_back(std::move(words));
        }
        if (static_cast<size_t>(begin) >= script.size()) throw std::invalid_argument("begin outside script");
        std::ofstream out(opt["out"]);
        if (!out) throw std::runtime_error("cannot create labels file");
        Env env;
        env.Reset(NewRunOptions{.seed = opt["seed"], .character = CharacterId::SILENT, .ascension = 10});
        ActorFilter filter(version);
        size_t script_index = 0;
        int64_t steps = 0, rows = 0, teacher_rows = 0, forced = 0, hidden_checks = 0, hidden_choice_skips = 0;
        auto step = [&](const Action& action) {
            Observation obs;
            Observe(env, obs);
            ActorFilter before_filter = filter;
            filter.Apply(obs);
            auto found = std::find(obs.legal.begin(), obs.legal.end(), action);
            if (found == obs.legal.end()) throw GameError("teacher action removed by actor filter at script " + std::to_string(script_index));
            int32_t label = static_cast<int32_t>(found - obs.legal.begin());
            // Independently restored full state must have the same successor.
            auto restored = Env::ImportSnapshotJson(env.ExportSnapshotJson());
            if (restored->Hash() != env.Hash()) throw GameError("export restore root differs");
            restored->Step(action);
            // Determinize deliberately refuses pending choice continuations;
            // do not change that contract or mutate a half-resolved effect.
            // Those choices still receive exact ABI/table/label replay checks.
            bool check_hidden = script_index >= static_cast<size_t>(begin) || steps % 16 == 0;
            const bool pending = env.decision().kind == DecisionKind::Choice || env.decision().kind == DecisionKind::Boundary;
            if (check_hidden && pending) ++hidden_choice_skips;
            if (check_hidden && !pending) {
                auto hidden = env.Clone();
                hidden->Determinize(0x4e51544fULL + steps);
                Observation changed;
                Observe(*hidden, changed);
                before_filter.Apply(changed);
                if (obs.global != changed.global || obs.tokens != changed.tokens || obs.actions != changed.actions)
                    throw GameError("teacher actor observation depends on hidden state");
                ++hidden_checks;
            }
            if (obs.legal.size() > 1) {
                JsonWriter w;
                w.begin_object().field("script_index", static_cast<int32_t>(script_index))
                    .field("step", steps).field("teacher", script_index >= static_cast<size_t>(begin))
                    .field("label", label).key("global").begin_array();
                for (auto v : obs.global) w.value(v);
                w.end_array().key("tokens").begin_array();
                for (auto v : obs.tokens) w.value(v);
                w.end_array().key("actions").begin_array();
                for (auto v : obs.actions) w.value(v);
                w.end_array().key("counts").begin_array()
                    .value(static_cast<int32_t>(obs.token_count())).value(static_cast<int32_t>(obs.action_count()))
                    .end_array().end_object();
                out << w.str() << '\n';
                ++rows;
                teacher_rows += script_index >= static_cast<size_t>(begin);
            } else ++forced;
            filter.OnAction(action);
            env.Step(action);
            if (restored->Hash() != env.Hash()) throw GameError("export restored successor differs");
            ++steps;
        };
        for (; script_index < script.size(); ++script_index)
            offline::ApplyOracleAction(env, script[script_index], step);
        out.close();
        JsonWriter summary;
        summary.begin_object().field("schema", kObsSchemaVersion).field("status", "exported")
            .field("script_actions", static_cast<int64_t>(script.size())).field("steps", steps)
            .field("rows", rows).field("teacher_rows", teacher_rows).field("forced", forced)
            .field("hidden_checks", hidden_checks).field("filter", version)
            .field("hidden_pending_choice_skips", hidden_choice_skips)
            .field("floor", env.state().run.total_floor).field("hp", Player(&env.sim(), 1).Creature().CurrentHp())
            .field("hash", std::to_string(env.Hash())).end_object();
        std::cout << summary.str() << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
