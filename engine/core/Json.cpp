// ============================================================================
//  SkyVault Engine - implementacion JSON
// ============================================================================
#include "core/Json.h"
#include <cstdlib>
#include <cmath>

namespace sv {

Json& Json::set(const char* key, Json value) {
    type = Type::Object;
    for (auto& [k, v] : obj)
        if (k == key) { v = std::move(value); return v; }
    obj.emplace_back(key, std::move(value));
    return obj.back().second;
}

const Json* Json::get(const char* key) const {
    if (type != Type::Object) return nullptr;
    for (const auto& [k, v] : obj)
        if (k == key) return &v;
    return nullptr;
}

Json& Json::operator[](const char* key) {
    type = Type::Object;
    for (auto& [k, v] : obj)
        if (k == key) return v;
    obj.emplace_back(key, Json{});
    return obj.back().second;
}

f64 Json::getNum(const char* key, f64 def) const {
    const Json* j = get(key);
    return (j && j->type == Type::Number) ? j->number : def;
}
i64 Json::getInt(const char* key, i64 def) const {
    const Json* j = get(key);
    return (j && j->type == Type::Number) ? static_cast<i64>(j->number) : def;
}
bool Json::getBool(const char* key, bool def) const {
    const Json* j = get(key);
    return (j && j->type == Type::Bool) ? j->boolean : def;
}
std::string Json::getStr(const char* key, std::string_view def) const {
    const Json* j = get(key);
    return (j && j->type == Type::String) ? j->str : std::string(def);
}

// ---------------------------------------------------------------------------
// Parser (descenso recursivo)
// ---------------------------------------------------------------------------
void Json::Parser::skipWs() {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p;
}

bool Json::Parser::expect(char c) {
    skipWs();
    if (p < end && *p == c) { ++p; return true; }
    failed = true;
    err = sv::format("se esperaba '%c'", c);
    return false;
}

bool Json::Parser::parseString(std::string& out) {
    skipWs();
    if (p >= end || *p != '"') { failed = true; err = "se esperaba cadena"; return false; }
    ++p;
    out.clear();
    while (p < end && *p != '"') {
        char ch = *p++;
        if (ch == '\\') {
            if (p >= end) break;
            char esc = *p++;
            switch (esc) {
                case '"':  out.push_back('"');  break;
                case '\\': out.push_back('\\'); break;
                case '/':  out.push_back('/');  break;
                case 'b':  out.push_back('\b'); break;
                case 'f':  out.push_back('\f'); break;
                case 'n':  out.push_back('\n'); break;
                case 'r':  out.push_back('\r'); break;
                case 't':  out.push_back('\t'); break;
                case 'u': {
                    // \uXXXX -> UTF-8
                    if (end - p < 4) { failed = true; err = "\\u incompleto"; return false; }
                    u32 cp = 0;
                    for (i32 i = 0; i < 4; ++i) {
                        char h = *p++;
                        cp <<= 4;
                        if (h >= '0' && h <= '9') cp |= static_cast<u32>(h - '0');
                        else if (h >= 'a' && h <= 'f') cp |= static_cast<u32>(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') cp |= static_cast<u32>(h - 'A' + 10);
                        else { failed = true; err = "hex invalido"; return false; }
                    }
                    if (cp < 0x80) out.push_back(static_cast<char>(cp));
                    else if (cp < 0x800) {
                        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                    } else {
                        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                    }
                    break;
                }
                default: failed = true; err = "escape invalido"; return false;
            }
        } else {
            out.push_back(ch);
        }
    }
    if (p >= end) { failed = true; err = "cadena sin cerrar"; return false; }
    ++p; // '"'
    return true;
}

bool Json::Parser::parseNumber(f64& out) {
    skipWs();
    const char* start = p;
    if (p < end && (*p == '-' || *p == '+')) ++p;
    bool any = false;
    while (p < end && ((*p >= '0' && *p <= '9') || *p == '.' || *p == 'e' || *p == 'E' || *p == '-' || *p == '+')) {
        ++p; any = true;
    }
    if (!any) { failed = true; err = "numero invalido"; return false; }
    out = std::strtod(start, nullptr);
    return true;
}

bool Json::Parser::parseValue(Json& out) {
    skipWs();
    if (p >= end) { failed = true; err = "fin inesperado"; return false; }
    const char c = *p;
    if (c == '{') {
        ++p;
        out = Json::makeObject();
        skipWs();
        if (p < end && *p == '}') { ++p; return true; }
        for (;;) {
            std::string key;
            if (!parseString(key)) return false;
            if (!expect(':')) return false;
            Json value;
            if (!parseValue(value)) return false;
            out.obj.emplace_back(std::move(key), std::move(value));
            skipWs();
            if (p < end && *p == ',') { ++p; continue; }
            return expect('}');
        }
    }
    if (c == '[') {
        ++p;
        out = Json::makeArray();
        skipWs();
        if (p < end && *p == ']') { ++p; return true; }
        for (;;) {
            Json value;
            if (!parseValue(value)) return false;
            out.arr.push_back(std::move(value));
            skipWs();
            if (p < end && *p == ',') { ++p; continue; }
            return expect(']');
        }
    }
    if (c == '"') { out = Json{}; out.type = Type::String; return parseString(out.str); }
    if (c == 't') {
        if (end - p >= 4 && std::string_view(p, 4) == "true") { p += 4; out = makeBool(true); return true; }
    }
    if (c == 'f') {
        if (end - p >= 5 && std::string_view(p, 5) == "false") { p += 5; out = makeBool(false); return true; }
    }
    if (c == 'n') {
        if (end - p >= 4 && std::string_view(p, 4) == "null") { p += 4; out = makeNull(); return true; }
    }
    f64 num = 0.0;
    if (parseNumber(num)) { out = makeNumber(num); return true; }
    failed = true;
    err = "valor inesperado";
    return false;
}

bool Json::parse(std::string_view text, std::string* error) {
    Parser parser;
    parser.p = text.data();
    parser.end = text.data() + text.size();
    Json result;
    if (!parser.parseValue(result)) {
        if (error) *error = parser.err;
        return false;
    }
    parser.skipWs();
    if (parser.p != parser.end) {
        if (error) *error = "contenido sobrante despues del valor";
        return false;
    }
    *this = std::move(result);
    return true;
}

// ---------------------------------------------------------------------------
// Dump
// ---------------------------------------------------------------------------
void Json::escapeString(const std::string& s, std::string& out) {
    out.push_back('"');
    for (char ch : s) {
        switch (ch) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<u8>(ch) < 0x20) {
                    out += sv::format("\\u%04x", static_cast<int>(static_cast<u8>(ch)));
                } else {
                    out.push_back(ch);
                }
        }
    }
    out.push_back('"');
}

