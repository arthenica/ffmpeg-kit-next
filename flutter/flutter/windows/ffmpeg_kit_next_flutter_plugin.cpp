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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General License for more details.
 *
 * You should have received a copy of the GNU Lesser General License
 * along with FFmpegKitNext. If not, see <http://www.gnu.org/licenses/>.
 */

#include "include/ffmpeg_kit_next_flutter/f_fmpeg_kit_flutter_plugin.h"

#include <flutter/event_channel.h>
#include <flutter/event_stream_handler_functions.h>
#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <windows.h>

#include <ArchDetect.h>
#include <FFmpegKit.h>
#include <FFmpegKitConfig.h>
#include <FFprobeKit.h>
#include <Log.h>
#include <MediaInformationJsonParser.h>
#include <MediaInformationSession.h>
#include <Packages.h>
#include <Session.h>
#include <Statistics.h>
#include <json/Value.h>

namespace ffmpeg_kit_windows {
using Value = flutter::EncodableValue;
using Map = flutter::EncodableMap;
using List = flutter::EncodableList;

std::string NativeString(const std::string &text) {
  if (text.find('\0') != std::string::npos)
    throw std::invalid_argument("Native strings cannot contain NUL.");
  return text;
}

std::string Utf8(const std::string &text) {
  if (text.empty())
    return text;
  if (text.size() > static_cast<size_t>((std::numeric_limits<int>::max)())) {
    throw std::invalid_argument("Text exceeds the Windows conversion limit.");
  }
  const int size = static_cast<int>(text.size());
  const int count =
      MultiByteToWideChar(CP_UTF8, 0, text.data(), size, nullptr, 0);
  std::wstring wide(count, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), size, wide.data(), count);
  const int length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), count,
                                         nullptr, 0, nullptr, nullptr);
  std::string result(length, '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.data(), count, result.data(), length,
                      nullptr, nullptr);
  return result;
}

Value JsonValue(const ffmpegkit::json::Value &source) {
  using Type = ffmpegkit::json::Value::Type;
  switch (source.getType()) {
  case Type::Null:
    return Value();
  case Type::Bool:
    return Value(*source.getBool());
  case Type::Int:
    return Value(static_cast<int64_t>(*source.getInt()));
  case Type::Double:
    return Value(*source.getDouble());
  case Type::String:
    return Value(Utf8(*source.getString()));
  case Type::Array: {
    List result;
    for (const auto &item : source.getArray())
      result.push_back(JsonValue(item));
    return Value(result);
  }
  case Type::Object: {
    Map result;
    for (const auto &item : source.getObject())
      result[Value(Utf8(item.first))] = JsonValue(item.second);
    return Value(result);
  }
  }
  return Value();
}

Value MediaValue(const std::shared_ptr<ffmpegkit::MediaInformation> &info) {
  if (!info)
    return Value();
  auto properties = info->getAllProperties();
  return properties ? JsonValue(*properties) : Value(Map{});
}

int64_t Milliseconds(std::chrono::system_clock::time_point value) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             value.time_since_epoch())
      .count();
}

Value SessionValue(const std::shared_ptr<ffmpegkit::Session> &session) {
  if (!session)
    return Value();
  Map result{
      {Value("sessionId"),
       Value(static_cast<int64_t>(session->getSessionId()))},
      {Value("createTime"), Value(Milliseconds(session->getCreateTime()))},
      {Value("startTime"), Value(Milliseconds(session->getStartTime()))},
      {Value("command"), Value(Utf8(session->getCommand()))},
      {Value("type"), Value(session->isFFmpeg()    ? 1
                            : session->isFFprobe() ? 2
                                                   : 3)}};
  if (session->isMediaInformation()) {
    result[Value("mediaInformation")] = MediaValue(
        std::static_pointer_cast<ffmpegkit::MediaInformationSession>(session)
            ->getMediaInformation());
  }
  return Value(result);
}

Value LogValue(const std::shared_ptr<ffmpegkit::Log> &log) {
  return Value(Map{
      {Value("sessionId"), Value(static_cast<int64_t>(log->getSessionId()))},
      {Value("level"), Value(static_cast<int>(log->getLevel()))},
      {Value("message"), Value(Utf8(log->getMessage()))}});
}

Value StatisticsValue(const std::shared_ptr<ffmpegkit::Statistics> &item) {
  return Value(Map{
      {Value("sessionId"), Value(static_cast<int64_t>(item->getSessionId()))},
      {Value("videoFrameNumber"), Value(item->getVideoFrameNumber())},
      {Value("videoFps"), Value(static_cast<double>(item->getVideoFps()))},
      {Value("videoQuality"),
       Value(static_cast<double>(item->getVideoQuality()))},
      {Value("size"), Value(static_cast<int64_t>(item->getSize()))},
      {Value("time"), Value(item->getTime())},
      {Value("bitrate"), Value(item->getBitrate())},
      {Value("speed"), Value(item->getSpeed())}});
}

