// Replays golden vectors produced by tools/rng_golden (the game's own C# code)
// against sts2sim/core and reports every mismatch.
#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

#include "core/dec.h"
#include "core/dotnet_sort.h"
#include "core/rng.h"
#include "core/xxhash64.h"

using namespace sts2;

namespace {

struct Suite {
    const char* name;
    long checks = 0;
    long failures = 0;

    void check(bool ok, const std::string& line, const std::string& got) {
        ++checks;
        if (!ok) {
            if (failures < (getenv("MAXFAIL") ? atoi(getenv("MAXFAIL")) : 10)) std::printf("  [%s] MISMATCH: %s\n      got: %s\n", name, line.c_str(), got.c_str());
            ++failures;
        }
    }
    bool report() const {
        std::printf("%-8s %8ld checks, %ld failures\n", name, checks, failures);
        return failures == 0;
    }
};

std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream in(s);
    std::string t;
    while (in >> t) out.push_back(t);
    return out;
}

std::string unhex(const std::string& h) {
    std::string s;
    for (size_t i = 0; i + 1 < h.size(); i += 2) s.push_back(static_cast<char>(std::stoi(h.substr(i, 2), nullptr, 16)));
    return s;
}

uint64_t u64(const std::string& s) { return std::stoull(s); }
int64_t i64(const std::string& s) { return std::stoll(s); }
float fbits(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
double dbits(const std::string& s) { return std::bit_cast<double>(static_cast<uint64_t>(std::stoull(s, nullptr, 16))); }
std::string hexf(float f) {
    char b[16];
    std::snprintf(b, sizeof b, "%08x", std::bit_cast<uint32_t>(f));
    return b;
}
std::string hexd(double d) {
    char b[24];
    std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(std::bit_cast<uint64_t>(d)));
    return b;
}

Dec parse_dec(const std::string& s) {
    uint32_t w[4];
    std::sscanf(s.c_str(), "%x:%x:%x:%x", &w[0], &w[1], &w[2], &w[3]);
    return Dec::from_bits(w[0], w[1], w[2], w[3]);
}

std::string fmt_dec(const Dec& d) {
    char b[48];
    std::snprintf(b, sizeof b, "%08x:%08x:%08x:%08x", d.lo, d.mid, d.hi, d.flags);
    return b;
}

template <class F>
void for_lines(const std::string& path, F&& f) {
    std::ifstream in(path);
    if (!in) {
        std::printf("cannot open %s\n", path.c_str());
        std::exit(2);
    }
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty()) f(line, split(line));
    }
}

bool test_hash(const std::string& dir) {
    Suite s{"hash"};
    for_lines(dir + "/hash.txt", [&](const std::string& line, const std::vector<std::string>& t) {
        const std::string& op = t[0];
        if (op == "hash") {
            std::string str = t.size() == 3 ? unhex(t[1]) : "";
            uint64_t want = u64(t.back());
            uint64_t got = deterministic_hash(str);
            s.check(got == want, line, std::to_string(got));
        } else if (op == "oldhash") {
            std::string str = t.size() == 3 ? unhex(t[1]) : "";
            bool ascii = true;
            for (char c : str) ascii &= static_cast<unsigned char>(c) < 128;
            if (!ascii) return;  // legacy hash is only ported for ASCII seeds
            int32_t got = deterministic_hash_old(str);
            s.check(got == static_cast<int32_t>(i64(t.back())), line, std::to_string(got));
        } else if (op == "runname") {
            auto got = kRunRngNames[std::stoul(t[1])];
            s.check(got == t[2], line, std::string(got));
        } else if (op == "playername") {
            auto got = kPlayerRngNames[std::stoul(t[1])];
            s.check(got == t[2], line, std::string(got));
        } else if (op == "canon") {
            std::string got = canonicalize_seed(unhex(t[1]));
            std::string want = t.size() == 3 ? unhex(t[2]) : "";
            s.check(got == want, line, got);
        } else if (op == "runseed") {
            uint64_t got = run_seed_from_string(unhex(t[1]));
            s.check(got == u64(t[2]), line, std::to_string(got));
        } else if (op == "stream") {
            RunRngSet set(u64(t[1]));
            const Rng& r = set[static_cast<RunRngType>(std::stoul(t[2]))];
            bool ok = r.random.s0 == u64(t[3]) && r.random.s1 == u64(t[4]) && r.random.s2 == u64(t[5]) &&
                      r.random.s3 == u64(t[6]);
            s.check(ok, line, std::to_string(r.random.s0));
        } else if (op == "pstream") {
            PlayerRngSet set = PlayerRngSet::for_player(unhex(t[1]), std::stoi(t[2]));
            const Rng& r = set[static_cast<PlayerRngType>(std::stoul(t[3]))];
            bool ok = r.random.s0 == u64(t[4]) && r.random.s1 == u64(t[5]) && r.random.s2 == u64(t[6]) &&
                      r.random.s3 == u64(t[7]);
            s.check(ok, line, std::to_string(r.random.s0));
        } else if (op == "content") {
            Rng r = Rng::for_content(u64(t[1]), std::stoi(t[2]), unhex(t[3]), u64(t[4]));
            bool ok = r.random.s0 == u64(t[5]) && r.random.s1 == u64(t[6]) && r.random.s2 == u64(t[7]) &&
                      r.random.s3 == u64(t[8]);
            s.check(ok, line, std::to_string(r.random.s0));
        }
    });
    return s.report();
}

