// Consume a TSV produced only by MonsterHistoryChecks against sts2.dll.
// Deliberately outside CTest: this is a large differential component probe.
#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include "engine/sim/state.h"
using namespace sts2;
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("usage: monster_history_checks native.tsv");
        std::ifstream input(argv[1]);
        if (!input) throw std::runtime_error("cannot read native TSV");
        MonsterMoveHistory history;
        uint64_t queries = 0, appends = 0, boundaries = 0;
        int fixture = -1;
        char command;
        while (input >> command) {
            if (command == 'V') { std::string header; std::getline(input, header); }
            else if (command == 'R') { input >> fixture; history = {}; }
            else if (command == 'A') {
                int state, is_move; input >> state >> is_move;
                history.Append(static_cast<int8_t>(state), is_move != 0); ++appends;
            } else if (command == 'L') {
                uint32_t count; int last; input >> count >> last;
                if (count != history.count || last != history.last) throw std::runtime_error("native Last/Count mismatch");
                ++boundaries;
            } else if (command == 'Q') {
                int state, repeat, maximum, cooldown; int32_t expected;
                input >> state >> repeat >> maximum >> cooldown >> expected;
                float multiplier = 1.0f;
                auto type = static_cast<MoveRepeatType>(repeat);
                if (type == MoveRepeatType::UseOnlyOnce) multiplier = history.Contains(state) ? 0.0f : 1.0f;
                else if (type != MoveRepeatType::CanRepeatForever)
                    multiplier = history.RepeatMultiplier(state, type == MoveRepeatType::CannotRepeat ? 1.0f : static_cast<float>(maximum));
                float actual = history.InMoveCooldown(state, cooldown) ? 0.0f : multiplier * 0.4f;
                if (std::bit_cast<int32_t>(actual) != expected) {
                    std::cerr << "fixture=" << fixture << " count=" << history.count << " target=" << state
                        << " repeat=" << repeat << " maximum=" << maximum << " cooldown=" << cooldown << '\n';
                    throw std::runtime_error("native GetStateWeight mismatch");
                }
                ++queries;
            } else throw std::runtime_error("unknown native command");
            if (!input) throw std::runtime_error("malformed native row");
        }
        if (queries == 0 || appends <= 1024 || fixture != 3) throw std::runtime_error("incomplete native probe");
        std::cout << queries << " native weight queries, " << boundaries << " Last/Count boundaries, "
            << appends << " appends identical; history=" << sizeof(MonsterMoveHistory)
            << " state=" << sizeof(GameState) << " bytes\n";
        return 0;
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