template <typename T, typename Convert>
Value ListValue(const std::shared_ptr<std::list<std::shared_ptr<T>>> &items,
                Convert convert) {
  List result;
  if (items)
    for (const auto &item : *items)
      result.push_back(convert(item));
  return Value(result);
}

class Arguments {
public:
  explicit Arguments(const Value &value) {
    if (const auto *map = std::get_if<Map>(&value))
      values_ = *map;
    else if (!std::holds_alternative<std::monostate>(value))
      throw std::invalid_argument("Expected arguments map.");
  }

  const Value *Find(const char *key) const {
    const auto found = values_.find(Value(key));
    return found == values_.end() ||
                   std::holds_alternative<std::monostate>(found->second)
               ? nullptr
               : &found->second;
  }

  std::string String(const char *key, bool required = true) const {
    const auto *value = Find(key);
    if (!value && !required)
      return {};
    if (value)
      if (auto *text = std::get_if<std::string>(value))
        return NativeString(*text);
    throw std::invalid_argument(std::string("Expected string: ") + key);
  }

  int64_t Integer(const char *key, int64_t fallback = 0,
                  bool required = true) const {
    const auto *value = Find(key);
    if (!value && !required)
      return fallback;
    if (value) {
      if (auto *number = std::get_if<int32_t>(value))
        return *number;
      if (auto *number = std::get_if<int64_t>(value))
        return *number;
    }
    throw std::invalid_argument(std::string("Expected integer: ") + key);
  }

  int Int(const char *key, int fallback = 0, bool required = true) const {
    const auto number = Integer(key, fallback, required);
    if (number < (std::numeric_limits<int>::min)() ||
        number > (std::numeric_limits<int>::max)()) {
      throw std::invalid_argument(std::string("Integer out of native range: ") +
                                  key);
    }
    return static_cast<int>(number);
  }

  std::list<std::string> Strings(const char *key) const {
    const auto *value = Find(key);
    const auto *list = value ? std::get_if<List>(value) : nullptr;
    if (!list)
      throw std::invalid_argument(std::string("Expected list: ") + key);
    std::list<std::string> result;
    for (const auto &entry : *list) {
      const auto *text = std::get_if<std::string>(&entry);
      if (!text)
        throw std::invalid_argument("Expected string list entries.");
      result.push_back(NativeString(*text));
    }
    return result;
  }

  std::map<std::string, std::string> Mapping(const char *key) const {
    const auto *value = Find(key);
    if (!value)
      return {};
    const auto *map = std::get_if<Map>(value);
    if (!map)
      throw std::invalid_argument("Expected string map.");
    std::map<std::string, std::string> result;
    for (const auto &item : *map) {
      const auto *name = std::get_if<std::string>(&item.first);
      const auto *text = std::get_if<std::string>(&item.second);
      if (!name || !text)
        throw std::invalid_argument("Expected string map entries.");
      result[NativeString(*name)] = NativeString(*text);
    }
    return result;
  }

  std::vector<uint8_t> Bytes(const char *key) const {
    const auto *value = Find(key);
    if (value)
      if (auto *bytes = std::get_if<std::vector<uint8_t>>(value))
        return *bytes;
    throw std::invalid_argument("Expected byte array.");
  }

private:
  Map values_;
};

class PlatformDispatcher {
public:
  PlatformDispatcher() {
    WNDCLASSW definition{};
    definition.lpfnWndProc = WindowProc;
    definition.hInstance = GetModuleHandleW(nullptr);
    definition.lpszClassName = L"FFmpegKitNextFlutterDispatcher";
    if (!RegisterClassW(&definition) &&
        GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
      throw std::runtime_error("Cannot register FFmpegKit dispatcher.");
    }
    window_ =
        CreateWindowExW(0, definition.lpszClassName, L"", 0, 0, 0, 0, 0,
                        HWND_MESSAGE, nullptr, definition.hInstance, this);
    if (!window_)
      throw std::runtime_error("Cannot create FFmpegKit dispatcher.");
  }

  ~PlatformDispatcher() { Close(); }