struct Item {
    int key;
    int id;
};

bool test_rng(const std::string& dir) {
    Suite s{"rng"};
    Rng rng;
    for_lines(dir + "/rng.txt", [&](const std::string& line, const std::vector<std::string>& t) {
        const std::string& op = t[0];
        auto eq = std::find(t.begin(), t.end(), "=");
        std::vector<std::string> args(t.begin() + 1, eq);
        std::vector<std::string> want(eq == t.end() ? t.end() : eq + 1, t.end());

        if (op == "seed") {
            rng = Rng(u64(t[1]));
        } else if (op == "state") {
            bool ok = rng.random.s0 == u64(t[1]) && rng.random.s1 == u64(t[2]) && rng.random.s2 == u64(t[3]) &&
                      rng.random.s3 == u64(t[4]) && rng.counter == std::stoi(t[5]);
            s.check(ok, line, std::to_string(rng.random.s0) + " counter " + std::to_string(rng.counter));
        } else if (op == "named") {
            Rng r(u64(t[1]), unhex(t[2]));
            bool ok = r.random.s0 == u64(t[3]) && r.random.s1 == u64(t[4]) && r.random.s2 == u64(t[5]) &&
                      r.random.s3 == u64(t[6]);
            s.check(ok, line, std::to_string(r.random.s0));
        } else if (op == "ulong") {
            auto got = rng.next_unsigned_long();
            s.check(got == u64(want[0]), line, std::to_string(got));
        } else if (op == "int1") {
            auto got = rng.next_int(std::stoi(args[0]));
            s.check(got == std::stoi(want[0]), line, std::to_string(got));
        } else if (op == "int2") {
            auto got = rng.next_int(std::stoi(args[0]), std::stoi(args[1]));
            s.check(got == std::stoi(want[0]), line, std::to_string(got));
        } else if (op == "bool") {
            auto got = rng.next_bool();
            s.check(got == (want[0] == "1"), line, std::to_string(got));
        } else if (op == "float0") {
            auto got = rng.next_float();
            s.check(hexf(got) == want[0], line, hexf(got));
        } else if (op == "float1") {
            auto got = rng.next_float(fbits(args[0]));
            s.check(hexf(got) == want[0], line, hexf(got));
        } else if (op == "float2") {
            auto got = rng.next_float(fbits(args[0]), fbits(args[1]));
            s.check(hexf(got) == want[0], line, hexf(got));
        } else if (op == "double0") {
            auto got = rng.next_double();
            s.check(hexd(got) == want[0], line, hexd(got));
        } else if (op == "double2") {
            auto got = rng.next_double(dbits(args[0]), dbits(args[1]));
            s.check(hexd(got) == want[0], line, hexd(got));
        } else if (op == "uint1") {
            auto got = rng.next_unsigned_int(static_cast<uint32_t>(std::stoul(args[0])));
            s.check(got == std::stoul(want[0]), line, std::to_string(got));
        } else if (op == "uint2") {
            auto got = rng.next_unsigned_int(static_cast<uint32_t>(std::stoul(args[0])),
                                             static_cast<uint32_t>(std::stoul(args[1])));
            s.check(got == std::stoul(want[0]), line, std::to_string(got));
        } else if (op == "ulong1") {
            auto got = rng.next_unsigned_long(u64(args[0]));
            s.check(got == u64(want[0]), line, std::to_string(got));
        } else if (op == "ulong2") {
            auto got = rng.next_unsigned_long(u64(args[0]), u64(args[1]));
            s.check(got == u64(want[0]), line, std::to_string(got));
        } else if (op == "gint") {
            auto got = rng.next_gaussian_int(std::stoi(args[0]), std::stoi(args[1]), std::stoi(args[2]),
                                             std::stoi(args[3]));
            s.check(got == std::stoi(want[0]), line, std::to_string(got));
        } else if (op == "gdouble") {
            auto got = rng.next_gaussian_double(dbits(args[0]), dbits(args[1]), dbits(args[2]), dbits(args[3]));
            s.check(hexd(got) == want[0], line, hexd(got));
        } else if (op == "gfloat") {
            auto got = rng.next_gaussian_float(fbits(args[0]), fbits(args[1]));
            s.check(hexf(got) == want[0], line, hexf(got));
        } else if (op == "item") {
            auto got = rng.next_item_index(std::stoi(args[0]));
            s.check(got == std::stoi(want[0]), line, std::to_string(got));
        } else if (op == "weighted") {
            std::vector<float> w;
            for (size_t i = 1; i < args.size(); ++i) w.push_back(fbits(args[i]));
            auto got = rng.weighted_next_index(w);
            s.check(got == std::stoi(want[0]), line, std::to_string(got));
        } else if (op == "shuffle" || op == "ushuffle") {
            int n = std::stoi(args[0]);
            std::vector<int> v(static_cast<size_t>(n));
            for (int i = 0; i < n; ++i) v[static_cast<size_t>(i)] = i;
            rng.shuffle(std::span<int>(v));
            std::string got;
            bool ok = static_cast<int>(want.size()) == n;
            for (int i = 0; i < n; ++i) {
                got += std::to_string(v[static_cast<size_t>(i)]) + " ";
                if (ok) ok = v[static_cast<size_t>(i)] == std::stoi(want[static_cast<size_t>(i)]);
            }
            s.check(ok, line, got);
        } else if (op == "sshuffle") {
            int n = std::stoi(args[0]);
            std::vector<Item> v;
            for (int i = 0; i < n; ++i) v.push_back({std::stoi(args[static_cast<size_t>(1 + i)]), i});
            dotnet_sort(std::span<Item>(v), [](const Item& a, const Item& b) { return (a.key > b.key) - (a.key < b.key); });
            rng.shuffle(std::span<Item>(v));
            std::string got;
            bool ok = static_cast<int>(want.size()) == n;
            for (int i = 0; i < n; ++i) {
                got += std::to_string(v[static_cast<size_t>(i)].id) + " ";
                if (ok) ok = v[static_cast<size_t>(i)].id == std::stoi(want[static_cast<size_t>(i)]);
            }
            s.check(ok, line, got);
        } else {
            s.check(false, line, "unknown op");
        }
    });
    return s.report();
}

