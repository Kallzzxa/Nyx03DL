// manifest.cpp — hand-rolled minimal JSON parser for the Nyx03DL manifest.
//
// Written for Nyx03DL (M2). Only the fixed manifest schema is supported:
// a top-level array of objects with string + unsigned-decimal fields.
// Anything else (trailing garbage, missing field, wrong type, out-of-range
// number, bad escape, traversal file name) is rejected, never trusted.
#include "manifest.h"

#include <cctype>

namespace nyx {
namespace {

class Parser {
   public:
    Parser(const std::string& text, std::string& err) : text_(text), err_(err) {}

    bool Parse(std::vector<ManifestModel>& out) {
        SkipWs();
        if (!Consume('[')) return Fail("expected '[' at start of manifest");
        SkipWs();
        std::vector<ManifestModel> models;
        if (Peek() == ']') {
            pos_++;
        } else {
            for (;;) {
                ManifestModel m;
                if (!ParseObject(m)) return false;
                models.push_back(m);
                SkipWs();
                if (Consume(',')) continue;
                if (Consume(']')) break;
                return Fail("expected ',' or ']' in manifest array");
            }
        }
        SkipWs();
        if (pos_ != text_.size()) return Fail("trailing data after manifest array");
        out.swap(models);
        return true;
    }