  void Post(std::function<void()> task) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!window_)
      return;
    tasks_.push(std::move(task));
    PostMessageW(window_, WM_APP + 1, 0, 0);
  }

  void Close() {
    HWND window;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      window = window_;
      window_ = nullptr;
      std::queue<std::function<void()>> empty;
      tasks_.swap(empty);
    }
    if (window)
      DestroyWindow(window);
  }

private:
  static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam,
                                     LPARAM lparam) {
    if (message == WM_NCCREATE) {
      auto *create = reinterpret_cast<CREATESTRUCTW *>(lparam);
      SetWindowLongPtrW(window, GWLP_USERDATA,
                        reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    if (message == WM_APP + 1) {
      auto *self = reinterpret_cast<PlatformDispatcher *>(
          GetWindowLongPtrW(window, GWLP_USERDATA));
      if (self)
        self->Drain();
      return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
  }

  void Drain() {
    std::queue<std::function<void()>> tasks;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      tasks.swap(tasks_);
    }
    while (!tasks.empty()) {
      tasks.front()();
      tasks.pop();
    }
  }

  HWND window_ = nullptr;
  std::mutex mutex_;
  std::queue<std::function<void()>> tasks_;
};

using Config = ffmpegkit::FFmpegKitConfig;
using Result = flutter::MethodResult<Value>;

// Channel and event names must stay aligned with Dart and the other native
// implementations.
constexpr char kMethodChannelName[] = "flutter.arthenica.com/ffmpeg_kit";
constexpr char kEventChannelName[] = "flutter.arthenica.com/ffmpeg_kit_event";
constexpr char kCompleteEventName[] = "FFmpegKitCompleteCallbackEvent";
constexpr char kLogEventName[] = "FFmpegKitLogCallbackEvent";
constexpr char kStatisticsEventName[] = "FFmpegKitStatisticsCallbackEvent";
constexpr char kSessionDeletedEventName[] =
    "FFmpegKitSessionDeletedCallbackEvent";

class ChannelError : public std::runtime_error {
public:
  ChannelError(std::string code, std::string message)
      : std::runtime_error(std::move(message)), code(std::move(code)) {}
  const std::string code;
};

struct Resource {
  long id;
  bool stream;
  bool input;
};

struct PendingResponse {
  explicit PendingResponse(std::shared_ptr<Result> value)
      : result(std::move(value)) {}

  std::shared_ptr<Result> result;
  std::atomic<bool> completed{false};
};

struct NativeState : std::enable_shared_from_this<NativeState> {
  std::shared_ptr<PlatformDispatcher> dispatcher =
      std::make_shared<PlatformDispatcher>();
  std::unique_ptr<flutter::EventSink<Value>> sink;
  std::atomic<bool> alive{true};
  std::atomic<bool> logs{false};
  std::atomic<bool> statistics{false};
  std::mutex mutex;
  std::map<std::string, Resource> resources;
  std::set<long> sessions;
  std::set<std::string> pipes;
  std::vector<std::shared_ptr<PendingResponse>> pending_responses;
  std::shared_ptr<ffmpegkit::SessionDeleteListener> delete_listener;

  bool AddPendingResponse(const std::shared_ptr<PendingResponse> &response) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!alive)
      return false;
    pending_responses.push_back(response);
    return true;
  }

  bool ClaimPendingResponse(const std::shared_ptr<PendingResponse> &response) {
    if (response->completed.exchange(true))
      return false;
    std::lock_guard<std::mutex> lock(mutex);
    pending_responses.erase(std::remove(pending_responses.begin(),
                                        pending_responses.end(), response),
                            pending_responses.end());
    return true;
  }

  void CompletePendingResponsesForShutdown() {
    std::vector<std::shared_ptr<PendingResponse>> responses;
    {
      std::lock_guard<std::mutex> lock(mutex);
      responses.swap(pending_responses);
    }
    for (const auto &response : responses) {
      if (!response->completed.exchange(true)) {
        response->result->Error("PLUGIN_SHUTDOWN",
                                Utf8("The Windows plugin is shutting down."));
      }
    }
  }

  void Emit(const std::string &name, Value payload) {
    // Flutter channel sinks are thread-affine; native callbacks must return
    // through the platform dispatcher.
    std::weak_ptr<NativeState> weak = shared_from_this();
    dispatcher->Post([weak, name, payload = std::move(payload)] {
      if (auto state = weak.lock()) {
        if (state->alive && state->sink)
          state->sink->Success(Value(Map{{Value(name), payload}}));
      }
    });
  }

  Resource FindResource(const Arguments &args, bool stream, bool input) {
    std::lock_guard<std::mutex> lock(mutex);
    auto found = resources.find(args.String("url"));
    if (found == resources.end() || found->second.stream != stream ||
        found->second.input != input) {
      throw ChannelError("NOT_FOUND", "Buffer or stream not found.");
    }
    return found->second;
  }
};

