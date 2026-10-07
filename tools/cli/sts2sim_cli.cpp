// sts2sim command-line driver.
//
//   newrun --seed S --character C [--ascension N]
//   newrun --seeds S1,S2,... --characters C1,... [--ascensions 0,10,...]
//       Same JSON as `Sts2Oracle newrun` (one object per line in batch mode):
//       {"seed","character","ascension","save":<SerializableRun subset>,"maps":[...]}
//   combat --seed S --character C [--ascension N] --coord COL,ROW --actions A
//       Replays an oracle combat; see combat_cmd.cpp.
//   smoke --seeds S1,S2,... [--character C] [--ascension N] [--policy-seed P] [--no-neow 1]
//       Random agents play runs up to the act-1 boss; see smoke_cmd.cpp.
//   replay --seed S --character C [--ascension N]   (oracle run actions on stdin)
//       Replays an oracle run; see replay_cmd.cpp.
#include <cstdio>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/run_setup.h"
#include "engine/save_json.h"
#include "tools/cli/combat_cmd.h"
#include "tools/cli/replay_cmd.h"
#include "tools/cli/smoke_cmd.h"

using namespace sts2;

namespace {

std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

CharacterId parse_character(const std::string& s) {
    std::string up;
    for (char c : s) up += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    for (size_t i = 1; i < kCharacterIdNames.size(); ++i) {
        if (kCharacterIdNames[i] == up) return static_cast<CharacterId>(i);
    }
    throw std::invalid_argument("unknown character " + s);
}

std::string newrun_json(const std::string& seed, CharacterId character, int ascension) {
    static GameState g;  // large; keep off the stack
    new_run(g, NewRunOptions{.seed = seed, .character = character, .ascension = ascension});
    const RunData& run = g.run;
    JsonWriter w;
    w.begin_object();
    w.field("seed", run.seed_string());
    w.field("character", name(character));
    w.field("ascension", ascension);
    w.key("save");
    write_run_save(w, g);
    w.key("maps").begin_array();
    for (size_t i = 0; i < run.acts.size(); ++i) write_map(w, generate_map_for_act(run, static_cast<int>(i)));
    w.end_array();
    w.end_object();
    return w.str();
}

int cmd_newrun(std::map<std::string, std::string>& opts) {
    std::vector<std::string> seeds = split(opts.count("seeds") ? opts["seeds"] : opts["seed"]);
    std::vector<std::string> chars = split(opts.count("characters") ? opts["characters"]
                                                                   : (opts.count("character") ? opts["character"] : "IRONCLAD"));
    std::vector<std::string> ascs = split(opts.count("ascensions") ? opts["ascensions"]
                                                                  : (opts.count("ascension") ? opts["ascension"] : "0"));
    for (const std::string& s : seeds) {
        for (const std::string& c : chars) {
            for (const std::string& a : ascs) {
                std::string line = newrun_json(s, parse_character(c), std::stoi(a));
                std::fwrite(line.data(), 1, line.size(), stdout);
                std::fputc('\n', stdout);
            }
        }
    }
    return 0;
}

int cmd_combat(std::map<std::string, std::string>& opts) {
    std::vector<std::string> coord = split(opts.at("coord"));
    return run_combat_replay(opts.at("seed"), parse_character(opts.count("character") ? opts["character"] : "IRONCLAD"),
                             opts.count("ascension") ? std::stoi(opts["ascension"]) : 0, std::stoi(coord.at(0)),
                             std::stoi(coord.at(1)), opts.count("encounter") ? opts["encounter"] : "",
                             opts.count("actions") ? opts["actions"] : "",
                             opts.count("deck") ? opts["deck"] : "", opts.count("relics") ? opts["relics"] : "",
                             opts.count("take-special-rewards") && opts["take-special-rewards"] == "1",
                             opts.count("potion") ? opts["potion"] : "",
                             opts.count("starting-energy") ? std::stoi(opts["starting-energy"]) : -1,
                             opts.count("starting-stars") ? std::stoi(opts["starting-stars"]) : -1,
                             opts.count("potion-target") ? opts["potion-target"] : "player",
                             opts.count("potion-auto-death") && opts["potion-auto-death"] == "1",
                             opts.count("mad-science") ? opts["mad-science"] : "",
                             opts.count("snapshots") && opts["snapshots"] == "1");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: sts2sim_cli newrun --seed S --character C [--ascension N]\n");
        return 2;
    }
    std::string cmd = argv[1];
    std::map<std::string, std::string> opts;
    for (int i = 2; i + 1 < argc; i += 2) {
        std::string k = argv[i];
        if (k.rfind("--", 0) != 0) {
            std::fprintf(stderr, "bad argument %s\n", argv[i]);
            return 2;
        }
        opts[k.substr(2)] = argv[i + 1];
    }
    try {
        if (cmd == "newrun") return cmd_newrun(opts);
        if (cmd == "combat") return cmd_combat(opts);
        if (cmd == "smoke") return run_smoke(opts, parse_character);
        if (cmd == "replay") {
            return run_replay(opts.at("seed"), parse_character(opts.count("character") ? opts["character"] : "IRONCLAD"),
                              opts.count("ascension") ? std::stoi(opts["ascension"]) : 0,
                              opts.count("no-neow") && opts["no-neow"] != "0", opts.count("hp") ? std::stoi(opts["hp"]) : 0,
                              opts.count("deck") ? opts["deck"] : "", opts.count("relics") ? opts["relics"] : "",
                              opts.count("run-queries") && opts["run-queries"] != "0",
                              opts.count("stop") ? opts["stop"] : "",
                              opts.count("verify-snapshots") && opts["verify-snapshots"] != "0",
                              opts.count("snapshots") && opts["snapshots"] == "1");
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
    std::fprintf(stderr, "unknown command %s\n", cmd.c_str());
    return 2;
}
