// Small strict parser for the integer-only snapshot wire grammar. Float and
// decimal values are encoded as integer bit patterns by the snapshot protocol.
#pragma once
#include <charconv>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include "engine/sim/errors.h"
#include "env/resource_limits.h"

namespace sts2::snapshot_detail {
struct Json {
    enum Kind { Null, Bool, Number, String, Array, Object } kind = Null;
    bool boolean = false;
    std::string text;
    std::vector<Json> array;
    std::map<std::string, Json, std::less<>> object;

    const Json& at(std::string_view key) const {
        if (kind != Object) throw GameError("snapshot: expected object");
        auto it = object.find(key);
        if (it == object.end()) throw GameError("snapshot: missing field " + std::string(key));
        return it->second;
    }
    template<class T> T integer() const {
        if (kind != Number) throw GameError("snapshot: expected integer");
        T result{};
        auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), result);
        if (error != std::errc{} || end != text.data() + text.size())
            throw GameError("snapshot: integer out of range");
        return result;
    }
    bool is_null() const { return kind == Null; }
};

class Parser {
public:
    explicit Parser(std::string_view source) : input(source) {
        if (input.size() > resource_limits().snapshot_bytes)
            throw CapacityError("snapshot JSON: configured document byte resource budget exceeded");
    }
    Json parse() {
        Json root = value(0); whitespace();
        if (pos != input.size()) fail("trailing input");
        return root;
    }
private:
    [[noreturn]] void fail(const char* reason) const {
        throw GameError("snapshot JSON at byte " + std::to_string(pos) + ": " + reason);
    }
    void whitespace() {
        while (pos < input.size() && (input[pos] == ' ' || input[pos] == '\n' || input[pos] == '\r' || input[pos] == '\t')) ++pos;
    }
    bool consume(char c) { whitespace(); if (pos < input.size() && input[pos] == c) { ++pos; return true; } return false; }
    void expect(char c) { if (!consume(c)) fail("unexpected delimiter"); }
    void literal(std::string_view word) {
        if (input.substr(pos, word.size()) != word) fail("invalid literal");
        pos += word.size();
    }
    uint32_t hex4() {
        uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            if (pos >= input.size()) fail("truncated unicode escape");
            char c = input[pos++];
            int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
            if (digit < 0) fail("invalid unicode escape");
            value = value * 16 + digit;
        }
        return value;
    }
    static void utf8(std::string& out, uint32_t c) {
        if (c < 0x80) out += static_cast<char>(c);
        else if (c < 0x800) { out += static_cast<char>(0xc0 | (c >> 6)); out += static_cast<char>(0x80 | (c & 63)); }
        else if (c < 0x10000) { out += static_cast<char>(0xe0 | (c >> 12)); out += static_cast<char>(0x80 | ((c >> 6) & 63)); out += static_cast<char>(0x80 | (c & 63)); }
        else { out += static_cast<char>(0xf0 | (c >> 18)); out += static_cast<char>(0x80 | ((c >> 12) & 63)); out += static_cast<char>(0x80 | ((c >> 6) & 63)); out += static_cast<char>(0x80 | (c & 63)); }
    }
    std::string string() {
        expect('"'); std::string out;
        while (pos < input.size()) {
            unsigned char c = input[pos++];
            if (c == '"') return out;
            if (c < 32) fail("unescaped control character");
            if (c != '\\') out += static_cast<char>(c);
            else {
                if (pos >= input.size()) fail("truncated escape");
                char escape = input[pos++];
                switch (escape) {
                case '"': case '\\': case '/': out += escape; break;
                case 'b': out += '\b'; break; case 'f': out += '\f'; break;
                case 'n': out += '\n'; break; case 'r': out += '\r'; break; case 't': out += '\t'; break;
                case 'u': {
                    uint32_t code = hex4();
                    if (code >= 0xd800 && code <= 0xdbff) {
                        if (input.substr(pos, 2) != "\\u") fail("missing low surrogate");
                        pos += 2; uint32_t low = hex4();
                        if (low < 0xdc00 || low > 0xdfff) fail("invalid low surrogate");
                        code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
                    } else if (code >= 0xdc00 && code <= 0xdfff) fail("unpaired low surrogate");
                    utf8(out, code); break;
                }
                default: fail("invalid escape");
                }
            }
        }
        fail("unterminated string");
    }
    Json value(unsigned depth) {
        whitespace();
        if (depth > 64) fail("document nesting limit");
        if (++nodes > resource_limits().json_nodes)
            throw CapacityError("snapshot JSON: configured node resource budget exceeded");
        if (pos >= input.size()) fail("missing value");
        Json out;
        const char c = input[pos];
        if (c == '{') {
            ++pos; out.kind = Json::Object;
            if (!consume('}')) do {
                std::string key = string(); expect(':'); Json child = value(depth + 1);
                if (!out.object.emplace(std::move(key), std::move(child)).second) fail("duplicate field");
                if (consume('}')) return out;
                expect(',');
            } while (true);
        } else if (c == '[') {
            ++pos; out.kind = Json::Array;
            if (!consume(']')) do {
                out.array.push_back(value(depth + 1));
                if (consume(']')) return out;
                expect(',');
            } while (true);
        } else if (c == '"') { out.kind = Json::String; out.text = string(); }
        else if (c == 'n') literal("null");
        else if (c == 't' || c == 'f') { out.kind = Json::Bool; out.boolean = c == 't'; literal(c == 't' ? "true" : "false"); }
        else if (c == '-' || (c >= '0' && c <= '9')) {
            out.kind = Json::Number; size_t start = pos;
            if (input[pos] == '-') ++pos;
            if (pos >= input.size() || input[pos] < '0' || input[pos] > '9') fail("invalid integer");
            if (input[pos] == '0') ++pos;
            else while (pos < input.size() && input[pos] >= '0' && input[pos] <= '9') ++pos;
            out.text = std::string(input.substr(start, pos - start));
        } else fail("invalid value");
        return out;
    }
    std::string_view input;
    size_t pos = 0, nodes = 0;
};
}  // namespace sts2::snapshot_detail