    // v1.3.0: parse a vehicles array (GET /nyx03dl/vehicles). Same
    // envelope as the skin manifest, different object schema.
    bool ParseVehicles(std::vector<VehicleModel>& out) {
        SkipWs();
        if (!Consume('[')) return Fail("expected '[' at start of vehicles");
        SkipWs();
        std::vector<VehicleModel> models;
        if (Peek() == ']') {
            pos_++;
        } else {
            for (;;) {
                VehicleModel m;
                if (!ParseVehicleObject(m)) return false;
                models.push_back(m);
                SkipWs();
                if (Consume(',')) continue;
                if (Consume(']')) break;
                return Fail("expected ',' or ']' in vehicles array");
            }
        }
        SkipWs();
        if (pos_ != text_.size()) return Fail("trailing data after vehicles array");
        out.swap(models);
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

    // Parse a JSON string. Handles the simple escapes \" \\ \/ \b \f \n
    // \r \t and \uXXXX (only ASCII code points; file names are ASCII).
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

    // Parse an unsigned decimal integer (base 10 only — CRCs in the manifest
    // are decimal, never hex). Rejects negatives, empty, and > UINT32_MAX.
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

    // Parse a non-negative JSON number with an optional fraction
    // (e.g. 0, 220, 2.5). No exponent: the server emits handling numbers
    // with %g, which never uses exponents in the 0-500 range. Rejects
    // negatives and absurd magnitudes.
    bool ParseFloat(float& out) {
        SkipWs();
        if (Peek() == '-') return Fail("negative number where unsigned expected");
        if (!std::isdigit(static_cast<unsigned char>(Peek())))
            return Fail("expected number");
        double v = 0;
        while (pos_ < text_.size() &&
               std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
            v = v * 10 + (text_[pos_] - '0');
            if (v > 100000) return Fail("number exceeds sane range");
            pos_++;
        }
        if (Peek() == '.') {
            pos_++;
            if (!std::isdigit(static_cast<unsigned char>(Peek())))
                return Fail("expected digit after decimal point");
            double frac = 0, div = 1;
            while (pos_ < text_.size() &&
                   std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                frac = frac * 10 + (text_[pos_] - '0');
                div *= 10;
                pos_++;
            }
            v += frac / div;
        }
        out = static_cast<float>(v);
        return true;
    }

    bool ParseObject(ManifestModel& m) {
        if (!Expect('{', "'{'")) return false;
        bool seenSkin = false, seenBase = false, seenDff = false, seenTxd = false;
        bool seenDffCrc = false, seenTxdCrc = false, seenDffSize = false,
             seenTxdSize = false;
        SkipWs();
        if (Peek() == '}') {
            pos_++;
            return Fail("empty model object (all fields are required)");
        }
        for (;;) {
            std::string key;
            if (!ParseString(key)) return false;
            if (!Expect(':', "':' after object key")) return false;
            if (key == "skinid") {
                uint32_t v;
                if (!ParseUint32(v)) return false;
                m.skinid = static_cast<int>(v);
                seenSkin = true;
            } else if (key == "baseid") {
                uint32_t v;
                if (!ParseUint32(v)) return false;
                m.baseid = static_cast<int>(v);
                seenBase = true;
            } else if (key == "dff") {
                if (!ParseString(m.dff)) return false;
                seenDff = true;
            } else if (key == "txd") {
                if (!ParseString(m.txd)) return false;
                seenTxd = true;
            } else if (key == "dff_crc32") {
                if (!ParseUint32(m.dffCrc)) return false;
                seenDffCrc = true;
            } else if (key == "txd_crc32") {
                if (!ParseUint32(m.txdCrc)) return false;
                seenTxdCrc = true;
            } else if (key == "dff_size") {
                if (!ParseUint32(m.dffSize)) return false;
                seenDffSize = true;
            } else if (key == "txd_size") {
                if (!ParseUint32(m.txdSize)) return false;
                seenTxdSize = true;
            } else {
                return Fail("unknown key in model object: " + key);
            }
            SkipWs();
            if (Consume(',')) continue;
            if (Consume('}')) break;
            return Fail("expected ',' or '}' in model object");
        }
        if (!seenSkin || !seenBase || !seenDff || !seenTxd || !seenDffCrc ||
            !seenTxdCrc || !seenDffSize || !seenTxdSize)
            return Fail("model object is missing a required field");
        if (m.skinid < 20000 || m.skinid > 30000)
            return Fail("skinid outside 20000-30000");
        if (m.baseid < 0 || m.baseid > 311)
            return Fail("baseid outside 0-311");
        if (m.dff.empty() || m.txd.empty())
            return Fail("empty dff/txd file name");
        if (m.dff.find('/') != std::string::npos ||
            m.dff.find('\\') != std::string::npos ||
            m.dff.find("..") != std::string::npos ||
            m.txd.find('/') != std::string::npos ||
            m.txd.find('\\') != std::string::npos ||
            m.txd.find("..") != std::string::npos)
            return Fail("dff/txd file name contains path characters");
        if (m.dffCrc == 0 || m.txdCrc == 0)
            return Fail("zero CRC32 in manifest");
        if (m.dffSize == 0 || m.txdSize == 0)
            return Fail("zero file size in manifest");
        return true;
    }

    // v1.3.0: one vehicle object. Required: vehid, dff, txd, both
    // CRCs, both sizes. Optional (default 0 = stock): maxspeed, accel,
    // brake. Same filename/CRC/size rules as skins; vehid 400-611.
    bool ParseVehicleObject(VehicleModel& m) {
        if (!Expect('{', "'{'")) return false;
        bool seenVeh = false, seenDff = false, seenTxd = false;
        bool seenDffCrc = false, seenTxdCrc = false, seenDffSize = false,
             seenTxdSize = false;
        SkipWs();
        if (Peek() == '}') {
            pos_++;
            return Fail("empty vehicle object (all fields are required)");
        }
        for (;;) {
            std::string key;
            if (!ParseString(key)) return false;
            if (!Expect(':', "':' after object key")) return false;
            if (key == "vehid") {
                uint32_t v;
                if (!ParseUint32(v)) return false;
                m.vehid = static_cast<int>(v);
                seenVeh = true;
            } else if (key == "dff") {
                if (!ParseString(m.dff)) return false;
                seenDff = true;
            } else if (key == "txd") {
                if (!ParseString(m.txd)) return false;
                seenTxd = true;
            } else if (key == "dff_crc32") {
                if (!ParseUint32(m.dffCrc)) return false;
                seenDffCrc = true;
            } else if (key == "txd_crc32") {
                if (!ParseUint32(m.txdCrc)) return false;
                seenTxdCrc = true;
            } else if (key == "dff_size") {
                if (!ParseUint32(m.dffSize)) return false;
                seenDffSize = true;
            } else if (key == "txd_size") {
                if (!ParseUint32(m.txdSize)) return false;
                seenTxdSize = true;
            } else if (key == "maxspeed") {
                if (!ParseFloat(m.maxspeed)) return false;
            } else if (key == "accel") {
                if (!ParseFloat(m.accel)) return false;
            } else if (key == "brake") {
                if (!ParseFloat(m.brake)) return false;
            } else {
                return Fail("unknown key in vehicle object: " + key);
            }
            SkipWs();
            if (Consume(',')) continue;
            if (Consume('}')) break;
            return Fail("expected ',' or '}' in vehicle object");
        }
        if (!seenVeh || !seenDff || !seenTxd || !seenDffCrc || !seenTxdCrc ||
            !seenDffSize || !seenTxdSize)
            return Fail("vehicle object is missing a required field");
        if (m.vehid < 400 || m.vehid > 611)
            return Fail("vehid outside 400-611");
        if (m.dff.empty() || m.txd.empty())
            return Fail("empty dff/txd file name");
        if (m.dff.find('/') != std::string::npos ||
            m.dff.find('\\') != std::string::npos ||
            m.dff.find("..") != std::string::npos ||
            m.txd.find('/') != std::string::npos ||
            m.txd.find('\\') != std::string::npos ||
            m.txd.find("..") != std::string::npos)
            return Fail("dff/txd file name contains path characters");
        if (m.dffCrc == 0 || m.txdCrc == 0)
            return Fail("zero CRC32 in vehicles");
        if (m.dffSize == 0 || m.txdSize == 0)
            return Fail("zero file size in vehicles");
        if (m.maxspeed < 0 || m.maxspeed > 500 || m.accel < 0 ||
            m.accel > 50 || m.brake < 0 || m.brake > 50)
            return Fail("handling value out of range");
        return true;
    }
};

}  // namespace

bool ParseManifest(const std::string& json, std::vector<ManifestModel>& out,
                   std::string& err) {
    err.clear();
    Parser p(json, err);
    return p.Parse(out);
}

bool ParseVehicles(const std::string& json, std::vector<VehicleModel>& out,
                   std::string& err) {
    err.clear();
    Parser p(json, err);
    return p.ParseVehicles(out);
}

}  // namespace nyx