class DeleteListener : public ffmpegkit::SessionDeleteListener {
public:
  explicit DeleteListener(std::weak_ptr<NativeState> state)
      : state_(std::move(state)) {}
  void sessionDeleted(long id) override {
    if (auto state = state_.lock()) {
      state->Emit(
          kSessionDeletedEventName,
          Value(Map{{Value("sessionId"), Value(static_cast<int64_t>(id))}}));
    }
  }

private:
  std::weak_ptr<NativeState> state_;
};

std::shared_ptr<ffmpegkit::Session> GetSession(const Arguments &args) {
  auto session = Config::getSession(args.Int("sessionId"));
  if (!session)
    throw ChannelError("SESSION_NOT_FOUND", "Session not found.");
  return session;
}

std::shared_ptr<ffmpegkit::FFmpegSession>
GetFFmpegSession(const Arguments &args) {
  auto session = GetSession(args);
  if (!session->isFFmpeg())
    throw ChannelError("NOT_FFMPEG_SESSION", "Expected FFmpeg session.");
  return std::static_pointer_cast<ffmpegkit::FFmpegSession>(session);
}

std::shared_ptr<ffmpegkit::FFprobeSession>
GetFFprobeSession(const Arguments &args) {
  auto session = GetSession(args);
  if (!session->isFFprobe())
    throw ChannelError("NOT_FFPROBE_SESSION", "Expected FFprobe session.");
  return std::static_pointer_cast<ffmpegkit::FFprobeSession>(session);
}

std::shared_ptr<ffmpegkit::MediaInformationSession>
GetMediaSession(const Arguments &args) {
  auto session = GetSession(args);
  if (!session->isMediaInformation())
    throw ChannelError("NOT_MEDIA_INFORMATION_SESSION",
                       "Expected media information session.");
  return std::static_pointer_cast<ffmpegkit::MediaInformationSession>(session);
}

Value BytesResult(uint8_t *bytes, size_t size, bool present) {
  std::unique_ptr<uint8_t, decltype(&ffk_bytes_free)> owned(bytes,
                                                            ffk_bytes_free);
  ffmpegkit::detail::checkError();
  if (!present)
    return Value();
  return Value(size ? std::vector<uint8_t>(bytes, bytes + size)
                    : std::vector<uint8_t>{});
}

