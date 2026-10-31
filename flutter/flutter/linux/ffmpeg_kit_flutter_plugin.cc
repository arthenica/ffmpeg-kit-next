/*
 * Copyright (c) 2018-2022, 2026 Taner Sener
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

#include "include/ffmpeg_kit_next_flutter/f_fmpeg_kit_flutter_plugin.h"

#include <flutter_linux/flutter_linux.h>
#include <gtk/gtk.h>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <exception>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "plugin_state.h"

#include <AbstractSession.h>
#include <ArchDetect.h>
#include <FFmpegKit.h>
#include <FFmpegKitConfig.h>
#include <FFmpegKitInputBuffer.h>
#include <FFmpegKitOutputBuffer.h>
#include <FFmpegKitStreamInput.h>
#include <FFmpegKitStreamOutput.h>
#include <FFmpegSession.h>
#include <FFprobeKit.h>
#include <FFprobeSession.h>
#include <Level.h>
#include <Log.h>
#include <LogRedirectionStrategy.h>
#include <MediaInformation.h>
#include <MediaInformationJsonParser.h>
#include <MediaInformationSession.h>
#include <Packages.h>
#include <ReturnCode.h>
#include <Session.h>
#include <SessionDeleteListener.h>
#include <SessionState.h>
#include <Statistics.h>
#include <json/Value.h>

#define FFMPEG_KIT_PLATFORM_NAME "linux"

static const char* kMethodChannelName = "flutter.arthenica.com/ffmpeg_kit";
static const char* kEventChannelName = "flutter.arthenica.com/ffmpeg_kit_event";

// Log / statistics / session keys — must match the ObjC handler and Dart parsers.
static const char* KEY_LOG_SESSION_ID = "sessionId";
static const char* KEY_LOG_LEVEL = "level";
static const char* KEY_LOG_MESSAGE = "message";
static const char* KEY_STATISTICS_SESSION_ID = "sessionId";
static const char* KEY_STATISTICS_VIDEO_FRAME_NUMBER = "videoFrameNumber";
static const char* KEY_STATISTICS_VIDEO_FPS = "videoFps";
static const char* KEY_STATISTICS_VIDEO_QUALITY = "videoQuality";
static const char* KEY_STATISTICS_SIZE = "size";
static const char* KEY_STATISTICS_TIME = "time";
static const char* KEY_STATISTICS_BITRATE = "bitrate";
static const char* KEY_STATISTICS_SPEED = "speed";
static const char* KEY_SESSION_ID = "sessionId";
static const char* KEY_SESSION_CREATE_TIME = "createTime";
static const char* KEY_SESSION_START_TIME = "startTime";
static const char* KEY_SESSION_COMMAND = "command";
static const char* KEY_SESSION_ARGUMENTS = "arguments";
static const char* KEY_SESSION_TYPE = "type";
static const char* KEY_SESSION_MEDIA_INFORMATION = "mediaInformation";
static const int SESSION_TYPE_FFMPEG = 1;
static const int SESSION_TYPE_FFPROBE = 2;
static const int SESSION_TYPE_MEDIA_INFORMATION = 3;

// Event names — must match the ObjC handler and the Dart event dispatcher.
static const char* EVENT_LOG_CALLBACK_EVENT = "FFmpegKitLogCallbackEvent";
static const char* EVENT_STATISTICS_CALLBACK_EVENT =
    "FFmpegKitStatisticsCallbackEvent";
static const char* EVENT_COMPLETE_CALLBACK_EVENT =
    "FFmpegKitCompleteCallbackEvent";
static const char* EVENT_SESSION_DELETED_CALLBACK_EVENT =
    "FFmpegKitSessionDeletedCallbackEvent";

using ffmpeg_kit_linux::PendingResponse;

// What workers and native callbacks hold instead of the plugin GObject.
// Lifecycle (admission, executions, replies, async sessions) is PluginState.
struct PluginData {
    std::shared_ptr<ffmpeg_kit_linux::PluginState> executions = std::make_shared<ffmpeg_kit_linux::PluginState>();
    // Read by native callbacks, so disabled events are dropped before conversion.
    std::atomic<bool> logs_enabled{false};
    std::atomic<bool> statistics_enabled{false};
    GWeakRef plugin;
    PluginData() { g_weak_ref_init(&plugin, nullptr); }
    ~PluginData() { g_weak_ref_clear(&plugin); }
};

struct _FfmpegKitNextFlutterPlugin {
  GObject parent_instance;
  FlEventChannel* event_channel;
  gboolean listening;
  gboolean retain_shutdown_sessions;
  std::map<long, std::shared_ptr<ffmpegkit::Session>> *shutdown_sessions;

  // "ffkit" protocol object registries, keyed by the generated protocol url.
  // Held as pointers because g_object_new does not run C++ member constructors.
  std::map<std::string, std::shared_ptr<ffmpegkit::FFmpegKitInputBuffer>>*
      input_buffers;
  std::map<std::string, std::shared_ptr<ffmpegkit::FFmpegKitOutputBuffer>>*
      output_buffers;
  std::map<std::string, std::shared_ptr<ffmpegkit::FFmpegKitStreamInput>>*
      stream_inputs;
  std::map<std::string, std::shared_ptr<ffmpegkit::FFmpegKitStreamOutput>>*
      stream_outputs;
  std::shared_ptr<ffmpegkit::SessionDeleteListener>* delete_listener;
  std::shared_ptr<PluginData> *state;
};

G_DEFINE_TYPE(FfmpegKitNextFlutterPlugin, ffmpeg_kit_next_flutter_plugin,
              g_object_get_type())

#define FFMPEG_KIT_NEXT_FLUTTER_PLUGIN(obj)                              \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),                                     \
                              ffmpeg_kit_next_flutter_plugin_get_type(), \
                              FfmpegKitNextFlutterPlugin))

// ---------------------------------------------------------------------------
// Serializers
// ---------------------------------------------------------------------------

// Milliseconds since the Unix epoch, matching the ObjC
// [[date] timeIntervalSince1970]*1000 conversion.
static int64_t to_epoch_millis(
    const std::chrono::time_point<std::chrono::system_clock>& tp) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             tp.time_since_epoch())
      .count();
}

// FFmpeg/FFprobe emit log and metadata text in whatever encoding the source
// media uses, so it is not guaranteed to be valid UTF-8. The Flutter standard
// message codec requires UTF-8 and throws a FormatException Dart-side on invalid
// bytes, so sanitize (invalid sequences -> U+FFFD) before building the value.
static FlValue* new_utf8_value(const std::string& s) {
  gchar* valid = g_utf8_make_valid(s.c_str(), static_cast<gssize>(s.size()));
  FlValue* v = fl_value_new_string(valid);
  g_free(valid);
  return v;
}

// Recursively converts an ffmpegkit::json::Value tree into an FlValue, so
// MediaInformation's arbitrary JSON properties cross the channel intact.
static FlValue* json_value_to_fl_value(const ffmpegkit::json::Value& value) {
  switch (value.getType()) {
    case ffmpegkit::json::Value::Type::Null:
      return fl_value_new_null();
    case ffmpegkit::json::Value::Type::Bool: {
      auto b = value.getBool();
      return fl_value_new_bool(b ? *b : false);
    }
    case ffmpegkit::json::Value::Type::Int: {
      auto i = value.getInt();
      return fl_value_new_int(i ? *i : 0);
    }
    case ffmpegkit::json::Value::Type::Double: {
      auto d = value.getDouble();
      return fl_value_new_float(d ? *d : 0.0);
    }
    case ffmpegkit::json::Value::Type::String: {
      auto s = value.getString();
      return s ? new_utf8_value(*s) : fl_value_new_string("");
    }
    case ffmpegkit::json::Value::Type::Array: {
      FlValue* list = fl_value_new_list();
      for (const auto& element : value.getArray()) {
        fl_value_append_take(list, json_value_to_fl_value(element));
      }
      return list;
    }
    case ffmpegkit::json::Value::Type::Object: {
      FlValue* map = fl_value_new_map();
      for (const auto& entry : value.getObject()) {
          gchar *key = g_utf8_make_valid(entry.first.c_str(), static_cast<gssize>(entry.first.size()));
          fl_value_set_string_take(map, key, json_value_to_fl_value(entry.second));
          g_free(key);
      }
      return map;
    }
  }
  return fl_value_new_null();
}

static FlValue* to_log_map(const std::shared_ptr<ffmpegkit::Log>& log) {
  FlValue* m = fl_value_new_map();
  fl_value_set_string_take(m, KEY_LOG_SESSION_ID,
                           fl_value_new_int(log->getSessionId()));
  fl_value_set_string_take(m, KEY_LOG_LEVEL,
                           fl_value_new_int(static_cast<int>(log->getLevel())));
  fl_value_set_string_take(m, KEY_LOG_MESSAGE, new_utf8_value(log->getMessage()));
  return m;
}

static FlValue* to_statistics_map(
    const std::shared_ptr<ffmpegkit::Statistics>& statistics) {
  FlValue* m = fl_value_new_map();
  fl_value_set_string_take(m, KEY_STATISTICS_SESSION_ID,
                           fl_value_new_int(statistics->getSessionId()));
  fl_value_set_string_take(m, KEY_STATISTICS_VIDEO_FRAME_NUMBER,
                           fl_value_new_int(statistics->getVideoFrameNumber()));
  fl_value_set_string_take(m, KEY_STATISTICS_VIDEO_FPS,
                           fl_value_new_float(statistics->getVideoFps()));
  fl_value_set_string_take(m, KEY_STATISTICS_VIDEO_QUALITY,
                           fl_value_new_float(statistics->getVideoQuality()));
  fl_value_set_string_take(m, KEY_STATISTICS_SIZE,
                           fl_value_new_int(statistics->getSize()));
  fl_value_set_string_take(m, KEY_STATISTICS_TIME,
                           fl_value_new_float(statistics->getTime()));
  fl_value_set_string_take(m, KEY_STATISTICS_BITRATE,
                           fl_value_new_float(statistics->getBitrate()));
  fl_value_set_string_take(m, KEY_STATISTICS_SPEED,
                           fl_value_new_float(statistics->getSpeed()));
  return m;
}

static FlValue* to_media_information_map(
    const std::shared_ptr<ffmpegkit::MediaInformation>& media_information) {
  if (media_information == nullptr) {
    return fl_value_new_null();
  }
  auto all_properties = media_information->getAllProperties();
  if (all_properties == nullptr) {
    return fl_value_new_map();
  }
  return json_value_to_fl_value(*all_properties);
}

static FlValue* to_session_map(
    const std::shared_ptr<ffmpegkit::Session>& session) {
  if (session == nullptr) {
    return fl_value_new_null();
  }
  FlValue* m = fl_value_new_map();
  fl_value_set_string_take(m, KEY_SESSION_ID,
                           fl_value_new_int(session->getSessionId()));
  fl_value_set_string_take(
      m, KEY_SESSION_CREATE_TIME,
      fl_value_new_int(to_epoch_millis(session->getCreateTime())));
  fl_value_set_string_take(
      m, KEY_SESSION_START_TIME,
      fl_value_new_int(to_epoch_millis(session->getStartTime())));
  fl_value_set_string_take(m, KEY_SESSION_COMMAND,
                           new_utf8_value(session->getCommand()));
  FlValue* arguments = fl_value_new_list();
  auto native_arguments = session->getArguments();
  if (native_arguments) {
    for (const auto& argument : *native_arguments)
      fl_value_append_take(arguments, new_utf8_value(argument));
  }
  fl_value_set_string_take(m, KEY_SESSION_ARGUMENTS, arguments);
  if (session->isFFmpeg()) {
    fl_value_set_string_take(m, KEY_SESSION_TYPE,
                             fl_value_new_int(SESSION_TYPE_FFMPEG));
  } else if (session->isFFprobe()) {
    fl_value_set_string_take(m, KEY_SESSION_TYPE,
                             fl_value_new_int(SESSION_TYPE_FFPROBE));
  } else if (session->isMediaInformation()) {
    auto media_information_session =
        std::static_pointer_cast<ffmpegkit::MediaInformationSession>(session);
    auto media_information = media_information_session->getMediaInformation();
    if (media_information != nullptr) {
        fl_value_set_string_take(m, KEY_SESSION_MEDIA_INFORMATION, to_media_information_map(media_information));
    }
    fl_value_set_string_take(m, KEY_SESSION_TYPE,
                             fl_value_new_int(SESSION_TYPE_MEDIA_INFORMATION));
  }
  return m;
}

// Wraps a payload under its event name, matching the ObjC toStringDictionary.
static FlValue* to_event_map(const char* event_name, FlValue* payload) {
  FlValue* m = fl_value_new_map();
  fl_value_set_string_take(m, event_name, payload);
  return m;
}

// Builds an FlValue list of session maps. Templated so it accepts lists of any
// concrete session type (FFmpegSession, FFprobeSession, ...) that derives from
// Session.
template <typename T>
static FlValue* to_session_list_value(
    const std::shared_ptr<std::list<std::shared_ptr<T>>>& sessions) {
  FlValue* list = fl_value_new_list();
  if (sessions != nullptr) {
    for (const auto& session : *sessions) {
      fl_value_append_take(list, to_session_map(session));
    }
  }
  return list;
}

static FlValue* to_log_list_value(
    const std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::Log>>>& logs) {
  FlValue* list = fl_value_new_list();
  if (logs != nullptr) {
    for (const auto& log : *logs) {
      fl_value_append_take(list, to_log_map(log));
    }
  }
  return list;
}

static FlValue* to_statistics_list_value(
    const std::shared_ptr<std::list<std::shared_ptr<ffmpegkit::Statistics>>>&
        statistics) {
  FlValue* list = fl_value_new_list();
  if (statistics != nullptr) {
    for (const auto& item : *statistics) {
      fl_value_append_take(list, to_statistics_map(item));
    }
  }
  return list;
}

// ---------------------------------------------------------------------------
// Argument extraction and response helpers
// ---------------------------------------------------------------------------

// A rejected argument. method_call_cb reports it with its code and message.
class ArgumentError : public std::runtime_error {
  public:
    ArgumentError(const std::string &code, const std::string &message) : std::runtime_error(message), code(code) {}
    explicit ArgumentError(const std::string &message) : ArgumentError("INVALID_ARGUMENTS", message) {}
    const std::string code;
};

// The code and message a method reports for a bad argument, as on Windows. An
// empty check falls back to INVALID_ARGUMENTS.
struct Check {
    const char *code;
    const char *message;
};

// Argument checks shared with the Android, iOS and Windows plugins. Keep the
// codes and messages aligned with them.
static const Check kInvalidSession{"INVALID_SESSION", "Invalid session id."};
static const Check kInvalidArgumentsArray{"INVALID_ARGUMENTS", "Invalid arguments array."};
static const Check kInvalidShutdownTimeout{"INVALID_ARGUMENTS", "waitTimeout is required."};
static const Check kInvalidFfprobeJsonOutput{"INVALID_FFPROBE_JSON_OUTPUT", "Invalid ffprobe json output."};
static const Check kInvalidPath{"INVALID_PATH", "Invalid path."};
static const Check kInvalidFontDirectory{"INVALID_FONT_DIRECTORY", "Invalid font directory."};
static const Check kInvalidFontDirectoryList{"INVALID_FONT_DIRECTORY_LIST", "Invalid font directory list."};
static const Check kInvalidPipePath{"INVALID_PIPE_PATH", "Invalid ffmpeg pipe path."};
static const Check kInvalidVariableName{"INVALID_NAME", "Invalid environment variable name."};
static const Check kInvalidVariableValue{"INVALID_VALUE", "Invalid environment variable value."};
static const Check kInvalidSignal{"INVALID_SIGNAL", "Invalid signal value."};
static const Check kInvalidLevel{"INVALID_LEVEL", "Invalid level value."};
static const Check kInvalidSize{"INVALID_SIZE", "Invalid session history size value."};
static const Check kInvalidSessionState{"INVALID_SESSION_STATE", "Invalid session state value."};
static const Check kInvalidLogRedirectionStrategy{"INVALID_LOG_REDIRECTION_STRATEGY", "Invalid log redirection strategy value."};
static const Check kInvalidData{"INVALID_DATA", "Invalid input buffer data."};
static const Check kInvalidInputBufferUrl{"INVALID_URL", "Invalid input buffer url."};
static const Check kInvalidOutputBufferUrl{"INVALID_URL", "Invalid output buffer url."};
static const Check kInvalidStreamInputUrl{"INVALID_URL", "Invalid stream input url."};
static const Check kInvalidStreamOutputUrl{"INVALID_URL", "Invalid stream output url."};
static const Check kInvalidStreamInputArguments{"INVALID_ARGUMENTS", "Invalid stream input arguments."};
static const Check kInvalidStreamOutputArguments{"INVALID_ARGUMENTS", "Invalid stream output arguments."};
// writeToPipe is Linux-only here; these match Android and iOS.
static const Check kInvalidPipe{"INVALID_PIPE", "Invalid pipe value."};
static const Check kInvalidInput{"INVALID_INPUT", "Invalid input value."};

[[noreturn]] static void reject(const Check &check, const std::string &message) {
    if (check.code)
        throw ArgumentError(check.code, check.message);
    throw ArgumentError(message);
}

// Null counts as missing, as on Windows.
static FlValue *lookup(FlValue *args, const char *name) {
    if (args == nullptr || fl_value_get_type(args) != FL_VALUE_TYPE_MAP)
        return nullptr;
    FlValue *value = fl_value_lookup_string(args, name);
    return value == nullptr || fl_value_get_type(value) == FL_VALUE_TYPE_NULL ? nullptr : value;
}

// Required accessors reject a missing, null, wrongly typed or unrepresentable
// value with check.
static std::string string_argument(FlValue *args, const char *name, const Check &check = {}) {
    FlValue *value = lookup(args, name);
    if (value == nullptr || fl_value_get_type(value) != FL_VALUE_TYPE_STRING)
        reject(check, std::string("Expected string: ") + name);
    return fl_value_get_string(value);
}

static std::string optional_string_argument(FlValue *args, const char *name) { return lookup(args, name) ? string_argument(args, name) : std::string(); }

static int int_argument(FlValue *args, const char *name, const Check &check = {}) {
    FlValue *value = lookup(args, name);
    if (value == nullptr || fl_value_get_type(value) != FL_VALUE_TYPE_INT)
        reject(check, std::string("Expected integer: ") + name);
    const int64_t number = fl_value_get_int(value);
    if (number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max())
        reject(check, std::string("Integer out of native range: ") + name);
    return static_cast<int>(number);
}

static int optional_int_argument(FlValue *args, const char *name, int fallback) { return lookup(args, name) ? int_argument(args, name) : fallback; }

static std::list<std::string> strings_argument(FlValue *args, const char *name, const Check &check = {}) {
    FlValue *value = lookup(args, name);
    if (value == nullptr || fl_value_get_type(value) != FL_VALUE_TYPE_LIST)
        reject(check, std::string("Expected list: ") + name);
    std::list<std::string> result;
    const size_t length = fl_value_get_length(value);
    for (size_t i = 0; i < length; i++) {
        FlValue *entry = fl_value_get_list_value(value, i);
        if (fl_value_get_type(entry) != FL_VALUE_TYPE_STRING)
            reject(check, "Expected string list entries.");
        result.emplace_back(fl_value_get_string(entry));
    }
    return result;
}

static std::map<std::string, std::string> mapping_argument(FlValue *args, const char *name) {
    FlValue *value = lookup(args, name);
    if (value == nullptr)
        return {};
    if (fl_value_get_type(value) != FL_VALUE_TYPE_MAP)
        throw ArgumentError("Expected string map.");
    std::map<std::string, std::string> result;
    const size_t length = fl_value_get_length(value);
    for (size_t i = 0; i < length; i++) {
        FlValue *key = fl_value_get_map_key(value, i);
        FlValue *text = fl_value_get_map_value(value, i);
        if (fl_value_get_type(key) != FL_VALUE_TYPE_STRING || fl_value_get_type(text) != FL_VALUE_TYPE_STRING)
            throw ArgumentError("Expected string map entries.");
        result[fl_value_get_string(key)] = fl_value_get_string(text);
    }
    return result;
}

// Returns the call's own byte list. Use it before the handler returns; a worker
// must never ref or unref it, since FlValue reference counts are not atomic.
static FlValue *bytes_argument(FlValue *args, const char *name, const Check &check = {}) {
    FlValue *value = lookup(args, name);
    if (value == nullptr || fl_value_get_type(value) != FL_VALUE_TYPE_UINT8_LIST)
        reject(check, "Expected byte array.");
    return value;
}

// Like Windows and the ObjC isValidPositiveNumber gate: a non-positive
// waitTimeout selects the default.
static int resolve_timeout(FlValue *args) {
    const int fallback = ffmpegkit::AbstractSession::DefaultTimeoutForAsynchronousMessagesInTransmit;
    const int wait = optional_int_argument(args, "waitTimeout", fallback);
    return wait > 0 ? wait : fallback;
}

// All response builders return a newly-owned FlMethodResponse. The caller
// (method_call_cb, or respond_later for async paths) unrefs it after responding.
static FlMethodResponse* resp_take_value(FlValue* value) {
  g_autoptr(FlValue) v = value;
  return FL_METHOD_RESPONSE(fl_method_success_response_new(v));
}
static FlMethodResponse* resp_string(const char* s) {
  return resp_take_value(s != nullptr ? fl_value_new_string(s)
                                      : fl_value_new_null());
}
// For strings that may carry raw FFmpeg output (logs, stack traces).
static FlMethodResponse* resp_utf8_string(const std::string& s) {
  return resp_take_value(new_utf8_value(s));
}
static FlMethodResponse* resp_int(int64_t v) {
  return resp_take_value(fl_value_new_int(v));
}
static FlMethodResponse* resp_bool(bool v) {
  return resp_take_value(fl_value_new_bool(v));
}
static FlMethodResponse* resp_null() {
  return resp_take_value(fl_value_new_null());
}
// Native exception text can carry raw bytes; the codec requires UTF-8.
static FlMethodResponse* resp_error(const char* code, const char* message) {
    gchar *valid = message != nullptr ? g_utf8_make_valid(message, -1) : nullptr;
    FlMethodResponse *response = FL_METHOD_RESPONSE(fl_method_error_response_new(code, valid, nullptr));
    g_free(valid);
    return response;
}
static FlMethodResponse* resp_session_not_found() {
  return resp_error("SESSION_NOT_FOUND", "Session not found.");
}
static FlMethodResponse* resp_not_supported() {
  return resp_error("NOT_SUPPORTED", "Not supported on linux platform.");
}

static std::shared_ptr<ffmpegkit::Session> get_session(FfmpegKitNextFlutterPlugin *self, FlValue *args) {
    const long id = int_argument(args, "sessionId", kInvalidSession);
    auto session = ffmpegkit::FFmpegKitConfig::getSession(id);
    if (session)
        return session;
    auto found_session = self->shutdown_sessions->find(id);
    return found_session == self->shutdown_sessions->end() ? nullptr : found_session->second;
}

// ---------------------------------------------------------------------------
// Event emission (native worker thread -> platform thread hop)
// ---------------------------------------------------------------------------

struct EmitData {
    std::weak_ptr<PluginData> state;
    FlValue *event;
    int kind; // 0: session; 1: log; 2: statistics.
};

// Delivery uses a weak plugin reference and checks admission again on the
// platform thread. A queued event cannot keep a detached plugin alive.
static gboolean emit_on_main(gpointer user_data) {
    std::unique_ptr<EmitData> data(static_cast<EmitData *>(user_data));
    auto state = data->state.lock();
    if (state && !state->executions->IsStopping()) {
        auto *self = static_cast<FfmpegKitNextFlutterPlugin *>(g_weak_ref_get(&state->plugin));
        if (self) {
            if (self->listening && (data->kind != 1 || state->logs_enabled) && (data->kind != 2 || state->statistics_enabled))
                fl_event_channel_send(self->event_channel, data->event, nullptr, nullptr);
            g_object_unref(self);
        }
    }
    fl_value_unref(data->event);
    return G_SOURCE_REMOVE;
}

static void post_event(const std::weak_ptr<PluginData> &weak, FlValue *event, int kind = 0) {
    auto state = weak.lock();
    if (!state || state->executions->IsStopping()) {
        fl_value_unref(event);
        return;
    }
    g_idle_add(emit_on_main, new EmitData{weak, event, kind});
}

static void emit_log(const std::weak_ptr<PluginData> &weak, const std::shared_ptr<ffmpegkit::Log> &log) {
    auto state = weak.lock();
    if (state && state->logs_enabled)
        post_event(weak, to_event_map(EVENT_LOG_CALLBACK_EVENT, to_log_map(log)), 1);
}

static void emit_statistics(const std::weak_ptr<PluginData> &weak, const std::shared_ptr<ffmpegkit::Statistics> &statistics) {
    auto state = weak.lock();
    if (state && state->statistics_enabled)
        post_event(weak, to_event_map(EVENT_STATISTICS_CALLBACK_EVENT, to_statistics_map(statistics)), 2);
}

static void emit_session(const std::weak_ptr<PluginData> &state, const std::shared_ptr<ffmpegkit::Session> &session) {
    if (auto current = state.lock())
        current->executions->FinishAsync(session->getSessionId());
    post_event(state, to_event_map(EVENT_COMPLETE_CALLBACK_EVENT, to_session_map(session)));
}

static void emit_session_deleted(const std::weak_ptr<PluginData> &state, long session_id) {
    FlValue *payload = fl_value_new_map();
    fl_value_set_string_take(payload, KEY_SESSION_ID, fl_value_new_int(session_id));
    post_event(state, to_event_map(EVENT_SESSION_DELETED_CALLBACK_EVENT, payload));
}

class LinuxSessionDeleteListener : public ffmpegkit::SessionDeleteListener {
 public:
   explicit LinuxSessionDeleteListener(const std::shared_ptr<PluginData> &state) : state_(state) {}
   void sessionDeleted(const long sessionId) override { emit_session_deleted(state_, sessionId); }

 private:
   std::weak_ptr<PluginData> state_;
};

static long stream_id(const std::string &url) {
    const auto colon = url.find(':');
    return colon == std::string::npos ? 0 : std::strtol(url.c_str() + colon + 1, nullptr, 10);
}

// ---------------------------------------------------------------------------
// Deferred (asynchronous) responses
// ---------------------------------------------------------------------------

struct DeferredResponse {
    std::shared_ptr<PluginData> state;
    std::shared_ptr<PendingResponse> pending;
    FlMethodResponse *response;
};

static gboolean deliver_response(gpointer user_data) {
    std::unique_ptr<DeferredResponse> data(static_cast<DeferredResponse *>(user_data));
    data->pending->Complete(data->response);
    data->state->executions->PrunePendingResponses();
    return G_SOURCE_REMOVE;
}

static void respond_later(const std::shared_ptr<PluginData> &state, const std::shared_ptr<PendingResponse> &pending, FlMethodResponse *response) { g_idle_add(deliver_response, new DeferredResponse{state, pending, response}); }

// Runs execute on a detached worker. Tracked work holds a lease, so teardown
// cancels it and waits; untracked work (shutdown) is only answered by teardown.
// execute is moved, never copied, into the worker.
template <typename Execute> static FlMethodResponse *enqueue_call(FfmpegKitNextFlutterPlugin *self, FlMethodCall *call, Execute execute, std::function<void()> cancel = {}, bool tracked = true) {
    auto state = *self->state;
    auto pending = std::make_shared<PendingResponse>(call);
    if (!state->executions->AddPendingResponse(pending))
        return resp_error("PLUGIN_SHUTDOWN", "The Flutter plugin is shutting down.");
    try {
        std::thread([state, pending, execute = std::move(execute), cancel, tracked]() {
            std::shared_ptr<ffmpeg_kit_linux::PluginState::Lease> lease;
            if (!tracked) {
                if (state->executions->IsStopping())
                    return;
            } else if (!(lease = state->executions->TryStart(cancel))) {
                respond_later(state, pending, resp_error("PLUGIN_SHUTDOWN", "The Flutter plugin is shutting down."));
                return;
            }
            try {
                respond_later(state, pending, execute());
            } catch (const std::exception &error) {
                respond_later(state, pending, resp_error("EXECUTION_FAILED", error.what()));
            } catch (...) {
                respond_later(state, pending, resp_error("EXECUTION_FAILED", "Native execution failed."));
            }
        }).detach();
    } catch (const std::exception &error) {
        pending->Complete(resp_error("EXECUTION_FAILED", error.what()));
        state->executions->PrunePendingResponses();
    }
    return nullptr;
}

template <typename SessionType, typename Submit> static FlMethodResponse *enqueue_async_execution(FfmpegKitNextFlutterPlugin *self, const std::shared_ptr<SessionType> &session, Submit submit) {
    auto state = *self->state;
    if (!state->executions->TrackAsync(session))
        return resp_error("PLUGIN_SHUTDOWN", "The Flutter plugin is shutting down.");
    try {
        submit();
        if (ffmpegkit::FFmpegKitConfig::isShutdown() && session->getState() == ffmpegkit::SessionStateFailed)
            state->executions->FinishAsync(session->getSessionId());
        return resp_null();
    } catch (const std::exception &error) {
        state->executions->FinishAsync(session->getSessionId());
        return resp_error("EXECUTION_FAILED", error.what());
    }
}

// ---------------------------------------------------------------------------
// Method dispatch
// ---------------------------------------------------------------------------

// Returns an owned FlMethodResponse for synchronous methods. Returns nullptr
// for asynchronous methods that took ownership of the call and will respond
// later via respond_later.
static FlMethodResponse* handle_method_call(FfmpegKitNextFlutterPlugin* self,
                                            FlMethodCall* call) {
    if ((*self->state)->executions->IsStopping())
        return resp_error("PLUGIN_SHUTDOWN", "The Flutter plugin is shutting down.");
    const gchar *m = fl_method_call_get_name(call);
    FlValue *args = fl_method_call_get_args(call);
    if (args != nullptr && fl_value_get_type(args) != FL_VALUE_TYPE_NULL && fl_value_get_type(args) != FL_VALUE_TYPE_MAP)
        throw ArgumentError("Expected arguments map.");

    // -- Abstract session accessors --------------------------------------------
    if (g_strcmp0(m, "shutdown") == 0) {
        const int wait = int_argument(args, "waitTimeout", kInvalidShutdownTimeout);
        self->retain_shutdown_sessions = TRUE;
        auto sessions = ffmpegkit::FFmpegKitConfig::getSessions();
        if (sessions)
            for (const auto &session : *sessions)
                (*self->shutdown_sessions)[session->getSessionId()] = session;
        // Untracked, as on Windows: shutdown(-1) cannot be cancelled and must not
        // hold teardown. Teardown still answers the pending reply.
        return enqueue_call(
            self, call,
            [wait]() {
                try {
                    return resp_bool(ffmpegkit::FFmpegKitConfig::shutdown(wait));
                } catch (const std::exception &error) {
                    return resp_error("SHUTDOWN_FAILED", error.what());
                }
            },
            {}, false);
    }
    // Like Windows, every other method validates an optional waitTimeout first.
    const int timeout = resolve_timeout(args);
    if (g_strcmp0(m, "isShutdown") == 0) {
        return resp_bool(ffmpegkit::FFmpegKitConfig::isShutdown());
    } else if (g_strcmp0(m, "abstractSessionGetEndTime") == 0) {
        auto session = get_session(self, args);
        if (session == nullptr)
            return resp_session_not_found();
        auto end_time = session->getEndTime();
        // getEndTime() is epoch (time_since_epoch()==0) until the session ends.
        if (end_time.time_since_epoch().count() == 0)
            return resp_null();
        return resp_int(to_epoch_millis(end_time));
    } else if (g_strcmp0(m, "abstractSessionGetDuration") == 0) {
        auto session = get_session(self, args);
        if (session == nullptr)
            return resp_session_not_found();
        return resp_int(session->getDuration());
    } else if (g_strcmp0(m, "abstractSessionGetAllLogs") == 0) {
        auto session = get_session(self, args);
        if (session == nullptr)
            return resp_session_not_found();
        // The wait is bounded by timeout, so the lease needs no cancel.
        return enqueue_call(self, call, [session, timeout]() { return resp_take_value(to_log_list_value(session->getAllLogsWithTimeout(timeout))); });
    } else if (g_strcmp0(m, "abstractSessionGetLogs") == 0) {
        auto session = get_session(self, args);
        if (session == nullptr)
            return resp_session_not_found();
        return resp_take_value(to_log_list_value(session->getLogs()));
    } else if (g_strcmp0(m, "abstractSessionGetAllLogsAsString") == 0) {
        auto session = get_session(self, args);
        if (session == nullptr)
            return resp_session_not_found();
        return enqueue_call(self, call, [session, timeout]() { return resp_utf8_string(session->getAllLogsAsStringWithTimeout(timeout)); });
    } else if (g_strcmp0(m, "abstractSessionGetState") == 0) {
        auto session = get_session(self, args);
        if (session == nullptr)
            return resp_session_not_found();
        return resp_int(static_cast<int>(session->getState()));
    } else if (g_strcmp0(m, "abstractSessionGetReturnCode") == 0) {
        auto session = get_session(self, args);
        if (session == nullptr)
            return resp_session_not_found();
        auto return_code = session->getReturnCode();
        if (return_code == nullptr)
            return resp_null();
        return resp_int(return_code->getValue());
    } else if (g_strcmp0(m, "abstractSessionGetFailStackTrace") == 0) {
        auto session = get_session(self, args);
        if (session == nullptr)
            return resp_session_not_found();
        return resp_utf8_string(session->getFailStackTrace());
    } else if (g_strcmp0(m, "thereAreAsynchronousMessagesInTransmit") == 0) {
        auto session = get_session(self, args);
        if (session == nullptr)
            return resp_session_not_found();
        return resp_bool(session->thereAreAsynchronousMessagesInTransmit());

        // -- Arch / platform / version -------------------------------------------
    } else if (g_strcmp0(m, "getArch") == 0) {
        return resp_string(ffmpegkit::ArchDetect::getArch().c_str());
    } else if (g_strcmp0(m, "getPlatform") == 0) {
        return resp_string(FFMPEG_KIT_PLATFORM_NAME);
    } else if (g_strcmp0(m, "getFFmpegVersion") == 0) {
        return resp_string(ffmpegkit::FFmpegKitConfig::getFFmpegVersion().c_str());
    } else if (g_strcmp0(m, "isLTSBuild") == 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    return resp_bool(ffmpegkit::FFmpegKitConfig::isLTSBuild());
#pragma GCC diagnostic pop
    } else if (g_strcmp0(m, "getBuildDate") == 0) {
        return resp_string(ffmpegkit::FFmpegKitConfig::getBuildDate().c_str());
    } else if (g_strcmp0(m, "getPackageName") == 0) {
        return resp_string(ffmpegkit::Packages::getPackageName().c_str());
    } else if (g_strcmp0(m, "getExternalLibraries") == 0) {
        FlValue *list = fl_value_new_list();
        auto libraries = ffmpegkit::Packages::getExternalLibraries();
        if (libraries != nullptr) {
            for (const auto &name : *libraries) {
                fl_value_append_take(list, fl_value_new_string(name.c_str()));
            }
        }
        return resp_take_value(list);

        // -- Session creation -----------------------------------------------------
    } else if (g_strcmp0(m, "ffmpegSession") == 0) {
        auto session = ffmpegkit::FFmpegSession::create(
            strings_argument(args, "arguments", kInvalidArgumentsArray), [state = std::weak_ptr<PluginData>(*self->state)](std::shared_ptr<ffmpegkit::FFmpegSession> s) { emit_session(state, s); },
            [state = std::weak_ptr<PluginData>(*self->state)](std::shared_ptr<ffmpegkit::Log> log) { emit_log(state, log); }, [state = std::weak_ptr<PluginData>(*self->state)](std::shared_ptr<ffmpegkit::Statistics> s) { emit_statistics(state, s); });
        if (self->retain_shutdown_sessions || ffmpegkit::FFmpegKitConfig::isShutdown())
            (*self->shutdown_sessions)[session->getSessionId()] = session;
        return resp_take_value(to_session_map(session));
    } else if (g_strcmp0(m, "ffprobeSession") == 0) {
        auto session = ffmpegkit::FFprobeSession::create(
            strings_argument(args, "arguments", kInvalidArgumentsArray), [state = std::weak_ptr<PluginData>(*self->state)](std::shared_ptr<ffmpegkit::FFprobeSession> s) { emit_session(state, s); },
            [state = std::weak_ptr<PluginData>(*self->state)](std::shared_ptr<ffmpegkit::Log> log) { emit_log(state, log); });
        if (self->retain_shutdown_sessions || ffmpegkit::FFmpegKitConfig::isShutdown())
            (*self->shutdown_sessions)[session->getSessionId()] = session;
        return resp_take_value(to_session_map(session));
    } else if (g_strcmp0(m, "mediaInformationSession") == 0) {
        auto session = ffmpegkit::MediaInformationSession::create(
            strings_argument(args, "arguments", kInvalidArgumentsArray), [state = std::weak_ptr<PluginData>(*self->state)](std::shared_ptr<ffmpegkit::MediaInformationSession> s) { emit_session(state, s); },
            [state = std::weak_ptr<PluginData>(*self->state)](std::shared_ptr<ffmpegkit::Log> log) { emit_log(state, log); });
        if (self->retain_shutdown_sessions || ffmpegkit::FFmpegKitConfig::isShutdown())
            (*self->shutdown_sessions)[session->getSessionId()] = session;
        return resp_take_value(to_session_map(session));

        // -- FFmpeg statistics accessors -----------------------------------------
    } else if (g_strcmp0(m, "ffmpegSessionGetAllStatistics") == 0) {
        auto session = get_session(self, args);
        if (session == nullptr)
            return resp_session_not_found();
        if (!session->isFFmpeg())
            return resp_error("NOT_FFMPEG_SESSION", "A session is found but it does not have the correct "
                                                    "type.");
        auto ffmpeg_session = std::static_pointer_cast<ffmpegkit::FFmpegSession>(session);
        return enqueue_call(self, call, [ffmpeg_session, timeout]() { return resp_take_value(to_statistics_list_value(ffmpeg_session->getAllStatisticsWithTimeout(timeout))); });
    } else if (g_strcmp0(m, "ffmpegSessionGetStatistics") == 0) {
        auto session = get_session(self, args);
        if (session == nullptr)
            return resp_session_not_found();
        if (!session->isFFmpeg())
            return resp_error("NOT_FFMPEG_SESSION", "A session is found but it does not have the correct "
                                                    "type.");
        auto ffmpeg_session = std::static_pointer_cast<ffmpegkit::FFmpegSession>(session);
        return resp_take_value(to_statistics_list_value(ffmpeg_session->getStatistics()));

        // -- Media information ----------------------------------------------------
    } else if (g_strcmp0(m, "getMediaInformation") == 0) {
        auto session = get_session(self, args);
        if (session == nullptr)
            return resp_session_not_found();
        if (!session->isMediaInformation())
            return resp_error("NOT_MEDIA_INFORMATION_SESSION", "A session is found but it does not have the correct "
                                                               "type.");
        auto media_information_session = std::static_pointer_cast<ffmpegkit::MediaInformationSession>(session);
        return resp_take_value(to_media_information_map(media_information_session->getMediaInformation()));
    } else if (g_strcmp0(m, "mediaInformationJsonParserFrom") == 0) {
        const std::string json = string_argument(args, "ffprobeJsonOutput", kInvalidFfprobeJsonOutput);
        try {
            auto media_information = ffmpegkit::MediaInformationJsonParser::fromWithError(json);
            return resp_take_value(to_media_information_map(media_information));
        } catch (const std::exception &) {
            return resp_null();
        }
    } else if (g_strcmp0(m, "mediaInformationJsonParserFromWithError") == 0) {
        const std::string json = string_argument(args, "ffprobeJsonOutput", kInvalidFfprobeJsonOutput);
        try {
            auto media_information = ffmpegkit::MediaInformationJsonParser::fromWithError(json);
            return resp_take_value(to_media_information_map(media_information));
        } catch (const std::exception &) {
            return resp_error("PARSE_FAILED", "Parsing MediaInformation failed with JSON error.");
        }

        // -- Redirection / logging toggles ---------------------------------------
    } else if (g_strcmp0(m, "enableRedirection") == 0) {
        (*self->state)->logs_enabled = true;
        (*self->state)->statistics_enabled = true;
        ffmpegkit::FFmpegKitConfig::enableRedirection();
        return resp_null();
    } else if (g_strcmp0(m, "disableRedirection") == 0) {
        ffmpegkit::FFmpegKitConfig::disableRedirection();
        return resp_null();
    } else if (g_strcmp0(m, "enableLogs") == 0) {
        (*self->state)->logs_enabled = true;
        return resp_null();
    } else if (g_strcmp0(m, "disableLogs") == 0) {
        (*self->state)->logs_enabled = false;
        return resp_null();
    } else if (g_strcmp0(m, "enableStatistics") == 0) {
        (*self->state)->statistics_enabled = true;
        return resp_null();
    } else if (g_strcmp0(m, "disableStatistics") == 0) {
        (*self->state)->statistics_enabled = false;
        return resp_null();

        // -- Fontconfig -----------------------------------------------------------
    } else if (g_strcmp0(m, "setFontconfigConfigurationPath") == 0) {
        ffmpegkit::FFmpegKitConfig::setFontconfigConfigurationPath(string_argument(args, "path", kInvalidPath));
        return resp_null();
        // Like mobile, the specific check runs before the unvalidated font map.
    } else if (g_strcmp0(m, "setFontDirectory") == 0) {
        const std::string directory = string_argument(args, "fontDirectory", kInvalidFontDirectory);
        ffmpegkit::FFmpegKitConfig::setFontDirectory(directory, mapping_argument(args, "fontNameMap"));
        return resp_null();
    } else if (g_strcmp0(m, "setFontDirectoryList") == 0) {
        const auto directories = strings_argument(args, "fontDirectoryList", kInvalidFontDirectoryList);
        ffmpegkit::FFmpegKitConfig::setFontDirectoryList(directories, mapping_argument(args, "fontNameMap"));
        return resp_null();

        // -- Pipes ----------------------------------------------------------------
    } else if (g_strcmp0(m, "registerNewFFmpegPipe") == 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    auto pipe = ffmpegkit::FFmpegKitConfig::registerNewFFmpegPipe();
#pragma GCC diagnostic pop
    return resp_string(pipe ? pipe->c_str() : nullptr);
    } else if (g_strcmp0(m, "closeFFmpegPipe") == 0) {
        const std::string pipe = string_argument(args, "ffmpegPipePath", kInvalidPipePath);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        ffmpegkit::FFmpegKitConfig::closeFFmpegPipe(pipe);
#pragma GCC diagnostic pop
    return resp_null();

    // -- Environment / signals ------------------------------------------------
    } else if (g_strcmp0(m, "setEnvironmentVariable") == 0) {
        // Like mobile, a bad value wins over a bad name.
        const std::string value = string_argument(args, "variableValue", kInvalidVariableValue);
        const std::string name = string_argument(args, "variableName", kInvalidVariableName);
        ffmpegkit::FFmpegKitConfig::setEnvironmentVariable(name, value);
        return resp_null();
    } else if (g_strcmp0(m, "ignoreSignal") == 0) {
        const int idx = int_argument(args, "signal", kInvalidSignal);
        if (idx < 0 || idx > 4)
            return resp_error("INVALID_SIGNAL", "Signal value not supported.");
        ffmpegkit::Signal signal_value;
        switch (idx) {
        case 0:
            signal_value = ffmpegkit::SignalInt;
            break;
        case 1:
            signal_value = ffmpegkit::SignalQuit;
            break;
        case 2:
            signal_value = ffmpegkit::SignalPipe;
            break;
        case 3:
            signal_value = ffmpegkit::SignalTerm;
            break;
        default:
            signal_value = ffmpegkit::SignalXcpu;
            break;
        }
        ffmpegkit::FFmpegKitConfig::ignoreSignal(signal_value);
        return resp_null();

        // -- Synchronous (background) execution -----------------------------------
    } else if (g_strcmp0(m, "ffmpegSessionExecute") == 0) {
        auto session = get_session(self, args);
        if (session == nullptr)
            return resp_session_not_found();
        if (!session->isFFmpeg())
            return resp_error("NOT_FFMPEG_SESSION", "A session is found but it does not have the correct "
                                                    "type.");
        auto ffmpeg_session = std::static_pointer_cast<ffmpegkit::FFmpegSession>(session);
        return enqueue_call(
            self, call,
            [ffmpeg_session]() {
                ffmpegkit::FFmpegKitConfig::ffmpegExecute(ffmpeg_session);
                return resp_null();
            },
            [ffmpeg_session] { ffmpeg_session->cancel(); });
    } else if (g_strcmp0(m, "ffprobeSessionExecute") == 0) {
        auto session = get_session(self, args);
        if (session == nullptr)
            return resp_session_not_found();
        if (!session->isFFprobe())
            return resp_error("NOT_FFPROBE_SESSION", "A session is found but it does not have the correct "
                                                     "type.");
        auto ffprobe_session = std::static_pointer_cast<ffmpegkit::FFprobeSession>(session);
        return enqueue_call(
            self, call,
            [ffprobe_session]() {
                ffmpegkit::FFmpegKitConfig::ffprobeExecute(ffprobe_session);
                return resp_null();
            },
            [ffprobe_session] { ffprobe_session->cancel(); });
    } else if (g_strcmp0(m, "mediaInformationSessionExecute") == 0) {
        auto session = get_session(self, args);
        if (session == nullptr)
            return resp_session_not_found();
        if (!session->isMediaInformation())
            return resp_error("NOT_MEDIA_INFORMATION_SESSION", "A session is found but it does not have the correct "
                                                               "type.");
        auto media_information_session = std::static_pointer_cast<ffmpegkit::MediaInformationSession>(session);
        return enqueue_call(
            self, call,
            [media_information_session, timeout]() {
                ffmpegkit::FFmpegKitConfig::getMediaInformationExecute(media_information_session, timeout);
                return resp_null();
            },
            [media_information_session] { media_information_session->cancel(); });

        // -- Asynchronous execution (returns immediately) -------------------------
    } else if (g_strcmp0(m, "asyncFFmpegSessionExecute") == 0) {
        auto session = get_session(self, args);
        if (session == nullptr)
            return resp_session_not_found();
        if (!session->isFFmpeg())
            return resp_error("NOT_FFMPEG_SESSION", "A session is found but it does not have the correct "
                                                    "type.");
        auto typed = std::static_pointer_cast<ffmpegkit::FFmpegSession>(session);
        return enqueue_async_execution(self, typed, [typed] { ffmpegkit::FFmpegKitConfig::asyncFFmpegExecute(typed); });
    } else if (g_strcmp0(m, "asyncFFprobeSessionExecute") == 0) {
        auto session = get_session(self, args);
        if (session == nullptr)
            return resp_session_not_found();
        if (!session->isFFprobe())
            return resp_error("NOT_FFPROBE_SESSION", "A session is found but it does not have the correct "
                                                     "type.");
        auto typed = std::static_pointer_cast<ffmpegkit::FFprobeSession>(session);
        return enqueue_async_execution(self, typed, [typed] { ffmpegkit::FFmpegKitConfig::asyncFFprobeExecute(typed); });
    } else if (g_strcmp0(m, "asyncMediaInformationSessionExecute") == 0) {
        auto session = get_session(self, args);
        if (session == nullptr)
            return resp_session_not_found();
        if (!session->isMediaInformation())
            return resp_error("NOT_MEDIA_INFORMATION_SESSION", "A session is found but it does not have the correct "
                                                               "type.");
        auto typed = std::static_pointer_cast<ffmpegkit::MediaInformationSession>(session);
        return enqueue_async_execution(self, typed, [typed, timeout] { ffmpegkit::FFmpegKitConfig::asyncGetMediaInformationExecute(typed, timeout); });

        // -- Log level / history / redirection strategy --------------------------
    } else if (g_strcmp0(m, "getLogLevel") == 0) {
        return resp_int(static_cast<int>(ffmpegkit::FFmpegKitConfig::getLogLevel()));
    } else if (g_strcmp0(m, "setLogLevel") == 0) {
        ffmpegkit::FFmpegKitConfig::setLogLevel(static_cast<ffmpegkit::Level>(int_argument(args, "level", kInvalidLevel)));
        return resp_null();
    } else if (g_strcmp0(m, "getSessionHistorySize") == 0) {
        return resp_int(ffmpegkit::FFmpegKitConfig::getSessionHistorySize());
    } else if (g_strcmp0(m, "setSessionHistorySize") == 0) {
        ffmpegkit::FFmpegKitConfig::setSessionHistorySize(int_argument(args, "sessionHistorySize", kInvalidSize));
        return resp_null();
    } else if (g_strcmp0(m, "getLogRedirectionStrategy") == 0) {
        return resp_int(static_cast<int>(ffmpegkit::FFmpegKitConfig::getLogRedirectionStrategy()));
    } else if (g_strcmp0(m, "setLogRedirectionStrategy") == 0) {
        const int strategy = int_argument(args, "strategy", kInvalidLogRedirectionStrategy);
        if (strategy < 0 || strategy > 4)
            return resp_error("INVALID_LOG_REDIRECTION_STRATEGY", "Log redirection strategy value not supported.");
        ffmpegkit::FFmpegKitConfig::setLogRedirectionStrategy(static_cast<ffmpegkit::LogRedirectionStrategy>(strategy));
        return resp_null();
    } else if (g_strcmp0(m, "messagesInTransmit") == 0) {
        return resp_int(ffmpegkit::FFmpegKitConfig::messagesInTransmit(int_argument(args, "sessionId", kInvalidSession)));
    } else if (g_strcmp0(m, "printLoadConfirmation") == 0) {
        static gboolean logged = FALSE;
        if (!logged) {
            logged = TRUE;
            const std::string packageName = ffmpegkit::Packages::getPackageName();
            const std::string packageNamePart = packageName.empty() ? "" : packageName + "-";
            FlValue *version = lookup(args, "version");
            const char *versionText = version != nullptr && fl_value_get_type(version) == FL_VALUE_TYPE_STRING ? fl_value_get_string(version) : nullptr;
            const std::string versionPart = versionText != nullptr && versionText[0] != '\0' ? versionText : "unknown";
            g_print("Loaded ffmpeg-kit-next-flutter-%s%s-%s-%s-%s.\n", packageNamePart.c_str(), FFMPEG_KIT_PLATFORM_NAME, ffmpegkit::ArchDetect::getArch().c_str(), versionPart.c_str(), ffmpegkit::FFmpegKitConfig::getBuildDate().c_str());
        }
        return resp_null();

        // -- Session registry queries --------------------------------------------
    } else if (g_strcmp0(m, "getSession") == 0) {
        return resp_take_value(to_session_map(ffmpegkit::FFmpegKitConfig::getSession(int_argument(args, "sessionId", kInvalidSession))));
    } else if (g_strcmp0(m, "getLastSession") == 0) {
        return resp_take_value(to_session_map(ffmpegkit::FFmpegKitConfig::getLastSession()));
    } else if (g_strcmp0(m, "getLastCompletedSession") == 0) {
        return resp_take_value(to_session_map(ffmpegkit::FFmpegKitConfig::getLastCompletedSession()));
    } else if (g_strcmp0(m, "getSessions") == 0) {
        return resp_take_value(to_session_list_value(ffmpegkit::FFmpegKitConfig::getSessions()));
    } else if (g_strcmp0(m, "clearSessions") == 0) {
        ffmpegkit::FFmpegKitConfig::clearSessions();
        self->shutdown_sessions->clear();
        return resp_null();
    } else if (g_strcmp0(m, "deleteSession") == 0) {
        const long id = int_argument(args, "sessionId", kInvalidSession);
        ffmpegkit::FFmpegKitConfig::deleteSession(id);
        self->shutdown_sessions->erase(id);
        return resp_null();
    } else if (g_strcmp0(m, "getSessionsByState") == 0) {
        const int state = int_argument(args, "state", kInvalidSessionState);
        if (state < 0 || state > 3)
            return resp_error("INVALID_SESSION_STATE", "Session state value not supported.");
        return resp_take_value(to_session_list_value(ffmpegkit::FFmpegKitConfig::getSessionsByState(static_cast<ffmpegkit::SessionState>(state))));
    } else if (g_strcmp0(m, "getFFmpegSessions") == 0) {
        return resp_take_value(to_session_list_value(ffmpegkit::FFmpegKit::listSessions()));
    } else if (g_strcmp0(m, "getFFprobeSessions") == 0) {
        return resp_take_value(to_session_list_value(ffmpegkit::FFprobeKit::listFFprobeSessions()));
    } else if (g_strcmp0(m, "getMediaInformationSessions") == 0) {
        return resp_take_value(to_session_list_value(ffmpegkit::FFprobeKit::listMediaInformationSessions()));

        // -- Cancellation ---------------------------------------------------------
    } else if (g_strcmp0(m, "cancel") == 0) {
        ffmpegkit::FFmpegKit::cancel();
        return resp_null();
    } else if (g_strcmp0(m, "cancelSession") == 0) {
        ffmpegkit::FFmpegKit::cancel(int_argument(args, "sessionId", kInvalidSession));
        return resp_null();

        // -- Packages -------------------------------------------------------------
        // (getPackageName / getExternalLibraries handled above.)

        // -- writeToPipe (background file copy) -----------------------------------
    } else if (g_strcmp0(m, "writeToPipe") == 0) {
        // Like Android and iOS, a bad pipe wins over a bad input.
        const std::string pipe_path = string_argument(args, "pipe", kInvalidPipe);
        const std::string input_path = string_argument(args, "input", kInvalidInput);
        auto state = *self->state;
        return enqueue_call(self, call, [state, input_path, pipe_path]() {
            std::unique_ptr<FILE, decltype(&fclose)> input(fopen(input_path.c_str(), "rb"), fclose);
            if (!input)
                return resp_error("COPY_FAILED", "Failed to open file.");
            int output = -1;
            while (!state->executions->IsStopping()) {
                output = open(pipe_path.c_str(), O_WRONLY | O_NONBLOCK | O_CREAT | O_TRUNC, 0666);
                if (output >= 0)
                    break;
                if (errno != ENXIO && errno != EINTR)
                    return resp_error("COPY_FAILED", "Failed to open pipe.");
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (output < 0)
                return resp_error("PLUGIN_SHUTDOWN", "The Flutter plugin is shutting down.");
            // An abandoned FIFO must fail this worker rather than terminate the
            // process. Block SIGPIPE only here; preserve the process signal policy.
            sigset_t blocked, previous, pending;
            sigemptyset(&blocked);
            sigaddset(&blocked, SIGPIPE);
            sigpending(&pending);
            const bool already_pending = sigismember(&pending, SIGPIPE);
            pthread_sigmask(SIG_BLOCK, &blocked, &previous);
            char buffer[4096];
            bool ok = true;
            size_t count;
            while (!state->executions->IsStopping() && (count = fread(buffer, 1, sizeof(buffer), input.get())) > 0) {
                size_t offset = 0;
                while (offset < count && !state->executions->IsStopping()) {
                    ssize_t written = write(output, buffer + offset, count - offset);
                    if (written > 0)
                        offset += static_cast<size_t>(written);
                    else if (written < 0 && (errno == EAGAIN || errno == EINTR))
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    else {
                        ok = false;
                        break;
                    }
                }
                if (!ok)
                    break;
            }
            ok = ok && !ferror(input.get());
            close(output);
            sigpending(&pending);
            if (!already_pending && sigismember(&pending, SIGPIPE)) {
                int signal_number;
                sigwait(&blocked, &signal_number);
            }
            pthread_sigmask(SIG_SETMASK, &previous, nullptr);
            if (state->executions->IsStopping())
                return resp_error("PLUGIN_SHUTDOWN", "The Flutter plugin is shutting down.");
            return ok ? resp_int(0) : resp_error("COPY_FAILED", "Copy failed.");
        });

        // -- Android-only methods -> NOT_SUPPORTED --------------------------------
    } else if (g_strcmp0(m, "selectDocument") == 0 || g_strcmp0(m, "getSafParameter") == 0 || g_strcmp0(m, "unregisterSafProtocolUrl") == 0 || g_strcmp0(m, "getSupportedCameraIds") == 0) {
        return resp_not_supported();

        // -- ffkit input buffer ---------------------------------------------------
    } else if (g_strcmp0(m, "inputBufferFromByteArray") == 0) {
        // Like mobile, the data check runs before the unvalidated extension. The
        // buffer copies the call's bytes directly.
        FlValue *data = bytes_argument(args, "data", kInvalidData);
        const std::string extension = optional_string_argument(args, "extension");
        auto input_buffer = ffmpegkit::FFmpegKitInputBuffer::fromBytes(fl_value_get_uint8_list(data), fl_value_get_length(data), extension);
        std::string url = input_buffer->getUrl();
        (*self->input_buffers)[url] = input_buffer;
        return resp_string(url.c_str());
    } else if (g_strcmp0(m, "inputBufferClose") == 0) {
        auto it = self->input_buffers->find(string_argument(args, "url", kInvalidInputBufferUrl));
        if (it != self->input_buffers->end()) {
            it->second->close();
            self->input_buffers->erase(it);
        }
        return resp_null();

        // -- ffkit output buffer --------------------------------------------------
    } else if (g_strcmp0(m, "outputBufferCreate") == 0) {
        const std::string extension = optional_string_argument(args, "extension");
        std::shared_ptr<ffmpegkit::FFmpegKitOutputBuffer> output_buffer;
        if (lookup(args, "initialCapacity") && lookup(args, "maxCapacity")) {
            const int initial = int_argument(args, "initialCapacity");
            const int max = int_argument(args, "maxCapacity");
            output_buffer = ffmpegkit::FFmpegKitOutputBuffer::create(extension, initial, max);
        } else {
            output_buffer = ffmpegkit::FFmpegKitOutputBuffer::create(extension);
        }
        std::string url = output_buffer->getUrl();
        (*self->output_buffers)[url] = output_buffer;
        return resp_string(url.c_str());
    } else if (g_strcmp0(m, "outputBufferGetSize") == 0) {
        auto it = self->output_buffers->find(string_argument(args, "url", kInvalidOutputBufferUrl));
        if (it == self->output_buffers->end())
            return resp_error("NOT_FOUND", "Output buffer not found.");
        return resp_int(it->second->getSize());
    } else if (g_strcmp0(m, "outputBufferToByteArray") == 0) {
        auto it = self->output_buffers->find(string_argument(args, "url", kInvalidOutputBufferUrl));
        if (it == self->output_buffers->end())
            return resp_error("NOT_FOUND", "Output buffer not found.");
        auto data = it->second->toByteArray();
        if (data == nullptr)
            return resp_take_value(fl_value_new_uint8_list(nullptr, 0));
        return resp_take_value(fl_value_new_uint8_list(data->data(), data->size()));
    } else if (g_strcmp0(m, "outputBufferClose") == 0) {
        auto it = self->output_buffers->find(string_argument(args, "url", kInvalidOutputBufferUrl));
        if (it != self->output_buffers->end()) {
            it->second->close();
            self->output_buffers->erase(it);
        }
        return resp_null();

        // -- ffkit stream input ---------------------------------------------------
    } else if (g_strcmp0(m, "streamInputCreate") == 0) {
        const std::string extension = optional_string_argument(args, "extension");
        std::shared_ptr<ffmpegkit::FFmpegKitStreamInput> stream_input;
        if (lookup(args, "capacity")) {
            stream_input = ffmpegkit::FFmpegKitStreamInput::create(extension, int_argument(args, "capacity"));
        } else {
            stream_input = ffmpegkit::FFmpegKitStreamInput::create(extension);
        }
        std::string url = stream_input->getUrl();
        (*self->stream_inputs)[url] = stream_input;
        return resp_string(url.c_str());
    } else if (g_strcmp0(m, "streamInputWrite") == 0) {
        // Like mobile, stream I/O validates every argument before the lookup.
        const std::string url = string_argument(args, "url", kInvalidStreamInputArguments);
        FlValue *bytes = bytes_argument(args, "data", kInvalidStreamInputArguments);
        const bool timed = lookup(args, "timeoutMs") != nullptr;
        const int timeout_ms = timed ? int_argument(args, "timeoutMs", kInvalidStreamInputArguments) : 0;
        auto it = self->stream_inputs->find(url);
        if (it == self->stream_inputs->end())
            return resp_error("NOT_FOUND", "Stream input not found.");
        auto stream_input = it->second;
        const long stream = stream_id(it->first);
        // The worker outlives the call's FlValue, so it owns its one copy.
        const uint8_t *source = fl_value_get_uint8_list(bytes);
        std::vector<uint8_t> data(source, source + fl_value_get_length(bytes));
        return enqueue_call(
            self, call,
            [stream_input, data = std::move(data), timed, timeout_ms]() {
                try {
                    int written = timed ? stream_input->write(data, timeout_ms) : stream_input->write(data);
                    return resp_int(written);
                } catch (const std::exception &error) {
                    return resp_error("STREAM_INPUT_WRITE_FAILED", error.what());
                }
            },
            [stream] { ffmpegkit::FFmpegKitConfig::unregisterFFmpegKitStream(stream); });
    } else if (g_strcmp0(m, "streamInputCloseInput") == 0) {
        auto it = self->stream_inputs->find(string_argument(args, "url", kInvalidStreamInputUrl));
        if (it != self->stream_inputs->end())
            it->second->closeInput();
        return resp_null();
    } else if (g_strcmp0(m, "streamInputClose") == 0) {
        auto it = self->stream_inputs->find(string_argument(args, "url", kInvalidStreamInputUrl));
        if (it != self->stream_inputs->end()) {
            it->second->close();
            self->stream_inputs->erase(it);
        }
        return resp_null();

        // -- ffkit stream output --------------------------------------------------
    } else if (g_strcmp0(m, "streamOutputCreate") == 0) {
        const std::string extension = optional_string_argument(args, "extension");
        std::shared_ptr<ffmpegkit::FFmpegKitStreamOutput> stream_output;
        if (lookup(args, "capacity")) {
            stream_output = ffmpegkit::FFmpegKitStreamOutput::create(extension, int_argument(args, "capacity"));
        } else {
            stream_output = ffmpegkit::FFmpegKitStreamOutput::create(extension);
        }
        std::string url = stream_output->getUrl();
        (*self->stream_outputs)[url] = stream_output;
        return resp_string(url.c_str());
    } else if (g_strcmp0(m, "streamOutputRead") == 0) {
        const std::string url = string_argument(args, "url", kInvalidStreamOutputArguments);
        const int max_bytes = int_argument(args, "maxBytes", kInvalidStreamOutputArguments);
        const bool timed = lookup(args, "timeoutMs") != nullptr;
        const int timeout_ms = timed ? int_argument(args, "timeoutMs", kInvalidStreamOutputArguments) : 0;
        auto it = self->stream_outputs->find(url);
        if (it == self->stream_outputs->end())
            return resp_error("NOT_FOUND", "Stream output not found.");
        auto stream_output = it->second;
        const long stream = stream_id(it->first);
        return enqueue_call(
            self, call,
            [stream_output, max_bytes, timed, timeout_ms]() {
                std::shared_ptr<std::vector<uint8_t>> data;
                try {
                    data = timed ? stream_output->read(max_bytes, timeout_ms) : stream_output->read(max_bytes);
                } catch (const std::exception &error) {
                    return resp_error("STREAM_OUTPUT_READ_FAILED", error.what());
                }
                return data == nullptr ? resp_null() : resp_take_value(fl_value_new_uint8_list(data->data(), data->size()));
            },
            [stream] { ffmpegkit::FFmpegKitConfig::unregisterFFmpegKitStream(stream); });
    } else if (g_strcmp0(m, "streamOutputClose") == 0) {
        auto it = self->stream_outputs->find(string_argument(args, "url", kInvalidStreamOutputUrl));
        if (it != self->stream_outputs->end()) {
            it->second->close();
            self->stream_outputs->erase(it);
        }
        return resp_null();
    }

  return FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
}

// ---------------------------------------------------------------------------
// GObject / channel plumbing
// ---------------------------------------------------------------------------

static void method_call_cb(FlMethodChannel* channel, FlMethodCall* method_call,
                           gpointer user_data) {
  FfmpegKitNextFlutterPlugin* self = FFMPEG_KIT_NEXT_FLUTTER_PLUGIN(user_data);
  FlMethodResponse *response;
  // An exception must not unwind through this GLib callback.
  try {
      response = handle_method_call(self, method_call);
  } catch (const ArgumentError &error) {
      response = resp_error(error.code.c_str(), error.what());
  } catch (const std::exception &error) {
      response = resp_error("NATIVE_ERROR", error.what());
  } catch (...) {
      response = resp_error("NATIVE_ERROR", "Unknown native error.");
  }
  if (response != nullptr) {
    fl_method_call_respond(method_call, response, nullptr);
    g_object_unref(response);
  }
  // A nullptr response means an async handler took ownership of the call and
  // will respond later via respond_later.
}

static FlMethodErrorResponse* event_listen_cb(FlEventChannel* channel,
                                              FlValue* args,
                                              gpointer user_data) {
  FfmpegKitNextFlutterPlugin* self = FFMPEG_KIT_NEXT_FLUTTER_PLUGIN(user_data);
  (void)channel;
  (void)args;
  self->listening = !(*self->state)->executions->IsStopping();
  return nullptr;
}

static FlMethodErrorResponse* event_cancel_cb(FlEventChannel* channel,
                                              FlValue* args,
                                              gpointer user_data) {
  FfmpegKitNextFlutterPlugin* self = FFMPEG_KIT_NEXT_FLUTTER_PLUGIN(user_data);
  (void)channel;
  (void)args;
  self->listening = FALSE;
  return nullptr;
}

static void detach_plugin(FfmpegKitNextFlutterPlugin *self) {
    if (!self->state)
        return;
    auto state = *self->state;
    if (!state->executions->BeginShutdown())
        return;
    self->listening = FALSE;
    // Channel destroy notification runs independently of worker references. It
    // is the earliest public Linux embedding hook for engine detachment. Attempt
    // replies now; engine destruction may already have made delivery unavailable.
    state->executions->CompletePendingResponses("PLUGIN_SHUTDOWN", "The Flutter plugin is shutting down.");
    state->executions->CancelAsync();
    if (self->delete_listener && *self->delete_listener)
        ffmpegkit::FFmpegKitConfig::removeSessionDeleteListener(*self->delete_listener);
    const auto inputs = *self->input_buffers;
    const auto outputs = *self->output_buffers;
    const auto stream_inputs = *self->stream_inputs;
    const auto stream_outputs = *self->stream_outputs;
    // Wake blocked stream I/O now by unregistering through the locked native
    // registry. Stream closed flags are atomic; the objects still close after
    // workers drain, together with the buffers.
    for (const auto &resource : stream_inputs)
        ffmpegkit::FFmpegKitConfig::unregisterFFmpegKitStream(stream_id(resource.first));
    for (const auto &resource : stream_outputs)
        ffmpegkit::FFmpegKitConfig::unregisterFFmpegKitStream(stream_id(resource.first));
    // Cancel and drain off the platform thread. Workers keep only state/session
    // ownership and never need a platform reply to release their lease.
    std::thread([state, inputs, outputs, stream_inputs, stream_outputs] {
        state->executions->CancelAndWait();
        for (const auto &resource : inputs)
            resource.second->close();
        for (const auto &resource : outputs)
            resource.second->close();
        for (const auto &resource : stream_inputs)
            resource.second->close();
        for (const auto &resource : stream_outputs)
            resource.second->close();
    }).detach();
}

static void channel_detached(gpointer data) {
    auto *self = FFMPEG_KIT_NEXT_FLUTTER_PLUGIN(data);
    detach_plugin(self);
    g_object_unref(self);
}

static void ffmpeg_kit_next_flutter_plugin_dispose(GObject* object) {
  FfmpegKitNextFlutterPlugin* self = FFMPEG_KIT_NEXT_FLUTTER_PLUGIN(object);
  detach_plugin(self);
  delete self->input_buffers;
  delete self->output_buffers;
  delete self->stream_inputs;
  delete self->stream_outputs;
  delete self->delete_listener;
  delete self->shutdown_sessions;
  self->shutdown_sessions = nullptr;
  self->input_buffers = nullptr;
  self->output_buffers = nullptr;
  self->stream_inputs = nullptr;
  self->stream_outputs = nullptr;
  self->delete_listener = nullptr;
  if (self->event_channel)
      g_clear_object(&self->event_channel);
  delete self->state;
  self->state = nullptr;
  G_OBJECT_CLASS(ffmpeg_kit_next_flutter_plugin_parent_class)->dispose(object);
}

static void ffmpeg_kit_next_flutter_plugin_class_init(
    FfmpegKitNextFlutterPluginClass* klass) {
  G_OBJECT_CLASS(klass)->dispose = ffmpeg_kit_next_flutter_plugin_dispose;
}

static void ffmpeg_kit_next_flutter_plugin_init(
    FfmpegKitNextFlutterPlugin* self) {
    self->state = new std::shared_ptr<PluginData>(std::make_shared<PluginData>());
    g_weak_ref_set(&(*self->state)->plugin, G_OBJECT(self));
    self->retain_shutdown_sessions = FALSE;
    self->shutdown_sessions = new std::map<long, std::shared_ptr<ffmpegkit::Session>>();
    self->event_channel = nullptr;
    self->listening = FALSE;
    self->input_buffers = new std::map<std::string, std::shared_ptr<ffmpegkit::FFmpegKitInputBuffer>>();
    self->output_buffers = new std::map<std::string, std::shared_ptr<ffmpegkit::FFmpegKitOutputBuffer>>();
    self->stream_inputs = new std::map<std::string, std::shared_ptr<ffmpegkit::FFmpegKitStreamInput>>();
    self->stream_outputs = new std::map<std::string, std::shared_ptr<ffmpegkit::FFmpegKitStreamOutput>>();
    self->delete_listener = new std::shared_ptr<ffmpegkit::SessionDeleteListener>();
}

void f_fmpeg_kit_flutter_plugin_register_with_registrar(
    FlPluginRegistrar* registrar) {
  FfmpegKitNextFlutterPlugin* plugin = FFMPEG_KIT_NEXT_FLUTTER_PLUGIN(
      g_object_new(ffmpeg_kit_next_flutter_plugin_get_type(), nullptr));

  g_autoptr(FlStandardMethodCodec) codec = fl_standard_method_codec_new();
  g_autoptr(FlMethodChannel) method_channel = fl_method_channel_new(
      fl_plugin_registrar_get_messenger(registrar), kMethodChannelName,
      FL_METHOD_CODEC(codec));
  fl_method_channel_set_method_call_handler(method_channel, method_call_cb, g_object_ref(plugin), channel_detached);

  g_autoptr(FlStandardMethodCodec) event_codec = fl_standard_method_codec_new();
  g_autoptr(FlEventChannel) event_channel = fl_event_channel_new(fl_plugin_registrar_get_messenger(registrar), kEventChannelName, FL_METHOD_CODEC(event_codec));
  plugin->event_channel = static_cast<FlEventChannel *>(g_object_ref(event_channel));
  fl_event_channel_set_stream_handlers(plugin->event_channel, event_listen_cb, event_cancel_cb, g_object_ref(plugin), channel_detached);

  // The process-global listener and session callbacks retain only weak state.
  *plugin->delete_listener = std::make_shared<LinuxSessionDeleteListener>(*plugin->state);
  ffmpegkit::FFmpegKitConfig::addSessionDeleteListener(*plugin->delete_listener);

  g_object_unref(plugin);
}
