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

#include "Value.h"

ffmpegkit::json::Value::Value()
    : _type{Type::Null}, _bool{false}, _int{0}, _double{0} {}

ffmpegkit::json::Value::Value(const bool value)
    : _type{Type::Bool}, _bool{value}, _int{0}, _double{0} {}

ffmpegkit::json::Value::Value(const int64_t value)
    : _type{Type::Int}, _bool{false}, _int{value}, _double{0} {}

ffmpegkit::json::Value::Value(const double value)
    : _type{Type::Double}, _bool{false}, _int{0}, _double{value} {}

ffmpegkit::json::Value::Value(std::string value)
    : _type{Type::String}, _bool{false}, _int{0}, _double{0},
      _string{std::move(value)} {}

ffmpegkit::json::Value ffmpegkit::json::Value::makeArray() {
    Value value;
    value._type = Type::Array;
    return value;
}

ffmpegkit::json::Value ffmpegkit::json::Value::makeObject() {
    Value value;
    value._type = Type::Object;
    return value;
}

ffmpegkit::json::Value::Type ffmpegkit::json::Value::getType() const {
    return _type;
}

bool ffmpegkit::json::Value::isNull() const { return _type == Type::Null; }

bool ffmpegkit::json::Value::isBool() const { return _type == Type::Bool; }

bool ffmpegkit::json::Value::isInt() const { return _type == Type::Int; }

bool ffmpegkit::json::Value::isDouble() const { return _type == Type::Double; }

bool ffmpegkit::json::Value::isString() const { return _type == Type::String; }

bool ffmpegkit::json::Value::isArray() const { return _type == Type::Array; }

bool ffmpegkit::json::Value::isObject() const { return _type == Type::Object; }

std::shared_ptr<bool> ffmpegkit::json::Value::getBool() const {
    if (_type != Type::Bool) {
        return nullptr;
    }
    return std::make_shared<bool>(_bool);
}

std::shared_ptr<int64_t> ffmpegkit::json::Value::getInt() const {
    if (_type != Type::Int) {
        return nullptr;
    }
    return std::make_shared<int64_t>(_int);
}

std::shared_ptr<double> ffmpegkit::json::Value::getDouble() const {
    if (_type == Type::Double) {
        return std::make_shared<double>(_double);
    } else if (_type == Type::Int) {
        return std::make_shared<double>(static_cast<double>(_int));
    } else {
        return nullptr;
    }
}

std::shared_ptr<std::string> ffmpegkit::json::Value::getString() const {
    if (_type != Type::String) {
        return nullptr;
    }
    return std::make_shared<std::string>(_string);
}

const ffmpegkit::json::Value *
ffmpegkit::json::Value::find(const std::string &key) const {
    if (_type != Type::Object) {
        return nullptr;
    }
    auto member = _object.find(key);
    if (member == _object.end()) {
        return nullptr;
    }
    return &member->second;
}

const std::map<std::string, ffmpegkit::json::Value> &
ffmpegkit::json::Value::getObject() const {
    return _object;
}

const std::vector<ffmpegkit::json::Value> &
ffmpegkit::json::Value::getArray() const {
    return _array;
}

void ffmpegkit::json::Value::set(const std::string &key, Value value) {
    if (_type != Type::Object) {
        return;
    }
    _object[key] = std::move(value);
}

void ffmpegkit::json::Value::append(Value value) {
    if (_type != Type::Array) {
        return;
    }
    _array.push_back(std::move(value));
}
