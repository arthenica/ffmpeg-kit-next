/*
 * Copyright (c) 2026 Taner Sener
 *
 * This file is part of FFmpegKitNext.
 *
 * FFmpegKitNext is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FFmpegKitNext is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General License for more details.
 *
 * You should have received a copy of the GNU Lesser General License
 * along with FFmpegKitNext. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef FFMPEG_KIT_JSON_VALUE_H
#define FFMPEG_KIT_JSON_VALUE_H

#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <utility>

namespace ffmpegkit {
namespace json {

/**
 * A JSON value.
 *
 * Values own their data and are copied by value, so a value stays valid after
 * the document it was read from is destroyed.
 *
 * Typed getters return nullptr when this value holds a different type, so a
 * caller never needs to check the type before reading it.
 */
class Value {
  public:
    enum class Type { Null, Bool, Int, Double, String, Array, Object };

    /**
     * Creates a null value.
     */
    Value();

    /**
     * Creates a boolean value.
     *
     * @param value boolean value
     */
    explicit Value(const bool value);

    /**
     * Creates an integer value.
     *
     * @param value integer value
     */
    explicit Value(const int64_t value);

    /**
     * Creates a double value.
     *
     * @param value double value
     */
    explicit Value(const double value);

    /**
     * Creates a string value.
     *
     * @param value string value
     */
    explicit Value(std::string value);

    /**
     * Creates an empty array value.
     *
     * @return array value
     */
    static Value makeArray();

    /**
     * Creates an empty object value.
     *
     * @return object value
     */
    static Value makeObject();

    /**
     * Returns the type of this value.
     *
     * @return value type
     */
    Type getType() const;

    bool isNull() const;
    bool isBool() const;
    bool isInt() const;
    bool isDouble() const;
    bool isString() const;
    bool isArray() const;
    bool isObject() const;

    /**
     * Returns the boolean held by this value.
     *
     * @return boolean value or nullptr if this value is not a boolean
     */
    std::shared_ptr<bool> getBool() const;

    /**
     * Returns the integer held by this value.
     *
     * @return integer value or nullptr if this value is not an integer
     */
    std::shared_ptr<int64_t> getInt() const;

    /**
     * Returns the double held by this value. Integers are returned as doubles
     * as well, since JSON does not distinguish between the two.
     *
     * @return double value or nullptr if this value is not a number
     */
    std::shared_ptr<double> getDouble() const;

    /**
     * Returns the string held by this value.
     *
     * @return string value or nullptr if this value is not a string
     */
    std::shared_ptr<std::string> getString() const;

    /**
     * Returns the object member associated with the key.
     *
     * The returned pointer is owned by this value and is invalidated when this
     * value is destroyed or modified.
     *
     * @param key member key
     * @return member value or nullptr if the key is not found or if this value
     * is not an object
     */
    const Value *find(const std::string &key) const;

    /**
     * Returns the members of this object.
     *
     * @return object members, empty if this value is not an object
     */
    const std::map<std::string, Value> &getObject() const;

    /**
     * Returns the elements of this array.
     *
     * @return array elements, empty if this value is not an array
     */
    const std::vector<Value> &getArray() const;

    /**
     * Sets an object member, replacing any existing member with the same key.
     * Ignored if this value is not an object.
     *
     * @param key member key
     * @param value member value
     */
    void set(const std::string &key, Value value);

    /**
     * Appends an array element. Ignored if this value is not an array.
     *
     * @param value element value
     */
    void append(Value value);

  private:
    Type _type;
    bool _bool;
    int64_t _int;
    double _double;
    std::string _string;
    std::map<std::string, Value> _object;
    std::vector<Value> _array;
};


inline Value::Value() : _type{Type::Null}, _bool{false}, _int{0}, _double{0} {}

inline Value::Value(const bool value)
    : _type{Type::Bool}, _bool{value}, _int{0}, _double{0} {}

inline Value::Value(const int64_t value)
    : _type{Type::Int}, _bool{false}, _int{value}, _double{0} {}

