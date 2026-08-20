/*
 * Copyright (c) 2022, 2026 Taner Sener
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

#include "ReturnCode.h"

bool ffmpegkit::internal::ReturnCode::isSuccess(
    const std::shared_ptr<ffmpegkit::internal::ReturnCode> value) {
    return (value != nullptr) && (value->getValue() == Success);
}

bool ffmpegkit::internal::ReturnCode::isCancel(
    const std::shared_ptr<ffmpegkit::internal::ReturnCode> value) {
    return (value != nullptr) && (value->getValue() == Cancel);
}

ffmpegkit::internal::ReturnCode::ReturnCode(const int value) : _value{value} {}

int ffmpegkit::internal::ReturnCode::getValue() const { return _value; }

bool ffmpegkit::internal::ReturnCode::isValueSuccess() const {
    return (_value == Success);
}

bool ffmpegkit::internal::ReturnCode::isValueError() const {
    return ((_value != Success) && (_value != Cancel));
}

bool ffmpegkit::internal::ReturnCode::isValueCancel() const { return (_value == Cancel); }

namespace ffmpegkit {
namespace internal {

std::ostream &operator<<(std::ostream &out,
                         const std::shared_ptr<ffmpegkit::internal::ReturnCode> &o) {
    if (o == nullptr) {
        return out;
    } else {
        return out << o->_value;
    }
}

} // namespace internal
} // namespace ffmpegkit
