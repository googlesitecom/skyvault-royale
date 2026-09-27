// ============================================================================
//  SkyVault Engine - JSON minimalista propio (parseo + serializacion)
//  Usado para: perfil de jugador, guardado, recetas de loot y metajuego.
// ============================================================================
#pragma once

#include "core/Core.h"
#include <vector>
#include <string>
#include <utility>

namespace sv {

class Json {
public:
    enum class Type : i32 { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;
    bool  boolean = false;
    f64   number = 0.0;
    std::string str;
    std::vector<Json> arr;
    std::vector<std::pair<std::string, Json>> obj;

    // ---- fabrica ----
    [[nodiscard]] static Json makeNull()   { return Json{}; }
    [[nodiscard]] static Json makeBool(bool b) { Json j; j.type = Type::Bool; j.boolean = b; return j; }
    [[nodiscard]] static Json makeNumber(f64 n) { Json j; j.type = Type::Number; j.number = n; return j; }
    [[nodiscard]] static Json makeString(std::string_view s) { Json j; j.type = Type::String; j.str = std::string(s); return j; }
    [[nodiscard]] static Json makeArray()  { Json j; j.type = Type::Array; return j; }
    [[nodiscard]] static Json makeObject() { Json j; j.type = Type::Object; return j; }

    // ---- objeto ----
    Json& set(const char* key, Json value);
    [[nodiscard]] const Json* get(const char* key) const;   // nullptr si no existe
    Json& operator[](const char* key);                      // crea si no existe
    [[nodiscard]] bool has(const char* key) const { return get(key) != nullptr; }

    // ---- array ----
    void push(Json value) { type = Type::Array; arr.push_back(std::move(value)); }
    [[nodiscard]] usize size() const { return type == Type::Array ? arr.size() : (type == Type::Object ? obj.size() : 0); }
    [[nodiscard]] const Json* at(usize i) const { return (type == Type::Array && i < arr.size()) ? &arr[i] : nullptr; }

    // ---- lectores con default ----
    [[nodiscard]] f64   getNum(const char* key, f64 def) const;
    [[nodiscard]] i64   getInt(const char* key, i64 def) const;
    [[nodiscard]] bool  getBool(const char* key, bool def) const;
    [[nodiscard]] std::string getStr(const char* key, std::string_view def) const;

    // ---- (de)serializacion ----
    [[nodiscard]] bool parse(std::string_view text, std::string* error = nullptr);
    void dump(std::string& out, bool pretty = false) const;
    [[nodiscard]] std::string dumpStr(bool pretty = false) const { std::string s; dump(s, pretty); return s; }

private:
    struct Parser {
        const char* p = nullptr;
        const char* end = nullptr;
        std::string err;
        bool failed = false;
        void skipWs();
        bool parseValue(Json& out);
        bool parseString(std::string& out);
        bool parseNumber(f64& out);
        bool expect(char c);
    };
    void dumpInternal(std::string& out, bool pretty, i32 depth) const;
    static void escapeString(const std::string& s, std::string& out);
};

} // namespace sv