Value Dispatch(const std::shared_ptr<NativeState> &state,
               const std::string &method, const Arguments &args) {
  const int wait = args.Int("waitTimeout", 5000, false);
  const int timeout = wait > 0 ? wait : 5000;
  if (method == "getArch")
    return Value(ffmpegkit::ArchDetect::getArch());
  if (method == "getPlatform")
    return Value("windows");
  if (method == "getFFmpegVersion")
    return Value(Config::getFFmpegVersion());
  if (method == "getBuildDate")
    return Value(Config::getBuildDate());
  if (method == "isLTSBuild")
    return Value(false);
  if (method == "printLoadConfirmation") {
    OutputDebugStringW(L"FFmpegKit Next Flutter Windows loaded.\n");
    return Value();
  }
  if (method == "getPackageName")
    return Value(ffmpegkit::Packages::getPackageName());
  if (method == "getExternalLibraries") {
    List libraries;
    auto source = ffmpegkit::Packages::getExternalLibraries();
    if (source)
      for (const auto &item : *source)
        libraries.emplace_back(item);
    return Value(libraries);
  }
  if (method == "ffmpegSession" || method == "ffprobeSession" ||
      method == "mediaInformationSession") {
    std::weak_ptr<NativeState> weak = state;
    auto complete = [weak](auto session) {
      if (auto current = weak.lock())
        if (current->alive) {
          try {
            current->Emit(kCompleteEventName, SessionValue(session));
          } catch (...) {
          }
        }
    };
    auto log = [weak](auto item) {
      if (auto current = weak.lock())
        if (current->alive && current->logs) {
          try {
            current->Emit(kLogEventName, LogValue(item));
          } catch (...) {
          }
        }
    };
    auto statistics = [weak](auto item) {
      if (auto current = weak.lock())
        if (current->alive && current->statistics) {
          try {
            current->Emit(kStatisticsEventName, StatisticsValue(item));
          } catch (...) {
          }
        }
    };
    const auto arguments = args.Strings("arguments");
    std::shared_ptr<ffmpegkit::Session> session;
    if (method == "ffmpegSession")
      session = ffmpegkit::FFmpegSession::create(arguments, complete, log,
                                                 statistics);
    else if (method == "ffprobeSession")
      session = ffmpegkit::FFprobeSession::create(arguments, complete, log);
    else
      session =
          ffmpegkit::MediaInformationSession::create(arguments, complete, log);
    state->sessions.insert(session->getSessionId());
    return SessionValue(session);
  }
  if (method == "abstractSessionGetEndTime") {
    const auto end = Milliseconds(GetSession(args)->getEndTime());
    return end ? Value(end) : Value();
  }
  if (method == "abstractSessionGetDuration")
    return Value(static_cast<int64_t>(GetSession(args)->getDuration()));
  if (method == "abstractSessionGetAllLogs")
    return ListValue(GetSession(args)->getAllLogsWithTimeout(timeout),
                     LogValue);
  if (method == "abstractSessionGetLogs")
    return ListValue(GetSession(args)->getLogs(), LogValue);
  if (method == "abstractSessionGetAllLogsAsString")
    return Value(
        Utf8(GetSession(args)->getAllLogsAsStringWithTimeout(timeout)));
  if (method == "abstractSessionGetState")
    return Value(static_cast<int>(GetSession(args)->getState()));
  if (method == "abstractSessionGetReturnCode") {
    const auto code = GetSession(args)->getReturnCode();
    return code ? Value(code->getValue()) : Value();
  }
  if (method == "abstractSessionGetFailStackTrace")
    return Value(Utf8(GetSession(args)->getFailStackTrace()));
  if (method == "thereAreAsynchronousMessagesInTransmit" ||
      method == "abstractSessionThereAreAsynchronousMessagesInTransmit")
    return Value(GetSession(args)->thereAreAsynchronousMessagesInTransmit());
  if (method == "ffmpegSessionGetAllStatistics")
    return ListValue(
        GetFFmpegSession(args)->getAllStatisticsWithTimeout(timeout),
        StatisticsValue);
  if (method == "ffmpegSessionGetStatistics")
    return ListValue(GetFFmpegSession(args)->getStatistics(), StatisticsValue);
  if (method == "getMediaInformation")
    return MediaValue(GetMediaSession(args)->getMediaInformation());
  if (method == "mediaInformationJsonParserFrom")
    return MediaValue(ffmpegkit::MediaInformationJsonParser::from(
        args.String("ffprobeJsonOutput")));
  if (method == "mediaInformationJsonParserFromWithError")
    return MediaValue(ffmpegkit::MediaInformationJsonParser::fromWithError(
        args.String("ffprobeJsonOutput")));
  if (method == "ffmpegSessionExecute") {
    Config::ffmpegExecute(GetFFmpegSession(args));
    return Value();
  }
  if (method == "ffprobeSessionExecute") {
    Config::ffprobeExecute(GetFFprobeSession(args));
    return Value();
  }
  if (method == "mediaInformationSessionExecute") {
    Config::getMediaInformationExecute(GetMediaSession(args), timeout);
    return Value();
  }
  if (method == "asyncFFmpegSessionExecute") {
    Config::asyncFFmpegExecute(GetFFmpegSession(args));
    return Value();
  }
  if (method == "asyncFFprobeSessionExecute") {
    Config::asyncFFprobeExecute(GetFFprobeSession(args));
    return Value();
  }
  if (method == "asyncMediaInformationSessionExecute") {
    Config::asyncGetMediaInformationExecute(GetMediaSession(args), timeout);
    return Value();
  }
  if (method == "enableRedirection") {
    state->logs = true;
    state->statistics = true;
    Config::enableRedirection();
    return Value();
  }
  if (method == "disableRedirection") {
    Config::disableRedirection();
    return Value();
  }
  if (method == "enableLogs") {
    state->logs = true;
    return Value();
  }
  if (method == "disableLogs") {
    state->logs = false;
    return Value();
  }
  if (method == "enableStatistics") {
    state->statistics = true;
    return Value();
  }
  if (method == "disableStatistics") {
    state->statistics = false;
    return Value();
  }
  if (method == "setFontconfigConfigurationPath") {
    Config::setFontconfigConfigurationPath(args.String("path"));
    return Value();
  }
  if (method == "setFontDirectory") {
    Config::setFontDirectory(args.String("fontDirectory"),
                             args.Mapping("fontNameMap"));
    return Value();
  }
  if (method == "setFontDirectoryList") {
    Config::setFontDirectoryList(args.Strings("fontDirectoryList"),
                                 args.Mapping("fontNameMap"));
    return Value();
  }
  if (method == "setEnvironmentVariable") {
    Config::setEnvironmentVariable(args.String("variableName"),
                                   args.String("variableValue"));
    return Value();
  }
  if (method == "ignoreSignal") {
    const int index = args.Int("signal");
    const ffmpegkit::Signal signals[] = {
        ffmpegkit::SignalInt, ffmpegkit::SignalQuit, ffmpegkit::SignalPipe,
        ffmpegkit::SignalTerm, ffmpegkit::SignalXcpu};
    if (index < 0 || index > 4)
      throw ChannelError("INVALID_SIGNAL", "Signal value not supported.");
    Config::ignoreSignal(signals[index]);
    return Value();
  }
  if (method == "getLogLevel")
    return Value(static_cast<int>(Config::getLogLevel()));
  if (method == "setLogLevel") {
    Config::setLogLevel(static_cast<ffmpegkit::Level>(args.Int("level")));
    return Value();
  }
  if (method == "getSessionHistorySize")
    return Value(Config::getSessionHistorySize());
  if (method == "setSessionHistorySize") {
    Config::setSessionHistorySize(args.Int("sessionHistorySize"));
    return Value();
  }
  if (method == "getLogRedirectionStrategy")
    return Value(static_cast<int>(Config::getLogRedirectionStrategy()));
  if (method == "setLogRedirectionStrategy") {
    const int strategy = args.Int("strategy");
    if (strategy < 0 || strategy > 4)
      throw ChannelError("INVALID_LOG_REDIRECTION_STRATEGY",
                         "Invalid strategy.");
    Config::setLogRedirectionStrategy(
        static_cast<ffmpegkit::LogRedirectionStrategy>(strategy));
    return Value();
  }
  if (method == "messagesInTransmit")
    return Value(Config::messagesInTransmit(args.Int("sessionId")));
  if (method == "getSession")
    return SessionValue(Config::getSession(args.Int("sessionId")));
  if (method == "getLastSession")
    return SessionValue(Config::getLastSession());
  if (method == "getLastCompletedSession")
    return SessionValue(Config::getLastCompletedSession());
  if (method == "getSessions")
    return ListValue(Config::getSessions(), SessionValue);
  if (method == "getFFmpegSessions")
    return ListValue(Config::getFFmpegSessions(), SessionValue);
  if (method == "getFFprobeSessions")
    return ListValue(Config::getFFprobeSessions(), SessionValue);
  if (method == "getMediaInformationSessions")
    return ListValue(Config::getMediaInformationSessions(), SessionValue);
  if (method == "getSessionsByState") {
    const int filter = args.Int("state");
    if (filter < 0 || filter > 3)
      throw ChannelError("INVALID_SESSION_STATE", "Invalid session state.");
    return ListValue(Config::getSessionsByState(
                         static_cast<ffmpegkit::SessionState>(filter)),
                     SessionValue);
  }
  if (method == "clearSessions") {
    Config::clearSessions();
    return Value();
  }
  if (method == "deleteSession") {
    Config::deleteSession(args.Int("sessionId"));
    return Value();
  }
  if (method == "cancel") {
    ffmpegkit::FFmpegKit::cancel();
    return Value();
  }
  if (method == "cancelSession") {
    ffmpegkit::FFmpegKit::cancel(args.Int("sessionId"));
    return Value();
  }
  if (method == "selectDocument" || method == "getSafParameter" ||
      method == "unregisterSafProtocolUrl" ||
      method == "getSupportedCameraIds") {
    throw ChannelError("NOT_SUPPORTED", "Not supported on Windows platform.");
  }
  if (method == "registerNewFFmpegPipe") {
    char *path = ffk_config_register_new_ffmpeg_pipe();
    ffmpegkit::detail::checkError();
    const auto text = ffmpegkit::detail::takeString(path);
    if (text.empty())
      throw ChannelError("CREATE_FAILED", "Failed to create pipe.");
    state->pipes.insert(text);
    return Value(text);
  }
  if (method == "closeFFmpegPipe") {
    const auto path = args.String("ffmpegPipePath");
    ffk_config_close_ffmpeg_pipe(path.c_str());
    ffmpegkit::detail::checkError();
    state->pipes.erase(path);
    return Value();
  }
  if (method == "writeToPipe") {
    const auto input = args.String("input");
    const auto pipe = args.String("pipe");
    const int result = ffk_config_write_to_pipe(input.c_str(), pipe.c_str());
    ffmpegkit::detail::checkError();
    return Value(result);
  }
  if (method == "writeBytesToPipe") {
    const auto bytes = args.Bytes("data");
    const auto pipe = args.String("pipe");
    const int result = ffk_config_write_bytes_to_pipe(
        bytes.empty() ? nullptr : bytes.data(), bytes.size(), pipe.c_str());
    ffmpegkit::detail::checkError();
    return Value(result);
  }
  if (method == "inputBufferFromByteArray" || method == "outputBufferCreate" ||
      method == "streamInputCreate" || method == "streamOutputCreate") {
    const bool stream =
        method == "streamInputCreate" || method == "streamOutputCreate";
    const bool input =
        method == "streamInputCreate" || method == "inputBufferFromByteArray";
    const auto extension = args.String("extension", false);
    long id;
    if (stream) {
      const int capacity = args.Int("capacity", 1048576, false);
      if (capacity <= 0)
        throw std::invalid_argument("Stream capacity must be positive.");
      id = ffk_config_register_ffmpegkit_stream(capacity, input ? 1 : 2);
    } else if (input) {
      const auto bytes = args.Bytes("data");
      id = ffk_config_register_ffmpegkit_input_buffer(bytes.data(),
                                                      bytes.size());
    } else
      id = ffk_config_register_ffmpegkit_output_buffer(
          args.Int("initialCapacity", 4096, false),
          args.Int("maxCapacity", 0, false));
    ffmpegkit::detail::checkError();
    if (!id)
      throw ChannelError("CREATE_FAILED", "Failed to create buffer or stream.");
    try {
      const auto url = ffmpegkit::detail::takeString(ffk_protocol_build_url(
          stream ? "ffkitstream" : "ffkitmem", id, extension.c_str()));
      ffmpegkit::detail::checkError();
      std::lock_guard<std::mutex> lock(state->mutex);
      state->resources[url] = Resource{id, stream, input};
      return Value(url);
    } catch (...) {
      if (stream)
        ffk_config_unregister_ffmpegkit_stream(id);
      else
        ffk_config_unregister_ffmpegkit_buffer(id);
      throw;
    }
  }
  if (method == "inputBufferClose" || method == "outputBufferClose" ||
      method == "streamInputClose" || method == "streamOutputClose") {
    std::lock_guard<std::mutex> lock(state->mutex);
    const auto found = state->resources.find(args.String("url"));
    if (found != state->resources.end()) {
      if (found->second.stream)
        ffk_config_unregister_ffmpegkit_stream(found->second.id);
      else
        ffk_config_unregister_ffmpegkit_buffer(found->second.id);
      ffmpegkit::detail::checkError();
      state->resources.erase(found);
    }
    return Value();
  }
  if (method == "outputBufferGetSize") {
    const auto resource = state->FindResource(args, false, false);
    const auto size = ffk_config_get_ffmpegkit_buffer_size(resource.id);
    ffmpegkit::detail::checkError();
    return Value(static_cast<int64_t>(size));
  }
  if (method == "outputBufferToByteArray") {
    const auto resource = state->FindResource(args, false, false);
    uint8_t *bytes = nullptr;
    size_t size = 0;
    const int present =
        ffk_config_get_ffmpegkit_output_buffer(resource.id, &bytes, &size);
    return BytesResult(bytes, size, present != 0);
  }
  if (method == "streamInputWrite") {
    const auto resource = state->FindResource(args, true, true);
    const auto bytes = args.Bytes("data");
    const int written = ffk_config_write_ffmpegkit_stream(
        resource.id, bytes.data(), bytes.size(),
        args.Int("timeoutMs", -1, false));
    ffmpegkit::detail::checkError();
    if (written < 0)
      throw ChannelError("WRITE_FAILED", "Stream write failed.");
    return Value(written);
  }
  if (method == "streamInputCloseInput") {
    ffk_config_close_ffmpegkit_stream_input(
        state->FindResource(args, true, true).id);
    ffmpegkit::detail::checkError();
    return Value();
  }
  if (method == "streamOutputRead") {
    const auto resource = state->FindResource(args, true, false);
    uint8_t *bytes = nullptr;
    size_t size = 0;
    const int present = ffk_config_read_ffmpegkit_stream(
        resource.id, args.Int("maxBytes"), args.Int("timeoutMs", -1, false),
        &bytes, &size);
    return BytesResult(bytes, size, present != 0);
  }
  throw ChannelError("METHOD_NOT_IMPLEMENTED", method);
}