inline Value::Value(const double value)
    : _type{Type::Double}, _bool{false}, _int{0}, _double{value} {}

inline Value::Value(std::string value)
    : _type{Type::String}, _bool{false}, _int{0}, _double{0},
      _string{std::move(value)} {}

inline Value Value::makeArray() {
    Value value;
    value._type = Type::Array;
    return value;
}

inline Value Value::makeObject() {
    Value value;
    value._type = Type::Object;
    return value;
}

inline Value::Type Value::getType() const { return _type; }

inline bool Value::isNull() const { return _type == Type::Null; }

inline bool Value::isBool() const { return _type == Type::Bool; }

inline bool Value::isInt() const { return _type == Type::Int; }

inline bool Value::isDouble() const { return _type == Type::Double; }

inline bool Value::isString() const { return _type == Type::String; }

inline bool Value::isArray() const { return _type == Type::Array; }

inline bool Value::isObject() const { return _type == Type::Object; }

inline std::shared_ptr<bool> Value::getBool() const {
    if (_type != Type::Bool) {
        return nullptr;
    }
    return std::make_shared<bool>(_bool);
}

inline std::shared_ptr<int64_t> Value::getInt() const {
    if (_type != Type::Int) {
        return nullptr;
    }
    return std::make_shared<int64_t>(_int);
}

inline std::shared_ptr<double> Value::getDouble() const {
    if (_type == Type::Double) {
        return std::make_shared<double>(_double);
    } else if (_type == Type::Int) {
        return std::make_shared<double>(static_cast<double>(_int));
    } else {
        return nullptr;
    }
}

inline std::shared_ptr<std::string> Value::getString() const {
    if (_type != Type::String) {
        return nullptr;
    }
    return std::make_shared<std::string>(_string);
}

inline const Value *Value::find(const std::string &key) const {
    if (_type != Type::Object) {
        return nullptr;
    }
    auto member = _object.find(key);
    if (member == _object.end()) {
        return nullptr;
    }
    return &member->second;
}

inline const std::map<std::string, Value> &Value::getObject() const {
    return _object;
}

inline const std::vector<Value> &Value::getArray() const { return _array; }

inline void Value::set(const std::string &key, Value value) {
    if (_type != Type::Object) {
        return;
    }
    _object[key] = std::move(value);
}

inline void Value::append(Value value) {
    if (_type != Type::Array) {
        return;
    }
    _array.push_back(std::move(value));
}

/**
 * <p>Serialization helpers.
 *
 * <p>A Value is a C++ object and can never cross the FFmpegKit ABI boundary,
 * so the shim inside the library serializes it to JSON text and the
 * header-only facade parses that text back into a Value on the caller's side.
 * Both directions are inline, which keeps json/Value.h off the ABI.
 */
namespace detail {

inline void serializeString(const std::string &value, std::string &out) {
    out += '"';
    for (unsigned char c : value) {
        switch (c) {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\b':
            out += "\\b";
            break;
        case '\f':
            out += "\\f";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        default:
            if (c < 0x20) {
                static const char *digits = "0123456789abcdef";
                out += "\\u00";
                out += digits[(c >> 4) & 0xf];
                out += digits[c & 0xf];
            } else {
                // UTF-8 lead and continuation bytes are emitted verbatim: JSON
                // text is UTF-8 and they need no further escaping.
                out += static_cast<char>(c);
            }
            break;
        }
    }
    out += '"';
}

inline void serializeValue(const Value &value, std::string &out) {
    switch (value.getType()) {
    case Value::Type::Null:
        out += "null";
        break;
    case Value::Type::Bool:
        out += *value.getBool() ? "true" : "false";
        break;
    case Value::Type::Int: {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%lld",
                      static_cast<long long>(*value.getInt()));
        out += buffer;
        break;
    }
    case Value::Type::Double: {
        const double number = *value.getDouble();
        if (number != number || number - number != 0) {
            // NaN and the infinities have no JSON representation
            out += "null";
        } else {
            char buffer[64];
            std::snprintf(buffer, sizeof(buffer), "%.17g", number);
            out += buffer;
        }
        break;
    }
    case Value::Type::String:
        serializeString(*value.getString(), out);
        break;
    case Value::Type::Array: {
        out += '[';
        bool first = true;
        for (const Value &element : value.getArray()) {
            if (!first) {
                out += ',';
            }
            first = false;
            serializeValue(element, out);
        }
        out += ']';
        break;
    }
    case Value::Type::Object: {
        out += '{';
        bool first = true;
        for (const auto &member : value.getObject()) {
            if (!first) {
                out += ',';
            }
            first = false;
            serializeString(member.first, out);
            out += ':';
            serializeValue(member.second, out);
        }
        out += '}';
        break;
    }
    }
}

