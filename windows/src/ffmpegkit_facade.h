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

#ifndef FFMPEG_KIT_FACADE_H
#define FFMPEG_KIT_FACADE_H

/*
 * Plumbing shared by the header-only C++ facade.
 *
 * Everything here compiles into the consumer's own translation unit. It turns
 * the flat C API in ffmpegkit_c.h into the C++ types the public classes hand
 * out: std::string, std::shared_ptr, std::list, std::function and exceptions.
 * Nothing declared in this header is part of the supported API.
 */

#include "ffmpegkit_c.h"
#include "ffmpegkit_exception.h"
#include "json/Value.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

/*
 * [[deprecated]] is C++14, and these headers still have to compile as C++11,
 * so deprecation goes through the compiler specific spelling instead.
 */
#if defined(_MSC_VER) && !defined(__clang__)
#define FFMPEGKIT_DEPRECATED(message) __declspec(deprecated(message))
#else
#define FFMPEGKIT_DEPRECATED(message) __attribute__((deprecated(message)))
#endif

namespace ffmpegkit {

class Session;

namespace detail {

/* ------------------------------------------------------------------------ */
/* Errors                                                                    */
/* ------------------------------------------------------------------------ */

/**
 * <p>Raises whatever the last library call left in this thread's error slot.
 *
 * <p>Call it directly after the library call whose result is being consumed:
 * every entry point clears the slot on the way in, so an intervening call
 * would discard the error.
 */
inline void checkError() {
    if (!ffk_has_error()) {
        return;
    }
    char *message = ffk_take_error();
    const std::string text =
        message == nullptr ? std::string("FFmpegKit call failed.")
                           : std::string(message);
    ffk_string_free(message);
    throw ffmpegkit::Exception(text);
}

/* ------------------------------------------------------------------------ */
/* Strings and buffers                                                       */
/* ------------------------------------------------------------------------ */

/** Copies a library owned string into a std::string and releases it. */
inline std::string takeString(char *value) {
    if (value == nullptr) {
        return std::string();
    }
    std::string result(value);
    ffk_string_free(value);
    return result;
}

/** Same as takeString(), but keeps "absent" distinguishable from "empty". */
inline std::shared_ptr<std::string> takeOptionalString(char *value) {
    if (value == nullptr) {
        return nullptr;
    }
    auto result = std::make_shared<std::string>(value);
    ffk_string_free(value);
    return result;
}

/** Copies a library owned byte buffer into a vector and releases it. */
inline std::shared_ptr<std::vector<uint8_t>> takeBytes(uint8_t *data,
                                                       const size_t size) {
    if (data == nullptr) {
        return nullptr;
    }
    auto result = std::make_shared<std::vector<uint8_t>>(data, data + size);
    ffk_bytes_free(data);
    return result;
}

/* ------------------------------------------------------------------------ */
/* JSON                                                                      */
/* ------------------------------------------------------------------------ */

/**
 * <p>Parses the JSON text a metadata accessor returned and releases it.
 *
 * <p>json::Value is a C++ object and cannot cross the ABI, so the library
 * serializes it and the value is rebuilt here, in the caller's own code.
 */
inline std::shared_ptr<ffmpegkit::json::Value> takeJson(char *json) {
    if (json == nullptr) {
        return nullptr;
    }
    const std::string text(json);
    ffk_string_free(json);
    auto value = std::make_shared<ffmpegkit::json::Value>();
    if (!ffmpegkit::json::detail::parse(text, *value)) {
        return nullptr;
    }
    return value;
}

/** Serializes a value so that it can be handed to the library. */
inline std::string
fromJson(const std::shared_ptr<ffmpegkit::json::Value> &value) {
    return value == nullptr ? std::string("null")
                            : ffmpegkit::json::detail::serialize(*value);
}

/* ------------------------------------------------------------------------ */
/* Protocol urls                                                             */
/* ------------------------------------------------------------------------ */

/**
 * <p>Builds the url that addresses a registered buffer or stream.
 *
 * <p>The library builds the string, because the ffkitmem and ffkitstream
 * protocol handlers inside it have to parse back exactly what was built.
 */
inline std::string buildProtocolUrl(const char *protocol, const long id,
                                    const std::string &extension) {
    std::string url = takeString(
        ffk_protocol_build_url(protocol, id, extension.c_str()));
    checkError();
    return url;
}

/* ------------------------------------------------------------------------ */
/* Time                                                                      */
/* ------------------------------------------------------------------------ */

/** Rebuilds a time point from the epoch milliseconds the C API carries. */
inline std::chrono::time_point<std::chrono::system_clock>
toTimePoint(const int64_t epochMilliseconds) {
    return std::chrono::time_point<std::chrono::system_clock>(
        std::chrono::milliseconds(epochMilliseconds));
}

/* ------------------------------------------------------------------------ */
/* Argument marshalling                                                      */
/* ------------------------------------------------------------------------ */

/**
 * <p>Holds a list of strings as the NUL terminated char pointer array the C
 * API expects, keeping the strings alive for as long as the array is used.
 */
class ArgumentArray {
  public:
    explicit ArgumentArray(const std::list<std::string> &arguments)
        : _storage(arguments.begin(), arguments.end()) {
        _pointers.reserve(_storage.size());
        for (const std::string &argument : _storage) {
            _pointers.push_back(argument.c_str());
        }
    }

