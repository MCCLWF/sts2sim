// Minimal streaming JSON writer (no dependencies). Floats use the shortest
// round-trip representation, matching System.Text.Json's output for float.
#pragma once

#include <charconv>
#include <cstdio>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sts2 {

class JsonWriter {
public:
    std::string& str() { return out_; }

    JsonWriter& begin_object() {
        value_prefix();
        out_ += '{';
        stack_.push_back(true);
        return *this;
    }
    JsonWriter& end_object() {
        out_ += '}';
        stack_.pop_back();
        return *this;
    }
    JsonWriter& begin_array() {
        value_prefix();
        out_ += '[';
        stack_.push_back(true);
        return *this;
    }
    JsonWriter& end_array() {
        out_ += ']';
        stack_.pop_back();
        return *this;
    }

    JsonWriter& key(std::string_view k) {
        if (!stack_.empty()) {
            if (!stack_.back()) out_ += ',';
            stack_.back() = false;
        }
        write_string(k);
        out_ += ':';
        after_key_ = true;
        return *this;
    }

    JsonWriter& value(std::string_view s) {
        value_prefix();
        write_string(s);
        return *this;
    }
    JsonWriter& value(const char* s) { return value(std::string_view(s)); }
    JsonWriter& value(bool b) {
        value_prefix();
        out_ += b ? "true" : "false";
        return *this;
    }
    // Embeds an already-serialized JSON document as the next value.
    JsonWriter& raw_json(const std::string& json) { return raw_number(json); }
    JsonWriter& value(int32_t v) { return raw_number(std::to_string(v)); }
    JsonWriter& value(uint32_t v) { return raw_number(std::to_string(v)); }
    JsonWriter& value(int64_t v) { return raw_number(std::to_string(v)); }
    JsonWriter& value(uint64_t v) { return raw_number(std::to_string(v)); }
    JsonWriter& value(float f) {
        char buf[32];
        auto res = std::to_chars(buf, buf + sizeof buf, f);
        return raw_number(std::string(buf, res.ptr));
    }
    JsonWriter& value(double d) {
        char buf[32];
        auto res = std::to_chars(buf, buf + sizeof buf, d);
        return raw_number(std::string(buf, res.ptr));
    }
    JsonWriter& null() {
        value_prefix();
        out_ += "null";
        return *this;
    }

    // Only for owned JSON produced by an internal encoder, never request text.
    JsonWriter& trusted_json(std::string_view json) {
        value_prefix();out_.append(json);return *this;
    }

    template <class T>
    JsonWriter& field(std::string_view k, const T& v) {
        key(k);
        return value(v);
    }

private:
    std::string out_;
    std::vector<bool> stack_;  // per container: "no element written yet"
    bool after_key_ = false;

    void value_prefix() {
        if (after_key_) {
            after_key_ = false;
            return;
        }
        if (!stack_.empty()) {
            if (!stack_.back()) out_ += ',';
            stack_.back() = false;
        }
    }

    JsonWriter& raw_number(const std::string& s) {
        value_prefix();
        out_ += s;
        return *this;
    }

    void write_string(std::string_view s) {
        out_ += '"';
        for (char c : s) {
            switch (c) {
                case '"': out_ += "\\\""; break;
                case '\\': out_ += "\\\\"; break;
                case '\n': out_ += "\\n"; break;
                case '\t': out_ += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8];
                        std::snprintf(buf, sizeof buf, "\\u%04x", c);
                        out_ += buf;
                    } else {
                        out_ += c;
                    }
            }
        }
        out_ += '"';
    }
};

// C# enum name -> System.Text.Json snake_case ("RestSite" -> "rest_site").
inline std::string snake_case(std::string_view name) {
    std::string out;
    for (size_t i = 0; i < name.size(); ++i) {
        char c = name[i];
        if (c >= 'A' && c <= 'Z') {
            if (i > 0) out += '_';
            out += static_cast<char>(c - 'A' + 'a');
        } else {
            out += c;
        }
    }
    return out;
}

}  // namespace sts2