/**
 * <p>Serializes a value into JSON text.
 *
 * @param value value to serialize
 * @return JSON text
 */
inline std::string serialize(const Value &value) {
    std::string out;
    serializeValue(value, out);
    return out;
}

/**
 * <p>Recursive descent parser for the JSON text produced by serialize().
 */
class Parser {
  public:
    Parser(const std::string &text) : _text(text), _position(0) {}

    bool parse(Value &out) {
        skipWhitespace();
        if (!parseValue(out)) {
            return false;
        }
        skipWhitespace();
        return _position == _text.size();
    }

  private:
    void skipWhitespace() {
        while (_position < _text.size()) {
            const char c = _text[_position];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                _position++;
            } else {
                break;
            }
        }
    }

    bool literal(const char *expected) {
        const size_t length = std::char_traits<char>::length(expected);
        if (_text.compare(_position, length, expected) != 0) {
            return false;
        }
        _position += length;
        return true;
    }

    static bool appendUtf8(uint32_t codepoint, std::string &out) {
        if (codepoint < 0x80) {
            out += static_cast<char>(codepoint);
        } else if (codepoint < 0x800) {
            out += static_cast<char>(0xc0 | (codepoint >> 6));
            out += static_cast<char>(0x80 | (codepoint & 0x3f));
        } else if (codepoint < 0x10000) {
            out += static_cast<char>(0xe0 | (codepoint >> 12));
            out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (codepoint & 0x3f));
        } else if (codepoint <= 0x10ffff) {
            out += static_cast<char>(0xf0 | (codepoint >> 18));
            out += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f));
            out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
            out += static_cast<char>(0x80 | (codepoint & 0x3f));
        } else {
            return false;
        }
        return true;
    }

    bool parseHex4(uint32_t &out) {
        if (_position + 4 > _text.size()) {
            return false;
        }
        out = 0;
        for (int index = 0; index < 4; index++) {
            const char c = _text[_position++];
            out <<= 4;
            if (c >= '0' && c <= '9') {
                out |= static_cast<uint32_t>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                out |= static_cast<uint32_t>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                out |= static_cast<uint32_t>(c - 'A' + 10);
            } else {
                return false;
            }
        }
        return true;
    }

    bool parseString(std::string &out) {
        if (_position >= _text.size() || _text[_position] != '"') {
            return false;
        }
        _position++;
        while (_position < _text.size()) {
            const char c = _text[_position++];
            if (c == '"') {
                return true;
            }
            if (c != '\\') {
                out += c;
                continue;
            }
            if (_position >= _text.size()) {
                return false;
            }
            const char escape = _text[_position++];
            switch (escape) {
            case '"':
                out += '"';
                break;
            case '\\':
                out += '\\';
                break;
            case '/':
                out += '/';
                break;
            case 'b':
                out += '\b';
                break;
            case 'f':
                out += '\f';
                break;
            case 'n':
                out += '\n';
                break;
            case 'r':
                out += '\r';
                break;
            case 't':
                out += '\t';
                break;
            case 'u': {
                uint32_t codepoint = 0;
                if (!parseHex4(codepoint)) {
                    return false;
                }
                if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                    // A high surrogate is only valid when its low surrogate
                    // follows; the pair encodes a single codepoint.
                    if (_position + 1 >= _text.size() ||
                        _text[_position] != '\\' ||
                        _text[_position + 1] != 'u') {
                        return false;
                    }
                    _position += 2;
                    uint32_t low = 0;
                    if (!parseHex4(low) || low < 0xdc00 || low > 0xdfff) {
                        return false;
                    }
                    codepoint =
                        0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
                }
                if (!appendUtf8(codepoint, out)) {
                    return false;
                }
                break;
            }
            default:
                return false;
            }
        }
        return false;
    }

    bool parseNumber(Value &out) {
        const size_t start = _position;
        if (_position < _text.size() &&
            (_text[_position] == '-' || _text[_position] == '+')) {
            _position++;
        }
        bool isDouble = false;
        while (_position < _text.size()) {
            const char c = _text[_position];
            if (c >= '0' && c <= '9') {
                _position++;
            } else if (c == '.' || c == 'e' || c == 'E' || c == '+' ||
                       c == '-') {
                isDouble = isDouble || c == '.' || c == 'e' || c == 'E';
                _position++;
            } else {
                break;
            }
        }
        if (_position == start) {
            return false;
        }
        const std::string number = _text.substr(start, _position - start);
        try {
            if (isDouble) {
                out = Value(static_cast<double>(std::stod(number)));
            } else {
                out = Value(static_cast<int64_t>(std::stoll(number)));
            }
        } catch (...) {
            return false;
        }
        return true;
    }

    bool parseValue(Value &out) {
        if (_position >= _text.size()) {
            return false;
        }
        switch (_text[_position]) {
        case 'n':
            if (!literal("null")) {
                return false;
            }
            out = Value();
            return true;
        case 't':
            if (!literal("true")) {
                return false;
            }
            out = Value(true);
            return true;
        case 'f':
            if (!literal("false")) {
                return false;
            }
            out = Value(false);
            return true;
        case '"': {
            std::string text;
            if (!parseString(text)) {
                return false;
            }
            out = Value(std::move(text));
            return true;
        }
        case '[': {
            _position++;
            out = Value::makeArray();
            skipWhitespace();
            if (_position < _text.size() && _text[_position] == ']') {
                _position++;
                return true;
            }
            while (true) {
                skipWhitespace();
                Value element;
                if (!parseValue(element)) {
                    return false;
                }
                out.append(std::move(element));
                skipWhitespace();
                if (_position >= _text.size()) {
                    return false;
                }
                if (_text[_position] == ',') {
                    _position++;
                    continue;
                }
                if (_text[_position] == ']') {
                    _position++;
                    return true;
                }
                return false;
            }
        }
        case '{': {
            _position++;
            out = Value::makeObject();
            skipWhitespace();
            if (_position < _text.size() && _text[_position] == '}') {
                _position++;
                return true;
            }
            while (true) {
                skipWhitespace();
                std::string key;
                if (!parseString(key)) {
                    return false;
                }
                skipWhitespace();
                if (_position >= _text.size() || _text[_position] != ':') {
                    return false;
                }
                _position++;
                skipWhitespace();
                Value member;
                if (!parseValue(member)) {
                    return false;
                }
                out.set(key, std::move(member));
                skipWhitespace();
                if (_position >= _text.size()) {
                    return false;
                }
                if (_text[_position] == ',') {
                    _position++;
                    continue;
                }
                if (_text[_position] == '}') {
                    _position++;
                    return true;
                }
                return false;
            }
        }
        default:
            return parseNumber(out);
        }
    }

    const std::string &_text;
    size_t _position;
};

/**
 * <p>Parses JSON text into a value.
 *
 * @param text JSON text
 * @param out value parsed, left untouched when parsing fails
 * @return true when the whole text was parsed, false otherwise
 */
inline bool parse(const std::string &text, Value &out) {
    Value parsed;
    Parser parser(text);
    if (!parser.parse(parsed)) {
        return false;
    }
    out = std::move(parsed);
    return true;
}

} // namespace detail

} // namespace json
} // namespace ffmpegkit

#endif // FFMPEG_KIT_JSON_VALUE_H