void Json::dumpInternal(std::string& out, bool pretty, i32 depth) const {
    const auto indent = [&](i32 d) {
        if (pretty) { out.push_back('\n'); out.append(static_cast<usize>(d) * 2, ' '); }
    };
    switch (type) {
        case Type::Null:   out += "null"; break;
        case Type::Bool:   out += boolean ? "true" : "false"; break;
        case Type::Number: {
            const f64 intPart = std::floor(number);
            if (std::fabs(number - intPart) < 1e-12 && std::fabs(number) < 9.0e15)
                out += sv::format("%lld", static_cast<long long>(intPart));
            else
                out += sv::format("%.6g", number);
            break;
        }
        case Type::String: escapeString(str, out); break;
        case Type::Array: {
            if (arr.empty()) { out += "[]"; break; }
            out.push_back('[');
            for (usize i = 0; i < arr.size(); ++i) {
                indent(depth + 1);
                arr[i].dumpInternal(out, pretty, depth + 1);
                if (i + 1 < arr.size()) out.push_back(',');
            }
            indent(depth);
            out.push_back(']');
            break;
        }
        case Type::Object: {
            if (obj.empty()) { out += "{}"; break; }
            out.push_back('{');
            for (usize i = 0; i < obj.size(); ++i) {
                indent(depth + 1);
                escapeString(obj[i].first, out);
                out += pretty ? ": " : ":";
                obj[i].second.dumpInternal(out, pretty, depth + 1);
                if (i + 1 < obj.size()) out.push_back(',');
            }
            indent(depth);
            out.push_back('}');
            break;
        }
    }
}

void Json::dump(std::string& out, bool pretty) const {
    dumpInternal(out, pretty, 0);
}

} // namespace sv