class WindowsPlugin : public flutter::Plugin {
public:
  explicit WindowsPlugin(flutter::PluginRegistrarWindows *registrar)
      : state_(std::make_shared<NativeState>()) {
    auto *codec = &flutter::StandardMethodCodec::GetInstance();
    methods_ = std::make_unique<flutter::MethodChannel<Value>>(
        registrar->messenger(), kMethodChannelName, codec);
    events_ = std::make_unique<flutter::EventChannel<Value>>(
        registrar->messenger(), kEventChannelName, codec);
    events_->SetStreamHandler(
        std::make_unique<flutter::StreamHandlerFunctions<Value>>(
            [state = state_](const Value *,
                             std::unique_ptr<flutter::EventSink<Value>> &&sink)
                -> std::unique_ptr<flutter::StreamHandlerError<Value>> {
              state->sink = std::move(sink);
              return nullptr;
            },
            [state = state_](const Value *)
                -> std::unique_ptr<flutter::StreamHandlerError<Value>> {
              state->sink.reset();
              return nullptr;
            }));
    state_->delete_listener = std::make_shared<DeleteListener>(state_);
    Config::addSessionDeleteListener(state_->delete_listener);
    methods_->SetMethodCallHandler(
        [state = state_](const flutter::MethodCall<Value> &call,
                         std::unique_ptr<Result> response) {
          auto result = std::shared_ptr<Result>(std::move(response));
          auto pending = std::make_shared<PendingResponse>(result);
          if (!state->AddPendingResponse(pending)) {
            result->Error("PLUGIN_SHUTDOWN",
                          Utf8("The Windows plugin is shutting down."));
            return;
          }
          const auto method = call.method_name();
          const auto arguments = call.arguments() ? *call.arguments() : Value();
          auto run = [state, method, arguments, pending](bool post_response) {
            Value value;
            std::string code, message;
            try {
              value = Dispatch(state, method, Arguments(arguments));
            } catch (const ChannelError &error) {
              code = error.code;
              message = error.what();
            } catch (const std::invalid_argument &error) {
              code = "INVALID_ARGUMENTS";
              message = error.what();
            } catch (const std::exception &error) {
              code = "NATIVE_ERROR";
              message = error.what();
            } catch (...) {
              code = "NATIVE_ERROR";
              message = "Unknown native error.";
            }
            auto respond = [state, pending, value = std::move(value), code,
                            message] {
              if (!state->ClaimPendingResponse(pending))
                return;
              if (code == "METHOD_NOT_IMPLEMENTED")
                pending->result->NotImplemented();
              else if (!code.empty())
                pending->result->Error(code, Utf8(message));
              else
                pending->result->Success(value);
            };
            if (post_response)
              state->dispatcher->Post(std::move(respond));
            else
              respond();
          };
          // These calls can block on FFmpeg or stream I/O and must not run on
          // the Flutter platform thread.
          static const std::set<std::string> background{
              "ffmpegSessionExecute",
              "ffprobeSessionExecute",
              "mediaInformationSessionExecute",
              "abstractSessionGetAllLogs",
              "abstractSessionGetAllLogsAsString",
              "ffmpegSessionGetAllStatistics",
              "streamInputWrite",
              "streamOutputRead",
              "writeToPipe",
              "writeBytesToPipe"};
          if (background.count(method)) {
            try {
              std::thread([run = std::move(run)] { run(true); }).detach();
            } catch (const std::exception &error) {
              if (state->ClaimPendingResponse(pending))
                pending->result->Error("NATIVE_ERROR", Utf8(error.what()));
            }
          } else
            run(false);
        });
  }

