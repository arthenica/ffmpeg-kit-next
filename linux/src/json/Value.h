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
#include <map>
#include <memory>
#include <string>
#include <vector>

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

} // namespace json
} // namespace ffmpegkit

#endif // FFMPEG_KIT_JSON_VALUE_H
