// players.cpp — hand-rolled minimal JSON parser for GET /nyx03dl/players.
//
// Written for Nyx03DL (M3). Same strict style as manifest.cpp: only the
// fixed schema is accepted; anything else (trailing garbage, wrong type,
// out-of-range id, duplicate key, bad escape) is rejected, never trusted.
// A rejected document leaves the previous player->skin mapping untouched
// (the monitor simply keeps polling with the old mapping).
#include "players.h"

#include <cctype>
#include <cstdint>

namespace nyx {
namespace {

class Parser {
   public:
    Parser(const std::string& text, std::string& err) : text_(text), err_(err) {}

    bool Parse(std::map<int, int>& out) {
        SkipWs();
        if (!Consume('{')) return Fail("expected '{' at start of players object");
        SkipWs();
        std::map<int, int> skins;
        if (Peek() == '}') {
            pos_++;
        } else {
            for (;;) {
                std::string key;
                if (!ParseString(key)) return false;
                if (!Expect(':', "':' after object key")) return false;
                uint32_t skinid = 0;
                if (!ParseUint32(skinid)) return false;
                int playerid = 0;
                if (!ParsePlayerId(key, playerid)) return false;
                if (skinid < 20000 || skinid > 30000)
                    return Fail("skinid outside 20000-30000");
                if (!skins.emplace(playerid, static_cast<int>(skinid)).second)
                    return Fail("duplicate playerid key in players object");
                SkipWs();
                if (Consume(',')) continue;
                if (Consume('}')) break;
                return Fail("expected ',' or '}' in players object");
            }
        }
        SkipWs();
        if (pos_ != text_.size()) return Fail("trailing data after players object");
        out.swap(skins);
        return true;
    }

   private:
    const std::string& text_;
    std::string& err_;
    size_t pos_ = 0;

    bool Fail(const std::string& msg) {
        if (err_.empty()) err_ = msg + " (offset " + std::to_string(pos_) + ")";
        return false;
    }

    char Peek() const { return pos_ < text_.size() ? text_[pos_] : '\0'; }

    void SkipWs() {
        while (pos_ < text_.size() &&
               (text_[pos_] == ' ' || text_[pos_] == '\t' || text_[pos_] == '\n' ||
                text_[pos_] == '\r'))
            pos_++;
    }

    bool Consume(char c) {
        SkipWs();
        if (Peek() == c) {
            pos_++;
            return true;
        }
        return false;
    }

    bool Expect(char c, const char* what) {
        if (!Consume(c)) return Fail(std::string("expected ") + what);
        return true;
    }

    // JSON string (same escapes as the manifest parser; keys are digits).
    bool ParseString(std::string& out) {
        SkipWs();
        if (Peek() != '"') return Fail("expected string");
        pos_++;
        std::string s;
        while (pos_ < text_.size()) {
            char c = text_[pos_++];
            if (c == '"') {
                out = s;
                return true;
            }
            if (c == '\\') {
                if (pos_ >= text_.size()) return Fail("dangling backslash in string");
                char e = text_[pos_++];
                switch (e) {
                    case '"': s += '"'; break;
                    case '\\': s += '\\'; break;
                    case '/': s += '/'; break;
                    case 'b': s += '\b'; break;
                    case 'f': s += '\f'; break;
                    case 'n': s += '\n'; break;
                    case 'r': s += '\r'; break;
                    case 't': s += '\t'; break;
                    case 'u': {
                        if (pos_ + 4 > text_.size()) return Fail("bad \\u escape");
                        unsigned code = 0;
                        for (int i = 0; i < 4; ++i) {
                            char h = text_[pos_++];
                            code <<= 4;
                            if (h >= '0' && h <= '9') code |= (unsigned)(h - '0');
                            else if (h >= 'a' && h <= 'f') code |= (unsigned)(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') code |= (unsigned)(h - 'A' + 10);
                            else return Fail("bad hex digit in \\u escape");
                        }
                        if (code > 0x7F) return Fail("\\u escape outside ASCII range");
                        s += static_cast<char>(code);
                        break;
                    }
                    default: return Fail("unsupported escape in string");
                }
            } else if (static_cast<unsigned char>(c) < 0x20) {
                return Fail("control character in string");
            } else {
                s += c;
            }
        }
        return Fail("unterminated string");
    }

    // Unsigned decimal integer (base 10 only). Rejects negatives, empty,
    // and > UINT32_MAX.
    bool ParseUint32(uint32_t& out) {
        SkipWs();
        if (Peek() == '-') return Fail("negative number where unsigned expected");
        if (!std::isdigit(static_cast<unsigned char>(Peek())))
            return Fail("expected number");
        unsigned long long v = 0;
        while (pos_ < text_.size() &&
               std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
            v = v * 10 + (unsigned)(text_[pos_] - '0');
            if (v > 0xFFFFFFFFull) return Fail("number exceeds uint32 range");
            pos_++;
        }
        out = static_cast<uint32_t>(v);
        return true;
    }

    // A playerid key must be a non-empty string of decimal digits naming
    // a SA-MP player id (0..999).
    bool ParsePlayerId(const std::string& key, int& out) {
        if (key.empty()) return Fail("empty playerid key");
        unsigned long long v = 0;
        for (char c : key) {
            if (!std::isdigit(static_cast<unsigned char>(c)))
                return Fail("playerid key is not a decimal number: " + key);
            v = v * 10 + (unsigned)(c - '0');
            if (v > 999) return Fail("playerid outside 0-999: " + key);
        }
        out = static_cast<int>(v);
        return true;
    }
};

}  // namespace

bool ParsePlayers(const std::string& json, std::map<int, int>& out,
                  std::string& err) {
    err.clear();
    Parser p(json, err);
    return p.Parse(out);
}

}  // namespace nyx