bool test_sort(const std::string& dir) {
    Suite s{"sort"};
    for_lines(dir + "/sort.txt", [&](const std::string& line, const std::vector<std::string>& t) {
        int n = std::stoi(t[1]);
        std::vector<Item> v;
        for (int i = 0; i < n; ++i) v.push_back({std::stoi(t[static_cast<size_t>(2 + i)]), i});
        dotnet_sort(std::span<Item>(v), [](const Item& a, const Item& b) { return (a.key > b.key) - (a.key < b.key); });
        size_t w = static_cast<size_t>(2 + n + 1);
        bool ok = t.size() == w + static_cast<size_t>(n);
        std::string got;
        for (int i = 0; i < n; ++i) {
            got += std::to_string(v[static_cast<size_t>(i)].id) + " ";
            if (ok) ok = v[static_cast<size_t>(i)].id == std::stoi(t[w + static_cast<size_t>(i)]);
        }
        s.check(ok, line.substr(0, 200), got.substr(0, 200));
    });
    return s.report();
}

std::string try_dec(const std::function<Dec()>& f) {
    try {
        return fmt_dec(f());
    } catch (const std::overflow_error&) {
        return "OVF";
    } catch (const std::domain_error&) {
        return "DIV0";
    }
}

bool test_decimal(const std::string& dir) {
    Suite s{"decimal"};
    for_lines(dir + "/decimal.txt", [&](const std::string& line, const std::vector<std::string>& t) {
        const std::string& op = t[0];
        const std::string& want = t.back();
        if (op == "fromfloat") {
            std::string got = try_dec([&] { return Dec::from_float(fbits(t[1])); });
            s.check(got == want, line, got);
            return;
        }
        Dec a = parse_dec(t[1]);
        std::string got;
        if (op == "add" || op == "sub" || op == "mul" || op == "div" || op == "cmp" || op == "max" || op == "min") {
            Dec b = parse_dec(t[2]);
            if (op == "add") got = try_dec([&] { return a + b; });
            if (op == "sub") got = try_dec([&] { return a - b; });
            if (op == "mul") got = try_dec([&] { return a * b; });
            if (op == "div") got = try_dec([&] { return a / b; });
            if (op == "cmp") got = std::to_string(compare(a, b));
            if (op == "max") got = fmt_dec(dec_max(a, b));
            if (op == "min") got = fmt_dec(dec_min(a, b));
        } else if (op == "toint") {
            try {
                got = std::to_string(a.to_int());
            } catch (const std::overflow_error&) {
                got = "OVF";
            }
        } else if (op == "floor") {
            got = fmt_dec(dec_floor(a));
        } else if (op == "ceil") {
            got = fmt_dec(dec_ceiling(a));
        } else if (op == "trunc") {
            got = fmt_dec(dec_truncate(a));
        } else if (op == "round") {
            got = fmt_dec(dec_round(a, std::stoi(t[2]), t[3] == "1" ? Midpoint::AwayFromZero : Midpoint::ToEven));
        } else if (op == "todouble") {
            got = hexd(a.to_double());
        } else if (op == "tofloat") {
            got = hexf(a.to_float());
        } else {
            got = "unknown op";
        }
        s.check(got == want, line, got);
    });
    return s.report();
}

}  // namespace

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "tests/golden";
    bool ok = true;
    ok &= test_hash(dir);
    ok &= test_rng(dir);
    ok &= test_sort(dir);
    ok &= test_decimal(dir);
    std::printf(ok ? "ALL PASSED\n" : "FAILURES\n");
    return ok ? 0 : 1;
}