    const char *const *data() const {
        return _pointers.empty() ? nullptr : _pointers.data();
    }

    size_t size() const { return _pointers.size(); }

  private:
    std::vector<std::string> _storage;
    std::vector<const char *> _pointers;
};

/* ------------------------------------------------------------------------ */
/* Lists                                                                     */
/* ------------------------------------------------------------------------ */

/** Rebuilds a std::list of strings from a library list and releases it. */
inline std::shared_ptr<std::list<std::string>>
takeStringList(FFKStringList *list) {
    auto result = std::make_shared<std::list<std::string>>();
    if (list == nullptr) {
        return result;
    }
    const size_t size = ffk_string_list_size(list);
    for (size_t index = 0; index < size; index++) {
        result->push_back(takeString(ffk_string_list_get(list, index)));
    }
    ffk_string_list_free(list);
    return result;
}

/* ------------------------------------------------------------------------ */
/* Sessions                                                                  */
/* ------------------------------------------------------------------------ */

/**
 * <p>Wraps a library session handle in the matching facade class.
 *
 * <p>Declared here so that the session classes can befriend it and keep their
 * handle constructors private.
 */
template <typename SessionType>
std::shared_ptr<SessionType> adoptSession(FFKSession *handle);

template <typename SessionType>
inline std::shared_ptr<SessionType> adoptSession(FFKSession *handle) {
    if (handle == nullptr) {
        return nullptr;
    }
    return std::shared_ptr<SessionType>(new SessionType(handle));
}

/** Rebuilds a std::list of sessions from a library list and releases it. */
template <typename SessionType>
inline std::shared_ptr<std::list<std::shared_ptr<SessionType>>>
takeSessionList(FFKSessionList *list) {
    auto result =
        std::make_shared<std::list<std::shared_ptr<SessionType>>>();
    if (list == nullptr) {
        return result;
    }
    const size_t size = ffk_session_list_size(list);
    for (size_t index = 0; index < size; index++) {
        result->push_back(
            adoptSession<SessionType>(ffk_session_list_get(list, index)));
    }
    ffk_session_list_free(list);
    return result;
}

/* ------------------------------------------------------------------------ */
/* Callbacks                                                                 */
/* ------------------------------------------------------------------------ */

/*
 * A std::function cannot cross the ABI, so it stays on this side of the
 * boundary in a heap allocated holder. The library receives a plain C
 * trampoline plus the holder address as its cookie, and a free function it
 * calls once it drops the callback - which means the holder is always deleted
 * by the same runtime that allocated it.
 *
 * The trampolines are inline functions with external linkage, so each consumer
 * binary has exactly one address for each of them. That is what lets the
 * callback getters recognise a callback as their own and hand the original
 * std::function back.
 */
template <typename Function> struct CallbackHolder {
    Function function;

    explicit CallbackHolder(Function value) : function(std::move(value)) {}
};

template <typename Function> inline void freeCallbackHolder(void *userData) {
    delete static_cast<CallbackHolder<Function> *>(userData);
}

/** Allocates a holder, or returns nullptr for an empty std::function. */
template <typename Function> inline void *makeCallbackHolder(const Function &function) {
    if (!function) {
        return nullptr;
    }
    return new CallbackHolder<Function>(function);
}

/** Returns the free function matching makeCallbackHolder(). */
template <typename Function> inline ffk_free_cb callbackHolderDeleter() {
    return &freeCallbackHolder<Function>;
}

/**
 * <p>Recovers a std::function previously registered through a trampoline.
 *
 * @param registered whether the library reported a callback at all
 * @param callback callback the library reported
 * @param userData cookie the library reported
 * @param trampoline this facade's trampoline for that callback kind
 * @return the original std::function, or an empty one when the callback did
 * not come from this facade
 */
template <typename Function, typename CallbackType, typename TrampolineType>
inline Function recoverCallback(const int registered,
                                const CallbackType callback, void *userData,
                                const TrampolineType trampoline) {
    if (registered == 0 || callback == nullptr || userData == nullptr) {
        return Function();
    }
    // The two pointers differ only in language linkage, which some compilers
    // model as a type difference, so they are compared through a common type
    if (reinterpret_cast<void (*)()>(callback) !=
        reinterpret_cast<void (*)()>(trampoline)) {
        return Function();
    }
    return static_cast<CallbackHolder<Function> *>(userData)->function;
}

} // namespace detail
} // namespace ffmpegkit

#endif // FFMPEG_KIT_FACADE_H