  ~WindowsPlugin() override {
    // Mark state dead before releasing channel-owned objects so detached
    // workers cannot access them during shutdown.
    state_->alive = false;
    state_->CompletePendingResponsesForShutdown();
    state_->sink.reset();
    Config::removeSessionDeleteListener(state_->delete_listener);
    for (const auto id : state_->sessions)
      ffmpegkit::FFmpegKit::cancel(id);
    for (const auto &path : state_->pipes)
      ffk_config_close_ffmpeg_pipe(path.c_str());
    {
      std::lock_guard<std::mutex> lock(state_->mutex);
      for (const auto &entry : state_->resources) {
        if (entry.second.stream)
          ffk_config_unregister_ffmpegkit_stream(entry.second.id);
        else
          ffk_config_unregister_ffmpegkit_buffer(entry.second.id);
      }
      state_->resources.clear();
    }
    state_->dispatcher->Close();
  }

private:
  std::shared_ptr<NativeState> state_;
  std::unique_ptr<flutter::MethodChannel<Value>> methods_;
  std::unique_ptr<flutter::EventChannel<Value>> events_;
};
} // namespace ffmpeg_kit_windows

void FFmpegKitFlutterPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  auto *wrapper =
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar);
  wrapper->AddPlugin(
      std::make_unique<ffmpeg_kit_windows::WindowsPlugin>(wrapper));
}
