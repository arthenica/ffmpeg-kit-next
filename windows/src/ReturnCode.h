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

#ifndef FFMPEG_KIT_RETURN_CODE_H
#define FFMPEG_KIT_RETURN_CODE_H

#include <iostream>
#include <memory>

namespace ffmpegkit {

/**
 * <p>Return code of a completed <code>FFmpegKit</code> session.
 */
class ReturnCode {
  public:
    static constexpr int Success = 0;
    static constexpr int Cancel = 255;

    static bool isSuccess(const std::shared_ptr<ffmpegkit::ReturnCode> value) {
        return (value != nullptr) && (value->getValue() == Success);
    }

    static bool isCancel(const std::shared_ptr<ffmpegkit::ReturnCode> value) {
        return (value != nullptr) && (value->getValue() == Cancel);
    }

    ReturnCode(const int value) : _value{value} {}

    int getValue() const { return _value; }

    bool isValueSuccess() const { return (_value == Success); }

    bool isValueError() const {
        return ((_value != Success) && (_value != Cancel));
    }

    bool isValueCancel() const { return (_value == Cancel); }

    friend std::ostream &
    operator<<(std::ostream &out,
               const std::shared_ptr<ffmpegkit::ReturnCode> &o) {
        if (o == nullptr) {
            return out;
        } else {
            return out << o->_value;
        }
    }

  private:
    int _value;
};

} // namespace ffmpegkit

#endif // FFMPEG_KIT_RETURN_CODE_H
