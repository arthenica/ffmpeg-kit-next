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

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

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
#define FFMPEG_KIT_LIBRARY_VERSION "8.1.0"

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

struct _FfmpegKitNextFlutterPlugin {
  GObject parent_instance;
  FlEventChannel* event_channel;
  gboolean listening;
  gboolean logs_enabled;
  gboolean statistics_enabled;

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
        fl_value_set_string_take(map, entry.first.c_str(),
                                 json_value_to_fl_value(entry.second));
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
  if (session->isFFmpeg()) {
    fl_value_set_string_take(m, KEY_SESSION_TYPE,
                             fl_value_new_int(SESSION_TYPE_FFMPEG));
  } else if (session->isFFprobe()) {
    fl_value_set_string_take(m, KEY_SESSION_TYPE,
                             fl_value_new_int(SESSION_TYPE_FFPROBE));
  } else if (session->isMediaInformation()) {
    auto media_information_session =
        std::static_pointer_cast<ffmpegkit::MediaInformationSession>(session);
    fl_value_set_string_take(
        m, KEY_SESSION_MEDIA_INFORMATION,
        to_media_information_map(
            media_information_session->getMediaInformation()));
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

static FlValue* lookup(FlValue* args, const char* name) {
  if (args == nullptr || fl_value_get_type(args) != FL_VALUE_TYPE_MAP) {
    return nullptr;
  }
  return fl_value_lookup_string(args, name);
}

static const gchar* get_string_argument(FlValue* args, const char* name) {
  FlValue* v = lookup(args, name);
  return (v != nullptr && fl_value_get_type(v) == FL_VALUE_TYPE_STRING)
             ? fl_value_get_string(v)
             : nullptr;
}

static int64_t get_int_argument(FlValue* args, const char* name,
                                gboolean* found) {
  FlValue* v = lookup(args, name);
  if (v != nullptr && fl_value_get_type(v) == FL_VALUE_TYPE_INT) {
    if (found != nullptr) *found = TRUE;
    return fl_value_get_int(v);
  }
  if (found != nullptr) *found = FALSE;
  return 0;
}

// Matches the ObjC isValidPositiveNumber gate used for waitTimeout arguments.
static gboolean is_valid_positive_argument(FlValue* args, const char* name) {
  gboolean found = FALSE;
  int64_t value = get_int_argument(args, name, &found);
  return found && value > 0;
}

static int resolve_timeout(FlValue* args) {
  if (is_valid_positive_argument(args, "waitTimeout")) {
    gboolean found = FALSE;
    return static_cast<int>(get_int_argument(args, "waitTimeout", &found));
  }
  return ffmpegkit::AbstractSession::
      DefaultTimeoutForAsynchronousMessagesInTransmit;
}

static std::list<std::string> fl_to_string_list(FlValue* value) {
  std::list<std::string> out;
  if (value != nullptr && fl_value_get_type(value) == FL_VALUE_TYPE_LIST) {
    size_t n = fl_value_get_length(value);
    for (size_t i = 0; i < n; i++) {
      FlValue* e = fl_value_get_list_value(value, i);
      if (fl_value_get_type(e) == FL_VALUE_TYPE_STRING) {
        out.emplace_back(fl_value_get_string(e));
      }
    }
  }
  return out;
}

static std::map<std::string, std::string> fl_to_string_map(FlValue* value) {
  std::map<std::string, std::string> out;
  if (value != nullptr && fl_value_get_type(value) == FL_VALUE_TYPE_MAP) {
    size_t n = fl_value_get_length(value);
    for (size_t i = 0; i < n; i++) {
      FlValue* k = fl_value_get_map_key(value, i);
      FlValue* v = fl_value_get_map_value(value, i);
      if (fl_value_get_type(k) == FL_VALUE_TYPE_STRING &&
          fl_value_get_type(v) == FL_VALUE_TYPE_STRING) {
        out[fl_value_get_string(k)] = fl_value_get_string(v);
      }
    }
  }
  return out;
}

static std::vector<uint8_t> fl_to_uint8_vector(FlValue* value) {
  std::vector<uint8_t> out;
  if (value != nullptr && fl_value_get_type(value) == FL_VALUE_TYPE_UINT8_LIST) {
    const uint8_t* d = fl_value_get_uint8_list(value);
    size_t n = fl_value_get_length(value);
    out.assign(d, d + n);
  }
  return out;
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
static FlMethodResponse* resp_error(const char* code, const char* message) {
  return FL_METHOD_RESPONSE(fl_method_error_response_new(code, message, nullptr));
}
static FlMethodResponse* resp_session_not_found() {
  return resp_error("SESSION_NOT_FOUND", "Session not found.");
}
static FlMethodResponse* resp_not_supported() {
  return resp_error("NOT_SUPPORTED", "Not supported on linux platform.");
}

static std::shared_ptr<ffmpegkit::Session> get_session(FlValue* args) {
  gboolean found = FALSE;
  int64_t id = get_int_argument(args, "sessionId", &found);
  if (!found) return nullptr;
  return ffmpegkit::FFmpegKitConfig::getSession(static_cast<long>(id));
}

// ---------------------------------------------------------------------------
// Event emission (native worker thread -> platform thread hop)
// ---------------------------------------------------------------------------

struct EmitData {
  FfmpegKitNextFlutterPlugin* self;
  FlValue* event;
};

// VERIFY(linux): fl_event_channel_send must be invoked on the platform thread,
// hence the g_idle_add hop from the native FFmpeg worker threads.
static gboolean emit_on_main(gpointer user_data) {
  EmitData* d = static_cast<EmitData*>(user_data);
  if (d->self->listening) {
    fl_event_channel_send(d->self->event_channel, d->event, nullptr, nullptr);
  }
  fl_value_unref(d->event);
  g_object_unref(d->self);
  delete d;
  return G_SOURCE_REMOVE;
}

static void post_event(FfmpegKitNextFlutterPlugin* self, FlValue* event) {
  EmitData* d = new EmitData{
      FFMPEG_KIT_NEXT_FLUTTER_PLUGIN(g_object_ref(self)), event};
  g_idle_add(emit_on_main, d);
}

static void emit_log(FfmpegKitNextFlutterPlugin* self,
                     const std::shared_ptr<ffmpegkit::Log>& log) {
  if (!self->logs_enabled) return;
  post_event(self, to_event_map(EVENT_LOG_CALLBACK_EVENT, to_log_map(log)));
}

static void emit_statistics(
    FfmpegKitNextFlutterPlugin* self,
    const std::shared_ptr<ffmpegkit::Statistics>& statistics) {
  if (!self->statistics_enabled) return;
  post_event(self, to_event_map(EVENT_STATISTICS_CALLBACK_EVENT,
                                to_statistics_map(statistics)));
}

static void emit_session(FfmpegKitNextFlutterPlugin* self,
                         const std::shared_ptr<ffmpegkit::Session>& session) {
  post_event(self,
             to_event_map(EVENT_COMPLETE_CALLBACK_EVENT, to_session_map(session)));
}

static void emit_session_deleted(FfmpegKitNextFlutterPlugin* self,
                                 long session_id) {
  FlValue* payload = fl_value_new_map();
  fl_value_set_string_take(payload, KEY_SESSION_ID, fl_value_new_int(session_id));
  post_event(self,
             to_event_map(EVENT_SESSION_DELETED_CALLBACK_EVENT, payload));
}

class LinuxSessionDeleteListener : public ffmpegkit::SessionDeleteListener {
 public:
  explicit LinuxSessionDeleteListener(FfmpegKitNextFlutterPlugin* plugin)
      : plugin_(plugin) {}
  void sessionDeleted(const long sessionId) override {
    emit_session_deleted(plugin_, sessionId);
  }

 private:
  FfmpegKitNextFlutterPlugin* plugin_;
};

// ---------------------------------------------------------------------------
// Deferred (asynchronous) responses
// ---------------------------------------------------------------------------

struct DeferredResponse {
  FlMethodCall* call;
  FlMethodResponse* response;
};

// VERIFY(linux): a method call may only be responded to on the platform thread;
// worker threads route their response back through this idle hop.
static gboolean deliver_response(gpointer user_data) {
  DeferredResponse* d = static_cast<DeferredResponse*>(user_data);
  fl_method_call_respond(d->call, d->response, nullptr);
  g_object_unref(d->response);
  g_object_unref(d->call);
  delete d;
  return G_SOURCE_REMOVE;
}

// Takes ownership of both refs (the call was g_object_ref'd by the async
// handler; response is freshly built).
static void respond_later(FlMethodCall* call, FlMethodResponse* response) {
  DeferredResponse* d = new DeferredResponse{call, response};
  g_idle_add(deliver_response, d);
}

// ---------------------------------------------------------------------------
// Method dispatch
// ---------------------------------------------------------------------------

// Returns an owned FlMethodResponse for synchronous methods. Returns nullptr
// for asynchronous methods that took ownership of the call and will respond
// later via respond_later.
static FlMethodResponse* handle_method_call(FfmpegKitNextFlutterPlugin* self,
                                            FlMethodCall* call) {
  const gchar* m = fl_method_call_get_name(call);
  FlValue* args = fl_method_call_get_args(call);

  // -- Abstract session accessors --------------------------------------------
  if (g_strcmp0(m, "abstractSessionGetEndTime") == 0) {
    auto session = get_session(args);
    if (session == nullptr) return resp_session_not_found();
    auto end_time = session->getEndTime();
    // getEndTime() is epoch (time_since_epoch()==0) until the session ends.
    if (end_time.time_since_epoch().count() == 0) return resp_null();
    return resp_int(to_epoch_millis(end_time));
  } else if (g_strcmp0(m, "abstractSessionGetDuration") == 0) {
    auto session = get_session(args);
    if (session == nullptr) return resp_session_not_found();
    return resp_int(session->getDuration());
  } else if (g_strcmp0(m, "abstractSessionGetAllLogs") == 0) {
    auto session = get_session(args);
    if (session == nullptr) return resp_session_not_found();
    return resp_take_value(
        to_log_list_value(session->getAllLogsWithTimeout(resolve_timeout(args))));
  } else if (g_strcmp0(m, "abstractSessionGetLogs") == 0) {
    auto session = get_session(args);
    if (session == nullptr) return resp_session_not_found();
    return resp_take_value(to_log_list_value(session->getLogs()));
  } else if (g_strcmp0(m, "abstractSessionGetAllLogsAsString") == 0) {
    auto session = get_session(args);
    if (session == nullptr) return resp_session_not_found();
    return resp_utf8_string(
        session->getAllLogsAsStringWithTimeout(resolve_timeout(args)));
  } else if (g_strcmp0(m, "abstractSessionGetState") == 0) {
    auto session = get_session(args);
    if (session == nullptr) return resp_session_not_found();
    return resp_int(static_cast<int>(session->getState()));
  } else if (g_strcmp0(m, "abstractSessionGetReturnCode") == 0) {
    auto session = get_session(args);
    if (session == nullptr) return resp_session_not_found();
    auto return_code = session->getReturnCode();
    if (return_code == nullptr) return resp_null();
    return resp_int(return_code->getValue());
  } else if (g_strcmp0(m, "abstractSessionGetFailStackTrace") == 0) {
    auto session = get_session(args);
    if (session == nullptr) return resp_session_not_found();
    return resp_utf8_string(session->getFailStackTrace());
  } else if (g_strcmp0(m, "thereAreAsynchronousMessagesInTransmit") == 0) {
    auto session = get_session(args);
    if (session == nullptr) return resp_session_not_found();
    return resp_bool(session->thereAreAsynchronousMessagesInTransmit());

    // -- Arch / platform / version -------------------------------------------
  } else if (g_strcmp0(m, "getArch") == 0) {
    return resp_string(ffmpegkit::ArchDetect::getArch().c_str());
  } else if (g_strcmp0(m, "getPlatform") == 0) {
    return resp_string(FFMPEG_KIT_PLATFORM_NAME);
  } else if (g_strcmp0(m, "getFFmpegVersion") == 0) {
    return resp_string(ffmpegkit::FFmpegKitConfig::getFFmpegVersion().c_str());
  } else if (g_strcmp0(m, "isLTSBuild") == 0) {
    return resp_bool(ffmpegkit::FFmpegKitConfig::isLTSBuild());
  } else if (g_strcmp0(m, "getBuildDate") == 0) {
    return resp_string(ffmpegkit::FFmpegKitConfig::getBuildDate().c_str());
  } else if (g_strcmp0(m, "getPackageName") == 0) {
    return resp_string(ffmpegkit::Packages::getPackageName().c_str());
  } else if (g_strcmp0(m, "getExternalLibraries") == 0) {
    FlValue* list = fl_value_new_list();
    auto libraries = ffmpegkit::Packages::getExternalLibraries();
    if (libraries != nullptr) {
      for (const auto& name : *libraries) {
        fl_value_append_take(list, fl_value_new_string(name.c_str()));
      }
    }
    return resp_take_value(list);

    // -- Session creation -----------------------------------------------------
  } else if (g_strcmp0(m, "ffmpegSession") == 0) {
    auto session = ffmpegkit::FFmpegSession::create(
        fl_to_string_list(lookup(args, "arguments")),
        [self](std::shared_ptr<ffmpegkit::FFmpegSession> s) {
          emit_session(self, s);
        },
        [self](std::shared_ptr<ffmpegkit::Log> log) { emit_log(self, log); },
        [self](std::shared_ptr<ffmpegkit::Statistics> s) {
          emit_statistics(self, s);
        });
    return resp_take_value(to_session_map(session));
  } else if (g_strcmp0(m, "ffprobeSession") == 0) {
    auto session = ffmpegkit::FFprobeSession::create(
        fl_to_string_list(lookup(args, "arguments")),
        [self](std::shared_ptr<ffmpegkit::FFprobeSession> s) {
          emit_session(self, s);
        },
        [self](std::shared_ptr<ffmpegkit::Log> log) { emit_log(self, log); });
    return resp_take_value(to_session_map(session));
  } else if (g_strcmp0(m, "mediaInformationSession") == 0) {
    auto session = ffmpegkit::MediaInformationSession::create(
        fl_to_string_list(lookup(args, "arguments")),
        [self](std::shared_ptr<ffmpegkit::MediaInformationSession> s) {
          emit_session(self, s);
        },
        [self](std::shared_ptr<ffmpegkit::Log> log) { emit_log(self, log); });
    return resp_take_value(to_session_map(session));

    // -- FFmpeg statistics accessors -----------------------------------------
  } else if (g_strcmp0(m, "ffmpegSessionGetAllStatistics") == 0) {
    auto session = get_session(args);
    if (session == nullptr) return resp_session_not_found();
    if (!session->isFFmpeg())
      return resp_error("NOT_FFMPEG_SESSION",
                        "A session is found but it does not have the correct "
                        "type.");
    auto ffmpeg_session =
        std::static_pointer_cast<ffmpegkit::FFmpegSession>(session);
    return resp_take_value(to_statistics_list_value(
        ffmpeg_session->getAllStatisticsWithTimeout(resolve_timeout(args))));
  } else if (g_strcmp0(m, "ffmpegSessionGetStatistics") == 0) {
    auto session = get_session(args);
    if (session == nullptr) return resp_session_not_found();
    if (!session->isFFmpeg())
      return resp_error("NOT_FFMPEG_SESSION",
                        "A session is found but it does not have the correct "
                        "type.");
    auto ffmpeg_session =
        std::static_pointer_cast<ffmpegkit::FFmpegSession>(session);
    return resp_take_value(
        to_statistics_list_value(ffmpeg_session->getStatistics()));

    // -- Media information ----------------------------------------------------
  } else if (g_strcmp0(m, "getMediaInformation") == 0) {
    auto session = get_session(args);
    if (session == nullptr) return resp_session_not_found();
    if (!session->isMediaInformation())
      return resp_error("NOT_MEDIA_INFORMATION_SESSION",
                        "A session is found but it does not have the correct "
                        "type.");
    auto media_information_session =
        std::static_pointer_cast<ffmpegkit::MediaInformationSession>(session);
    return resp_take_value(to_media_information_map(
        media_information_session->getMediaInformation()));
  } else if (g_strcmp0(m, "mediaInformationJsonParserFrom") == 0) {
    const gchar* json = get_string_argument(args, "ffprobeJsonOutput");
    try {
      auto media_information =
          ffmpegkit::MediaInformationJsonParser::fromWithError(json ? json : "");
      return resp_take_value(to_media_information_map(media_information));
    } catch (const std::exception&) {
      return resp_null();
    }
  } else if (g_strcmp0(m, "mediaInformationJsonParserFromWithError") == 0) {
    const gchar* json = get_string_argument(args, "ffprobeJsonOutput");
    try {
      auto media_information =
          ffmpegkit::MediaInformationJsonParser::fromWithError(json ? json : "");
      return resp_take_value(to_media_information_map(media_information));
    } catch (const std::exception&) {
      return resp_error("PARSE_FAILED",
                        "Parsing MediaInformation failed with JSON error.");
    }

    // -- Redirection / logging toggles ---------------------------------------
  } else if (g_strcmp0(m, "enableRedirection") == 0) {
    self->logs_enabled = TRUE;
    self->statistics_enabled = TRUE;
    ffmpegkit::FFmpegKitConfig::enableRedirection();
    return resp_null();
  } else if (g_strcmp0(m, "disableRedirection") == 0) {
    ffmpegkit::FFmpegKitConfig::disableRedirection();
    return resp_null();
  } else if (g_strcmp0(m, "enableLogs") == 0) {
    self->logs_enabled = TRUE;
    return resp_null();
  } else if (g_strcmp0(m, "disableLogs") == 0) {
    self->logs_enabled = FALSE;
    return resp_null();
  } else if (g_strcmp0(m, "enableStatistics") == 0) {
    self->statistics_enabled = TRUE;
    return resp_null();
  } else if (g_strcmp0(m, "disableStatistics") == 0) {
    self->statistics_enabled = FALSE;
    return resp_null();

    // -- Fontconfig -----------------------------------------------------------
  } else if (g_strcmp0(m, "setFontconfigConfigurationPath") == 0) {
    const gchar* path = get_string_argument(args, "path");
    if (path != nullptr)
      ffmpegkit::FFmpegKitConfig::setFontconfigConfigurationPath(path);
    return resp_null();
  } else if (g_strcmp0(m, "setFontDirectory") == 0) {
    const gchar* dir = get_string_argument(args, "fontDirectory");
    ffmpegkit::FFmpegKitConfig::setFontDirectory(
        dir ? dir : "", fl_to_string_map(lookup(args, "fontNameMap")));
    return resp_null();
  } else if (g_strcmp0(m, "setFontDirectoryList") == 0) {
    ffmpegkit::FFmpegKitConfig::setFontDirectoryList(
        fl_to_string_list(lookup(args, "fontDirectoryList")),
        fl_to_string_map(lookup(args, "fontNameMap")));
    return resp_null();

    // -- Pipes ----------------------------------------------------------------
  } else if (g_strcmp0(m, "registerNewFFmpegPipe") == 0) {
    auto pipe = ffmpegkit::FFmpegKitConfig::registerNewFFmpegPipe();
    return resp_string(pipe ? pipe->c_str() : nullptr);
  } else if (g_strcmp0(m, "closeFFmpegPipe") == 0) {
    const gchar* pipe = get_string_argument(args, "ffmpegPipePath");
    if (pipe != nullptr) ffmpegkit::FFmpegKitConfig::closeFFmpegPipe(pipe);
    return resp_null();

    // -- Environment / signals ------------------------------------------------
  } else if (g_strcmp0(m, "setEnvironmentVariable") == 0) {
    const gchar* name = get_string_argument(args, "variableName");
    const gchar* value = get_string_argument(args, "variableValue");
    if (name != nullptr && value != nullptr)
      ffmpegkit::FFmpegKitConfig::setEnvironmentVariable(name, value);
    return resp_null();
  } else if (g_strcmp0(m, "ignoreSignal") == 0) {
    gboolean found = FALSE;
    int64_t idx = get_int_argument(args, "signal", &found);
    if (!found || idx < 0 || idx > 4)
      return resp_error("INVALID_SIGNAL", "Signal value not supported.");
    ffmpegkit::Signal signal_value;
    switch (idx) {
      case 0: signal_value = ffmpegkit::SignalInt; break;
      case 1: signal_value = ffmpegkit::SignalQuit; break;
      case 2: signal_value = ffmpegkit::SignalPipe; break;
      case 3: signal_value = ffmpegkit::SignalTerm; break;
      default: signal_value = ffmpegkit::SignalXcpu; break;
    }
    ffmpegkit::FFmpegKitConfig::ignoreSignal(signal_value);
    return resp_null();

    // -- Synchronous (background) execution -----------------------------------
  } else if (g_strcmp0(m, "ffmpegSessionExecute") == 0) {
    auto session = get_session(args);
    if (session == nullptr) return resp_session_not_found();
    if (!session->isFFmpeg())
      return resp_error("NOT_FFMPEG_SESSION",
                        "A session is found but it does not have the correct "
                        "type.");
    auto ffmpeg_session =
        std::static_pointer_cast<ffmpegkit::FFmpegSession>(session);
    g_object_ref(call);
    std::thread([call, ffmpeg_session]() {
      ffmpegkit::FFmpegKitConfig::ffmpegExecute(ffmpeg_session);
      respond_later(call, resp_null());
    }).detach();
    return nullptr;
  } else if (g_strcmp0(m, "ffprobeSessionExecute") == 0) {
    auto session = get_session(args);
    if (session == nullptr) return resp_session_not_found();
    if (!session->isFFprobe())
      return resp_error("NOT_FFPROBE_SESSION",
                        "A session is found but it does not have the correct "
                        "type.");
    auto ffprobe_session =
        std::static_pointer_cast<ffmpegkit::FFprobeSession>(session);
    g_object_ref(call);
    std::thread([call, ffprobe_session]() {
      ffmpegkit::FFmpegKitConfig::ffprobeExecute(ffprobe_session);
      respond_later(call, resp_null());
    }).detach();
    return nullptr;
  } else if (g_strcmp0(m, "mediaInformationSessionExecute") == 0) {
    auto session = get_session(args);
    if (session == nullptr) return resp_session_not_found();
    if (!session->isMediaInformation())
      return resp_error("NOT_MEDIA_INFORMATION_SESSION",
                        "A session is found but it does not have the correct "
                        "type.");
    auto media_information_session =
        std::static_pointer_cast<ffmpegkit::MediaInformationSession>(session);
    int timeout = resolve_timeout(args);
    g_object_ref(call);
    std::thread([call, media_information_session, timeout]() {
      ffmpegkit::FFmpegKitConfig::getMediaInformationExecute(
          media_information_session, timeout);
      respond_later(call, resp_null());
    }).detach();
    return nullptr;

    // -- Asynchronous execution (returns immediately) -------------------------
  } else if (g_strcmp0(m, "asyncFFmpegSessionExecute") == 0) {
    auto session = get_session(args);
    if (session == nullptr) return resp_session_not_found();
    if (!session->isFFmpeg())
      return resp_error("NOT_FFMPEG_SESSION",
                        "A session is found but it does not have the correct "
                        "type.");
    ffmpegkit::FFmpegKitConfig::asyncFFmpegExecute(
        std::static_pointer_cast<ffmpegkit::FFmpegSession>(session));
    return resp_null();
  } else if (g_strcmp0(m, "asyncFFprobeSessionExecute") == 0) {
    auto session = get_session(args);
    if (session == nullptr) return resp_session_not_found();
    if (!session->isFFprobe())
      return resp_error("NOT_FFPROBE_SESSION",
                        "A session is found but it does not have the correct "
                        "type.");
    ffmpegkit::FFmpegKitConfig::asyncFFprobeExecute(
        std::static_pointer_cast<ffmpegkit::FFprobeSession>(session));
    return resp_null();
  } else if (g_strcmp0(m, "asyncMediaInformationSessionExecute") == 0) {
    auto session = get_session(args);
    if (session == nullptr) return resp_session_not_found();
    if (!session->isMediaInformation())
      return resp_error("NOT_MEDIA_INFORMATION_SESSION",
                        "A session is found but it does not have the correct "
                        "type.");
    ffmpegkit::FFmpegKitConfig::asyncGetMediaInformationExecute(
        std::static_pointer_cast<ffmpegkit::MediaInformationSession>(session),
        resolve_timeout(args));
    return resp_null();

    // -- Log level / history / redirection strategy --------------------------
  } else if (g_strcmp0(m, "getLogLevel") == 0) {
    return resp_int(static_cast<int>(ffmpegkit::FFmpegKitConfig::getLogLevel()));
  } else if (g_strcmp0(m, "setLogLevel") == 0) {
    gboolean found = FALSE;
    int64_t level = get_int_argument(args, "level", &found);
    if (found)
      ffmpegkit::FFmpegKitConfig::setLogLevel(
          static_cast<ffmpegkit::Level>(level));
    return resp_null();
  } else if (g_strcmp0(m, "getSessionHistorySize") == 0) {
    return resp_int(ffmpegkit::FFmpegKitConfig::getSessionHistorySize());
  } else if (g_strcmp0(m, "setSessionHistorySize") == 0) {
    gboolean found = FALSE;
    int64_t size = get_int_argument(args, "sessionHistorySize", &found);
    if (found)
      ffmpegkit::FFmpegKitConfig::setSessionHistorySize(static_cast<int>(size));
    return resp_null();
  } else if (g_strcmp0(m, "getLogRedirectionStrategy") == 0) {
    return resp_int(static_cast<int>(
        ffmpegkit::FFmpegKitConfig::getLogRedirectionStrategy()));
  } else if (g_strcmp0(m, "setLogRedirectionStrategy") == 0) {
    gboolean found = FALSE;
    int64_t strategy = get_int_argument(args, "strategy", &found);
    if (!found || strategy < 0 || strategy > 4)
      return resp_error("INVALID_LOG_REDIRECTION_STRATEGY",
                        "Log redirection strategy value not supported.");
    ffmpegkit::FFmpegKitConfig::setLogRedirectionStrategy(
        static_cast<ffmpegkit::LogRedirectionStrategy>(strategy));
    return resp_null();
  } else if (g_strcmp0(m, "messagesInTransmit") == 0) {
    gboolean found = FALSE;
    int64_t id = get_int_argument(args, "sessionId", &found);
    return resp_int(
        ffmpegkit::FFmpegKitConfig::messagesInTransmit(static_cast<long>(id)));
  } else if (g_strcmp0(m, "printLoadConfirmation") == 0) {
    static gboolean logged = FALSE;
    if (!logged) {
      logged = TRUE;
      g_message("Loaded ffmpeg-kit-next-flutter-%s-%s-%s.",
                FFMPEG_KIT_PLATFORM_NAME,
                ffmpegkit::ArchDetect::getArch().c_str(),
                FFMPEG_KIT_LIBRARY_VERSION);
    }
    return resp_null();

    // -- Session registry queries --------------------------------------------
  } else if (g_strcmp0(m, "getSession") == 0) {
    auto session = get_session(args);
    if (session == nullptr) return resp_session_not_found();
    return resp_take_value(to_session_map(session));
  } else if (g_strcmp0(m, "getLastSession") == 0) {
    return resp_take_value(
        to_session_map(ffmpegkit::FFmpegKitConfig::getLastSession()));
  } else if (g_strcmp0(m, "getLastCompletedSession") == 0) {
    return resp_take_value(
        to_session_map(ffmpegkit::FFmpegKitConfig::getLastCompletedSession()));
  } else if (g_strcmp0(m, "getSessions") == 0) {
    return resp_take_value(
        to_session_list_value(ffmpegkit::FFmpegKitConfig::getSessions()));
  } else if (g_strcmp0(m, "clearSessions") == 0) {
    ffmpegkit::FFmpegKitConfig::clearSessions();
    return resp_null();
  } else if (g_strcmp0(m, "deleteSession") == 0) {
    gboolean found = FALSE;
    int64_t id = get_int_argument(args, "sessionId", &found);
    if (found)
      ffmpegkit::FFmpegKitConfig::deleteSession(static_cast<long>(id));
    return resp_null();
  } else if (g_strcmp0(m, "getSessionsByState") == 0) {
    gboolean found = FALSE;
    int64_t state = get_int_argument(args, "state", &found);
    if (!found || state < 0 || state > 3)
      return resp_error("INVALID_SESSION_STATE",
                        "Session state value not supported.");
    return resp_take_value(
        to_session_list_value(ffmpegkit::FFmpegKitConfig::getSessionsByState(
            static_cast<ffmpegkit::SessionState>(state))));
  } else if (g_strcmp0(m, "getFFmpegSessions") == 0) {
    return resp_take_value(
        to_session_list_value(ffmpegkit::FFmpegKit::listSessions()));
  } else if (g_strcmp0(m, "getFFprobeSessions") == 0) {
    return resp_take_value(
        to_session_list_value(ffmpegkit::FFprobeKit::listFFprobeSessions()));
  } else if (g_strcmp0(m, "getMediaInformationSessions") == 0) {
    return resp_take_value(to_session_list_value(
        ffmpegkit::FFprobeKit::listMediaInformationSessions()));

    // -- Cancellation ---------------------------------------------------------
  } else if (g_strcmp0(m, "cancel") == 0) {
    ffmpegkit::FFmpegKit::cancel();
    return resp_null();
  } else if (g_strcmp0(m, "cancelSession") == 0) {
    gboolean found = FALSE;
    int64_t id = get_int_argument(args, "sessionId", &found);
    if (found) ffmpegkit::FFmpegKit::cancel(static_cast<long>(id));
    return resp_null();

    // -- Packages -------------------------------------------------------------
    // (getPackageName / getExternalLibraries handled above.)

    // -- writeToPipe (background file copy) -----------------------------------
  } else if (g_strcmp0(m, "writeToPipe") == 0) {
    const gchar* input = get_string_argument(args, "input");
    const gchar* pipe = get_string_argument(args, "pipe");
    if (input == nullptr || pipe == nullptr)
      return resp_error("INVALID_ARGUMENTS", "input and pipe are required.");
    std::string input_path(input);
    std::string pipe_path(pipe);
    g_object_ref(call);
    std::thread([call, input_path, pipe_path]() {
      FILE* in = fopen(input_path.c_str(), "rb");
      if (in == nullptr) {
        respond_later(call, resp_error("COPY_FAILED", "Failed to open file."));
        return;
      }
      FILE* out = fopen(pipe_path.c_str(), "wb");
      if (out == nullptr) {
        fclose(in);
        respond_later(call, resp_error("COPY_FAILED", "Failed to open pipe."));
        return;
      }
      char buffer[4096];
      size_t read_bytes;
      bool ok = true;
      while ((read_bytes = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        if (fwrite(buffer, 1, read_bytes, out) != read_bytes) {
          ok = false;
          break;
        }
      }
      fclose(in);
      fclose(out);
      respond_later(call,
                    ok ? resp_int(0) : resp_error("COPY_FAILED", "Copy failed."));
    }).detach();
    return nullptr;

    // -- Android-only methods -> NOT_SUPPORTED --------------------------------
  } else if (g_strcmp0(m, "selectDocument") == 0 ||
             g_strcmp0(m, "getSafParameter") == 0 ||
             g_strcmp0(m, "unregisterSafProtocolUrl") == 0 ||
             g_strcmp0(m, "getSupportedCameraIds") == 0) {
    return resp_not_supported();

    // -- ffkit input buffer ---------------------------------------------------
  } else if (g_strcmp0(m, "inputBufferFromByteArray") == 0) {
    const gchar* extension = get_string_argument(args, "extension");
    auto input_buffer = ffmpegkit::FFmpegKitInputBuffer::fromByteArray(
        fl_to_uint8_vector(lookup(args, "data")), extension ? extension : "");
    std::string url = input_buffer->getUrl();
    (*self->input_buffers)[url] = input_buffer;
    return resp_string(url.c_str());
  } else if (g_strcmp0(m, "inputBufferClose") == 0) {
    const gchar* url = get_string_argument(args, "url");
    if (url != nullptr) {
      auto it = self->input_buffers->find(url);
      if (it != self->input_buffers->end()) {
        it->second->close();
        self->input_buffers->erase(it);
      }
    }
    return resp_null();

    // -- ffkit output buffer --------------------------------------------------
  } else if (g_strcmp0(m, "outputBufferCreate") == 0) {
    const gchar* extension = get_string_argument(args, "extension");
    gboolean has_initial = FALSE, has_max = FALSE;
    int64_t initial = get_int_argument(args, "initialCapacity", &has_initial);
    int64_t max = get_int_argument(args, "maxCapacity", &has_max);
    std::shared_ptr<ffmpegkit::FFmpegKitOutputBuffer> output_buffer;
    if (has_initial && has_max) {
      output_buffer = ffmpegkit::FFmpegKitOutputBuffer::create(
          extension ? extension : "", static_cast<long>(initial),
          static_cast<long>(max));
    } else {
      output_buffer =
          ffmpegkit::FFmpegKitOutputBuffer::create(extension ? extension : "");
    }
    std::string url = output_buffer->getUrl();
    (*self->output_buffers)[url] = output_buffer;
    return resp_string(url.c_str());
  } else if (g_strcmp0(m, "outputBufferGetSize") == 0) {
    const gchar* url = get_string_argument(args, "url");
    auto it = url ? self->output_buffers->find(url)
                  : self->output_buffers->end();
    if (it == self->output_buffers->end())
      return resp_error("NOT_FOUND", "Output buffer not found.");
    return resp_int(it->second->getSize());
  } else if (g_strcmp0(m, "outputBufferToByteArray") == 0) {
    const gchar* url = get_string_argument(args, "url");
    auto it = url ? self->output_buffers->find(url)
                  : self->output_buffers->end();
    if (it == self->output_buffers->end())
      return resp_error("NOT_FOUND", "Output buffer not found.");
    auto data = it->second->toByteArray();
    if (data == nullptr) return resp_take_value(fl_value_new_uint8_list(nullptr, 0));
    return resp_take_value(fl_value_new_uint8_list(data->data(), data->size()));
  } else if (g_strcmp0(m, "outputBufferClose") == 0) {
    const gchar* url = get_string_argument(args, "url");
    if (url != nullptr) {
      auto it = self->output_buffers->find(url);
      if (it != self->output_buffers->end()) {
        it->second->close();
        self->output_buffers->erase(it);
      }
    }
    return resp_null();

    // -- ffkit stream input ---------------------------------------------------
  } else if (g_strcmp0(m, "streamInputCreate") == 0) {
    const gchar* extension = get_string_argument(args, "extension");
    gboolean has_capacity = FALSE;
    int64_t capacity = get_int_argument(args, "capacity", &has_capacity);
    std::shared_ptr<ffmpegkit::FFmpegKitStreamInput> stream_input;
    if (has_capacity) {
      stream_input = ffmpegkit::FFmpegKitStreamInput::create(
          extension ? extension : "", static_cast<long>(capacity));
    } else {
      stream_input =
          ffmpegkit::FFmpegKitStreamInput::create(extension ? extension : "");
    }
    std::string url = stream_input->getUrl();
    (*self->stream_inputs)[url] = stream_input;
    return resp_string(url.c_str());
  } else if (g_strcmp0(m, "streamInputWrite") == 0) {
    const gchar* url = get_string_argument(args, "url");
    auto it = url ? self->stream_inputs->find(url) : self->stream_inputs->end();
    if (it == self->stream_inputs->end())
      return resp_error("NOT_FOUND", "Stream input not found.");
    auto stream_input = it->second;
    std::vector<uint8_t> data = fl_to_uint8_vector(lookup(args, "data"));
    gboolean has_timeout = FALSE;
    int64_t timeout = get_int_argument(args, "timeoutMs", &has_timeout);
    g_object_ref(call);
    std::thread([call, stream_input, data, has_timeout, timeout]() {
      int written = has_timeout
                        ? stream_input->write(data, static_cast<int>(timeout))
                        : stream_input->write(data);
      respond_later(call, resp_int(written));
    }).detach();
    return nullptr;
  } else if (g_strcmp0(m, "streamInputCloseInput") == 0) {
    const gchar* url = get_string_argument(args, "url");
    if (url != nullptr) {
      auto it = self->stream_inputs->find(url);
      if (it != self->stream_inputs->end()) it->second->closeInput();
    }
    return resp_null();
  } else if (g_strcmp0(m, "streamInputClose") == 0) {
    const gchar* url = get_string_argument(args, "url");
    if (url != nullptr) {
      auto it = self->stream_inputs->find(url);
      if (it != self->stream_inputs->end()) {
        it->second->close();
        self->stream_inputs->erase(it);
      }
    }
    return resp_null();

    // -- ffkit stream output --------------------------------------------------
  } else if (g_strcmp0(m, "streamOutputCreate") == 0) {
    const gchar* extension = get_string_argument(args, "extension");
    gboolean has_capacity = FALSE;
    int64_t capacity = get_int_argument(args, "capacity", &has_capacity);
    std::shared_ptr<ffmpegkit::FFmpegKitStreamOutput> stream_output;
    if (has_capacity) {
      stream_output = ffmpegkit::FFmpegKitStreamOutput::create(
          extension ? extension : "", static_cast<long>(capacity));
    } else {
      stream_output =
          ffmpegkit::FFmpegKitStreamOutput::create(extension ? extension : "");
    }
    std::string url = stream_output->getUrl();
    (*self->stream_outputs)[url] = stream_output;
    return resp_string(url.c_str());
  } else if (g_strcmp0(m, "streamOutputRead") == 0) {
    const gchar* url = get_string_argument(args, "url");
    auto it = url ? self->stream_outputs->find(url)
                  : self->stream_outputs->end();
    if (it == self->stream_outputs->end())
      return resp_error("NOT_FOUND", "Stream output not found.");
    auto stream_output = it->second;
    gboolean has_max = FALSE, has_timeout = FALSE;
    int64_t max_bytes = get_int_argument(args, "maxBytes", &has_max);
    int64_t timeout = get_int_argument(args, "timeoutMs", &has_timeout);
    g_object_ref(call);
    std::thread([call, stream_output, max_bytes, has_timeout, timeout]() {
      auto data = has_timeout ? stream_output->read(static_cast<int>(max_bytes),
                                                    static_cast<int>(timeout))
                              : stream_output->read(static_cast<int>(max_bytes));
      if (data == nullptr) {
        respond_later(call, resp_null());
      } else {
        respond_later(call, resp_take_value(fl_value_new_uint8_list(
                                data->data(), data->size())));
      }
    }).detach();
    return nullptr;
  } else if (g_strcmp0(m, "streamOutputClose") == 0) {
    const gchar* url = get_string_argument(args, "url");
    if (url != nullptr) {
      auto it = self->stream_outputs->find(url);
      if (it != self->stream_outputs->end()) {
        it->second->close();
        self->stream_outputs->erase(it);
      }
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
  FlMethodResponse* response = handle_method_call(self, method_call);
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
  self->listening = TRUE;
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

static void ffmpeg_kit_next_flutter_plugin_dispose(GObject* object) {
  FfmpegKitNextFlutterPlugin* self = FFMPEG_KIT_NEXT_FLUTTER_PLUGIN(object);
  delete self->input_buffers;
  delete self->output_buffers;
  delete self->stream_inputs;
  delete self->stream_outputs;
  delete self->delete_listener;
  self->input_buffers = nullptr;
  self->output_buffers = nullptr;
  self->stream_inputs = nullptr;
  self->stream_outputs = nullptr;
  self->delete_listener = nullptr;
  G_OBJECT_CLASS(ffmpeg_kit_next_flutter_plugin_parent_class)->dispose(object);
}

static void ffmpeg_kit_next_flutter_plugin_class_init(
    FfmpegKitNextFlutterPluginClass* klass) {
  G_OBJECT_CLASS(klass)->dispose = ffmpeg_kit_next_flutter_plugin_dispose;
}

static void ffmpeg_kit_next_flutter_plugin_init(
    FfmpegKitNextFlutterPlugin* self) {
  self->event_channel = nullptr;
  self->listening = FALSE;
  self->logs_enabled = FALSE;
  self->statistics_enabled = FALSE;
  self->input_buffers =
      new std::map<std::string,
                   std::shared_ptr<ffmpegkit::FFmpegKitInputBuffer>>();
  self->output_buffers =
      new std::map<std::string,
                   std::shared_ptr<ffmpegkit::FFmpegKitOutputBuffer>>();
  self->stream_inputs =
      new std::map<std::string,
                   std::shared_ptr<ffmpegkit::FFmpegKitStreamInput>>();
  self->stream_outputs =
      new std::map<std::string,
                   std::shared_ptr<ffmpegkit::FFmpegKitStreamOutput>>();
  self->delete_listener =
      new std::shared_ptr<ffmpegkit::SessionDeleteListener>();
}

void f_fmpeg_kit_flutter_plugin_register_with_registrar(
    FlPluginRegistrar* registrar) {
  FfmpegKitNextFlutterPlugin* plugin = FFMPEG_KIT_NEXT_FLUTTER_PLUGIN(
      g_object_new(ffmpeg_kit_next_flutter_plugin_get_type(), nullptr));

  g_autoptr(FlStandardMethodCodec) codec = fl_standard_method_codec_new();
  g_autoptr(FlMethodChannel) method_channel = fl_method_channel_new(
      fl_plugin_registrar_get_messenger(registrar), kMethodChannelName,
      FL_METHOD_CODEC(codec));
  fl_method_channel_set_method_call_handler(
      method_channel, method_call_cb, g_object_ref(plugin), g_object_unref);

  g_autoptr(FlStandardMethodCodec) event_codec = fl_standard_method_codec_new();
  plugin->event_channel = fl_event_channel_new(
      fl_plugin_registrar_get_messenger(registrar), kEventChannelName,
      FL_METHOD_CODEC(event_codec));
  fl_event_channel_set_stream_handlers(plugin->event_channel, event_listen_cb,
                                       event_cancel_cb, g_object_ref(plugin),
                                       g_object_unref);

  // The session delete listener is process-global; the plugin outlives it.
  *plugin->delete_listener =
      std::make_shared<LinuxSessionDeleteListener>(plugin);
  ffmpegkit::FFmpegKitConfig::addSessionDeleteListener(*plugin->delete_listener);

  g_object_unref(plugin);
}
