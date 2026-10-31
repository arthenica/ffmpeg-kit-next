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
#include "plugin_state.h"

#include <flutter/event_channel.h>
#include <flutter/event_stream_handler_functions.h>
#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include <windows.h>

#include <AbstractSession.h>
#include <ArchDetect.h>
#include <FFmpegKit.h>
#include <FFmpegKitConfig.h>
#include <FFmpegKitInputBuffer.h>
#include <FFmpegKitOutputBuffer.h>
#include <FFmpegKitStreamInput.h>
#include <FFmpegKitStreamOutput.h>
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

// Flutter's Windows build settings define _HAS_EXCEPTIONS=0. MSVC standard
// exceptions then keep only a pointer to their message, so plugin exceptions
// own the text they report.
class ChannelError : public std::exception {
  public:
    ChannelError(std::string code, std::string message) : code(std::move(code)), message_(std::move(message)) {}
    const char *what() const noexcept override { return message_.c_str(); }
    const std::string code;

  private:
    std::string message_;
};

class ArgumentError : public std::exception {
  public:
    ArgumentError(std::string code, std::string message) : code(std::move(code)), message_(std::move(message)) {}
    explicit ArgumentError(std::string message) : ArgumentError("INVALID_ARGUMENTS", std::move(message)) {}
    const char *what() const noexcept override { return message_.c_str(); }
    const std::string code;

  private:
    std::string message_;
};

// The code and message a method reports for a bad argument. The Android and
// iOS plugins return them when the argument is missing; Windows also returns
// them for a wrongly typed or unrepresentable value. An empty check falls back
// to INVALID_ARGUMENTS, for arguments mobile does not validate.
struct Check {
    const char *code = nullptr;
    const char *message = nullptr;
};

[[noreturn]] void Reject(const Check &check, std::string message) {
    if (check.code)
        throw ArgumentError(check.code, check.message);
    throw ArgumentError(std::move(message));
}

std::string NativeString(const std::string &text, const Check &check = {}) {
    if (text.find('\0') != std::string::npos)
        Reject(check, "Native strings cannot contain NUL.");
    return text;
}

std::string Utf8(const std::string &text) {
  if (text.empty())
    return text;
  if (text.size() > static_cast<size_t>((std::numeric_limits<int>::max)())) {
      throw ArgumentError("Text exceeds the Windows conversion limit.");
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
  List arguments;
  auto native_arguments = session->getArguments();
  if (native_arguments) {
    for (const auto &argument : *native_arguments)
      arguments.emplace_back(Utf8(argument));
  }
  result[Value("arguments")] = Value(arguments);
  if (session->isMediaInformation()) {
      auto media_information = std::static_pointer_cast<ffmpegkit::MediaInformationSession>(session)->getMediaInformation();
      if (media_information)
          result[Value("mediaInformation")] = MediaValue(media_information);
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

// A non-owning view over the method call arguments. Byte payloads can be large,
// so the map is never copied; the viewed value must outlive this object and
// every reference it returns.
class Arguments {
public:
  explicit Arguments(const Value &value) {
    if (const auto *map = std::get_if<Map>(&value))
        values_ = map;
    else if (!std::holds_alternative<std::monostate>(value))
        throw ArgumentError("Expected arguments map.");
  }

  const Value *Find(const char *key) const {
      if (!values_)
          return nullptr;
      const auto found = values_->find(Value(key));
      return found == values_->end() || std::holds_alternative<std::monostate>(found->second) ? nullptr : &found->second;
  }

  // Required accessors reject a missing, null, wrongly typed or unrepresentable
  // value with check.
  std::string String(const char *key, const Check &check = {}) const {
      const auto *value = Find(key);
      if (const auto *text = value ? std::get_if<std::string>(value) : nullptr)
          return NativeString(*text, check);
      Reject(check, std::string("Expected string: ") + key);
  }

  std::string OptionalString(const char *key) const { return Find(key) ? String(key) : std::string(); }

  int Int(const char *key, const Check &check = {}) const {
      const auto *value = Find(key);
      int64_t number = 0;
      if (const auto *int32_value = value ? std::get_if<int32_t>(value) : nullptr)
          number = *int32_value;
      else if (const auto *int64_value = value ? std::get_if<int64_t>(value) : nullptr)
          number = *int64_value;
      else
          Reject(check, std::string("Expected integer: ") + key);
      if (number < (std::numeric_limits<int>::min)() || number > (std::numeric_limits<int>::max)()) {
          Reject(check, std::string("Integer out of native range: ") + key);
      }
      return static_cast<int>(number);
  }

  int OptionalInt(const char *key, int fallback) const { return Find(key) ? Int(key) : fallback; }

  std::list<std::string> Strings(const char *key, const Check &check = {}) const {
      const auto *value = Find(key);
      const auto *list = value ? std::get_if<List>(value) : nullptr;
      if (!list)
          Reject(check, std::string("Expected list: ") + key);
      std::list<std::string> result;
      for (const auto &entry : *list) {
          const auto *text = std::get_if<std::string>(&entry);
          if (!text)
              Reject(check, "Expected string list entries.");
          result.push_back(NativeString(*text, check));
      }
      return result;
  }

  std::map<std::string, std::string> Mapping(const char *key) const {
    const auto *value = Find(key);
    if (!value)
      return {};
    const auto *map = std::get_if<Map>(value);
    if (!map)
        throw ArgumentError("Expected string map.");
    std::map<std::string, std::string> result;
    for (const auto &item : *map) {
      const auto *name = std::get_if<std::string>(&item.first);
      const auto *text = std::get_if<std::string>(&item.second);
      if (!name || !text)
          throw ArgumentError("Expected string map entries.");
      result[NativeString(*name)] = NativeString(*text);
    }
    return result;
  }

  const std::vector<uint8_t> &Bytes(const char *key, const Check &check = {}) const {
      const auto *value = Find(key);
      if (const auto *bytes = value ? std::get_if<std::vector<uint8_t>>(value) : nullptr)
          return *bytes;
      Reject(check, "Expected byte array.");
  }

private:
  const Map *values_ = nullptr;
};

// Argument checks shared with the Android and iOS plugins. Keep the codes and
// messages aligned with them.
constexpr Check kInvalidSession{"INVALID_SESSION", "Invalid session id."};
constexpr Check kInvalidArgumentsArray{"INVALID_ARGUMENTS", "Invalid arguments array."};
constexpr Check kInvalidShutdownTimeout{"INVALID_ARGUMENTS", "waitTimeout is required."};
constexpr Check kInvalidFfprobeJsonOutput{"INVALID_FFPROBE_JSON_OUTPUT", "Invalid ffprobe json output."};
constexpr Check kInvalidPath{"INVALID_PATH", "Invalid path."};
constexpr Check kInvalidFontDirectory{"INVALID_FONT_DIRECTORY", "Invalid font directory."};
constexpr Check kInvalidFontDirectoryList{"INVALID_FONT_DIRECTORY_LIST", "Invalid font directory list."};
constexpr Check kInvalidPipePath{"INVALID_PIPE_PATH", "Invalid ffmpeg pipe path."};
constexpr Check kInvalidVariableName{"INVALID_NAME", "Invalid environment variable name."};
constexpr Check kInvalidVariableValue{"INVALID_VALUE", "Invalid environment variable value."};
constexpr Check kInvalidSignal{"INVALID_SIGNAL", "Invalid signal value."};
constexpr Check kInvalidLevel{"INVALID_LEVEL", "Invalid level value."};
constexpr Check kInvalidSize{"INVALID_SIZE", "Invalid session history size value."};
constexpr Check kInvalidSessionState{"INVALID_SESSION_STATE", "Invalid session state value."};
constexpr Check kInvalidLogRedirectionStrategy{"INVALID_LOG_REDIRECTION_STRATEGY", "Invalid log redirection strategy value."};
constexpr Check kInvalidData{"INVALID_DATA", "Invalid input buffer data."};
constexpr Check kInvalidInputBufferUrl{"INVALID_URL", "Invalid input buffer url."};
constexpr Check kInvalidOutputBufferUrl{"INVALID_URL", "Invalid output buffer url."};
constexpr Check kInvalidStreamInputUrl{"INVALID_URL", "Invalid stream input url."};
constexpr Check kInvalidStreamOutputUrl{"INVALID_URL", "Invalid stream output url."};
constexpr Check kInvalidStreamInputArguments{"INVALID_ARGUMENTS", "Invalid stream input arguments."};
constexpr Check kInvalidStreamOutputArguments{"INVALID_ARGUMENTS", "Invalid stream output arguments."};

class PlatformDispatcher {
public:
  PlatformDispatcher() {
    WNDCLASSW definition{};
    definition.lpfnWndProc = WindowProc;
    definition.hInstance = GetModuleHandleW(nullptr);
    definition.lpszClassName = L"FFmpegKitNextFlutterDispatcher";
    if (!RegisterClassW(&definition) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        throw std::runtime_error("Cannot register FFmpegKit dispatcher.");
    }
    window_ = CreateWindowExW(0, definition.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, definition.hInstance, this);
    if (!window_)
      throw std::runtime_error("Cannot create FFmpegKit dispatcher.");
  }

  ~PlatformDispatcher() { Close(); }

  // Mirrors the Flutter engine's TaskRunnerWindow: at most one wakeup message
  // is in the queue, and it drains everything posted before it is handled.
  // That keeps a burst of logs from flooding the thread's posted-message queue.
  void Post(std::function<void()> task) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!window_)
      return;
    tasks_.push(std::move(task));
    if (wake_pending_)
        return;
    wake_pending_ = PostMessageW(window_, kWakeMessage, 0, 0) != 0;
    // Only a full posted-message queue (10,000 messages by default) rejects
    // the wakeup. Unlike the engine, leave the flag clear so the next Post
    // retries; the queued work stays in order until then.
    if (!wake_pending_ && !post_failed_)
        std::fprintf(stderr, "ffmpeg-kit-next-flutter: failed to post a message to the platform thread.\n");
    post_failed_ = !wake_pending_;
  }

  void Close() {
    HWND window;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      window = window_;
      window_ = nullptr;
      wake_pending_ = false;
      std::queue<std::function<void()>> empty;
      tasks_.swap(empty);
    }
    if (window)
        DestroyWindow(window);
  }

private:
  static constexpr UINT kWakeMessage = WM_APP + 1;

  static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
      if (message == WM_NCCREATE) {
          auto *create = reinterpret_cast<CREATESTRUCTW *>(lparam);
          SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
      }
      if (message == kWakeMessage) {
          auto *self = reinterpret_cast<PlatformDispatcher *>(GetWindowLongPtrW(window, GWLP_USERDATA));
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
          // From here on, Post must send a new wakeup for later work.
          wake_pending_ = false;
          tasks.swap(tasks_);
      }
      while (!tasks.empty()) {
          // An exception must not unwind through the window procedure, and
          // one failing task must not drop the replies queued after it.
          try {
              tasks.front()();
          } catch (...) {
          }
          tasks.pop();
      }
  }

  bool wake_pending_ = false;
  bool post_failed_ = false;
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

// Error codes and messages must stay aligned with the Linux implementation.
constexpr char kPluginShutdownMessage[] = "The Flutter plugin is shutting down.";
constexpr char kWrongSessionTypeMessage[] = "A session is found but it does not have the correct type.";

using Resource =
    std::variant<std::shared_ptr<ffmpegkit::FFmpegKitInputBuffer>,
                 std::shared_ptr<ffmpegkit::FFmpegKitOutputBuffer>,
                 std::shared_ptr<ffmpegkit::FFmpegKitStreamInput>,
                 std::shared_ptr<ffmpegkit::FFmpegKitStreamOutput>>;

// Everything detached workers and native callbacks use. The plugin object dies
// when the engine stops, so they hold this shared state instead, as on Linux.
// Lifecycle (admission, executions, replies, async sessions) is PluginState.
struct PluginData : std::enable_shared_from_this<PluginData> {
    std::shared_ptr<PlatformDispatcher> dispatcher = std::make_shared<PlatformDispatcher>();
    std::unique_ptr<flutter::EventSink<Value>> sink;
    std::atomic<bool> logs{false};
    std::atomic<bool> statistics{false};
    PluginState executions;
    std::mutex mutex;
    std::map<std::string, Resource> resources;
    bool retain_shutdown_sessions = false;
    std::map<long, std::shared_ptr<ffmpegkit::Session>> shutdown_sessions;
    std::shared_ptr<ffmpegkit::SessionDeleteListener> delete_listener;

    // Teardown cancels admitted work through cancel and waits for its lease.
    std::unique_ptr<PluginState::Lease> StartTracked(std::function<void()> cancel) {
        auto lease = executions.TryStart(std::move(cancel));
        if (!lease)
            throw ChannelError("PLUGIN_SHUTDOWN", kPluginShutdownMessage);
        return lease;
    }

    std::unique_ptr<PluginState::Lease> StartExecution(const std::shared_ptr<ffmpegkit::Session> &session) {
        return StartTracked([session] { session->cancel(); });
    }

    // Like Linux, stream I/O is cancelled through the native registry, which
    // wakes a blocked read or write without touching the stream handle.
    std::unique_ptr<PluginState::Lease> StartStreamIo(long stream_id) {
        return StartTracked([stream_id] { Config::unregisterFFmpegKitStream(stream_id); });
    }

    template <typename T, typename Submit> void StartAsyncExecution(const std::shared_ptr<T> &session, Submit submit) {
        if (!executions.TrackAsync(session))
            throw ChannelError("PLUGIN_SHUTDOWN", kPluginShutdownMessage);
        const auto id = session->getSessionId();
        try {
            submit();
            // Shutdown rejects submissions without invoking a completion callback.
            if (Config::isShutdown() && session->getState() == ffmpegkit::SessionStateFailed)
                executions.FinishAsync(id);
        } catch (const std::exception &error) {
            executions.FinishAsync(id);
            throw ChannelError("EXECUTION_FAILED", error.what());
        } catch (...) {
            executions.FinishAsync(id);
            throw;
        }
    }

    void Emit(const std::string &name, Value payload) {
        // Flutter channel sinks are thread-affine; native callbacks must return
        // through the platform dispatcher.
        std::weak_ptr<PluginData> weak = shared_from_this();
        dispatcher->Post([weak, name, payload = std::move(payload)] {
            if (auto state = weak.lock()) {
                if (!state->executions.IsStopping() && state->sink)
                    state->sink->Success(Value(Map{{Value(name), payload}}));
            }
        });
    }

    template <typename T> std::shared_ptr<T> FindResource(const std::string &url, const char *missing) {
        std::lock_guard<std::mutex> lock(mutex);
        auto found = resources.find(url);
        if (found == resources.end()) {
            throw ChannelError("NOT_FOUND", missing);
        }
        const auto *resource = std::get_if<std::shared_ptr<T>>(&found->second);
        if (!resource) {
            throw ChannelError("NOT_FOUND", missing);
        }
        return *resource;
    }
};

class DeleteListener : public ffmpegkit::SessionDeleteListener {
public:
  explicit DeleteListener(std::weak_ptr<PluginData> state) : state_(std::move(state)) {}
  void sessionDeleted(long id) override {
    if (auto state = state_.lock()) {
      state->Emit(
          kSessionDeletedEventName,
          Value(Map{{Value("sessionId"), Value(static_cast<int64_t>(id))}}));
    }
  }

private:
  std::weak_ptr<PluginData> state_;
};

std::shared_ptr<ffmpegkit::Session> GetSession(const std::shared_ptr<PluginData> &state, const Arguments &args) {
    const long id = args.Int("sessionId", kInvalidSession);
    auto session = Config::getSession(id);
    if (!session) {
        std::lock_guard<std::mutex> lock(state->mutex);
        auto found = state->shutdown_sessions.find(id);
        if (found != state->shutdown_sessions.end())
            session = found->second;
    }
    if (!session)
        throw ChannelError("SESSION_NOT_FOUND", "Session not found.");
    return session;
}

std::shared_ptr<ffmpegkit::FFmpegSession> GetFFmpegSession(const std::shared_ptr<PluginData> &state, const Arguments &args) {
    auto session = GetSession(state, args);
    if (!session->isFFmpeg())
        throw ChannelError("NOT_FFMPEG_SESSION", kWrongSessionTypeMessage);
    return std::static_pointer_cast<ffmpegkit::FFmpegSession>(session);
}

std::shared_ptr<ffmpegkit::FFprobeSession> GetFFprobeSession(const std::shared_ptr<PluginData> &state, const Arguments &args) {
    auto session = GetSession(state, args);
    if (!session->isFFprobe())
        throw ChannelError("NOT_FFPROBE_SESSION", kWrongSessionTypeMessage);
    return std::static_pointer_cast<ffmpegkit::FFprobeSession>(session);
}

std::shared_ptr<ffmpegkit::MediaInformationSession> GetMediaSession(const std::shared_ptr<PluginData> &state, const Arguments &args) {
    auto session = GetSession(state, args);
    if (!session->isMediaInformation())
        throw ChannelError("NOT_MEDIA_INFORMATION_SESSION", kWrongSessionTypeMessage);
    return std::static_pointer_cast<ffmpegkit::MediaInformationSession>(session);
}

// Stream urls are "ffkitstream:<id>.<extension>".
long StreamId(const std::string &url) {
    const auto colon = url.find(':');
    return colon == std::string::npos ? 0 : std::strtol(url.c_str() + colon + 1, nullptr, 10);
}

Value BytesResult(const std::shared_ptr<std::vector<uint8_t>> &bytes) {
  if (!bytes)
    return Value();
  return Value(*bytes);
}

Value Dispatch(const std::shared_ptr<PluginData> &state, const std::string &method, const Arguments &args) {
    const int default_timeout = ffmpegkit::AbstractSession::DefaultTimeoutForAsynchronousMessagesInTransmit;
    const int wait = args.OptionalInt("waitTimeout", default_timeout);
    const int timeout = wait > 0 ? wait : default_timeout;
    if (method == "isShutdown")
        return Value(Config::isShutdown());
    if (method == "getArch")
        return Value(ffmpegkit::ArchDetect::getArch());
    if (method == "getPlatform")
        return Value("windows");
    if (method == "getFFmpegVersion")
        return Value(Config::getFFmpegVersion());
    if (method == "getBuildDate")
        return Value(Config::getBuildDate());
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
    if (method == "isLTSBuild")
        return Value(Config::isLTSBuild());
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
    if (method == "printLoadConfirmation") {
        static std::once_flag load_confirmation;
        const auto *version_value = args.Find("version");
        const auto *version_text = version_value ? std::get_if<std::string>(version_value) : nullptr;
        const std::string version = version_text ? *version_text : std::string();
        std::call_once(load_confirmation, [&version] {
            const auto package = ffmpegkit::Packages::getPackageName();
            const auto package_part = package.empty() ? "" : package + "-";
            const auto version_part = version.empty() ? std::string("unknown") : version;
            std::fprintf(stdout, "Loaded ffmpeg-kit-next-flutter-%swindows-%s-%s-%s.\n", package_part.c_str(), ffmpegkit::ArchDetect::getArch().c_str(), version_part.c_str(), Config::getBuildDate().c_str());
            std::fflush(stdout);
        });
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
    if (method == "ffmpegSession" || method == "ffprobeSession" || method == "mediaInformationSession") {
        std::weak_ptr<PluginData> weak = state;
        auto complete = [weak](auto session) {
            if (auto current = weak.lock()) {
                current->executions.FinishAsync(session->getSessionId());
                if (!current->executions.IsStopping()) {
                    try {
                        current->Emit(kCompleteEventName, SessionValue(session));
                    } catch (...) {
                    }
                }
            }
        };
        auto log = [weak](auto item) {
            if (auto current = weak.lock())
                if (!current->executions.IsStopping() && current->logs) {
                    try {
                        current->Emit(kLogEventName, LogValue(item));
                    } catch (...) {
                    }
                }
        };
        auto statistics = [weak](auto item) {
            if (auto current = weak.lock())
                if (!current->executions.IsStopping() && current->statistics) {
                    try {
                        current->Emit(kStatisticsEventName, StatisticsValue(item));
                    } catch (...) {
                    }
                }
        };
        const auto arguments = args.Strings("arguments", kInvalidArgumentsArray);
        std::shared_ptr<ffmpegkit::Session> session;
        if (method == "ffmpegSession")
            session = ffmpegkit::FFmpegSession::create(arguments, complete, log, statistics);
        else if (method == "ffprobeSession")
            session = ffmpegkit::FFprobeSession::create(arguments, complete, log);
        else
            session = ffmpegkit::MediaInformationSession::create(arguments, complete, log);
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (state->retain_shutdown_sessions || Config::isShutdown())
                state->shutdown_sessions[session->getSessionId()] = session;
        }
        return SessionValue(session);
    }
    if (method == "abstractSessionGetEndTime") {
        const auto end = Milliseconds(GetSession(state, args)->getEndTime());
        return end ? Value(end) : Value();
    }
    if (method == "abstractSessionGetDuration")
        return Value(static_cast<int64_t>(GetSession(state, args)->getDuration()));
    if (method == "abstractSessionGetLogs")
        return ListValue(GetSession(state, args)->getLogs(), LogValue);
    if (method == "abstractSessionGetState")
        return Value(static_cast<int>(GetSession(state, args)->getState()));
    if (method == "abstractSessionGetReturnCode") {
        const auto code = GetSession(state, args)->getReturnCode();
        return code ? Value(code->getValue()) : Value();
    }
    if (method == "abstractSessionGetFailStackTrace")
        return Value(Utf8(GetSession(state, args)->getFailStackTrace()));
    if (method == "thereAreAsynchronousMessagesInTransmit")
        return Value(GetSession(state, args)->thereAreAsynchronousMessagesInTransmit());
    if (method == "ffmpegSessionGetStatistics")
        return ListValue(GetFFmpegSession(state, args)->getStatistics(), StatisticsValue);
    if (method == "getMediaInformation")
        return MediaValue(GetMediaSession(state, args)->getMediaInformation());
    if (method == "mediaInformationJsonParserFrom")
        return MediaValue(ffmpegkit::MediaInformationJsonParser::from(args.String("ffprobeJsonOutput", kInvalidFfprobeJsonOutput)));
    if (method == "mediaInformationJsonParserFromWithError") {
        const auto json = args.String("ffprobeJsonOutput", kInvalidFfprobeJsonOutput);
        try {
            return MediaValue(ffmpegkit::MediaInformationJsonParser::fromWithError(json));
        } catch (const std::exception &) {
            throw ChannelError("PARSE_FAILED", "Parsing MediaInformation failed with JSON error.");
        }
    }
    if (method == "asyncFFmpegSessionExecute") {
        auto session = GetFFmpegSession(state, args);
        state->StartAsyncExecution(session, [session] { Config::asyncFFmpegExecute(session); });
        return Value();
    }
    if (method == "asyncFFprobeSessionExecute") {
        auto session = GetFFprobeSession(state, args);
        state->StartAsyncExecution(session, [session] { Config::asyncFFprobeExecute(session); });
        return Value();
    }
    if (method == "asyncMediaInformationSessionExecute") {
        auto session = GetMediaSession(state, args);
        state->StartAsyncExecution(session, [session, timeout] { Config::asyncGetMediaInformationExecute(session, timeout); });
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
        Config::setFontconfigConfigurationPath(args.String("path", kInvalidPath));
        return Value();
    }
    // Like mobile, the specific check runs before the unvalidated font map.
    if (method == "setFontDirectory") {
        const auto directory = args.String("fontDirectory", kInvalidFontDirectory);
        Config::setFontDirectory(directory, args.Mapping("fontNameMap"));
        return Value();
    }
    if (method == "setFontDirectoryList") {
        const auto directories = args.Strings("fontDirectoryList", kInvalidFontDirectoryList);
        Config::setFontDirectoryList(directories, args.Mapping("fontNameMap"));
        return Value();
    }
    if (method == "setEnvironmentVariable") {
        // Like mobile, a bad value wins over a bad name.
        const auto value = args.String("variableValue", kInvalidVariableValue);
        const auto name = args.String("variableName", kInvalidVariableName);
        Config::setEnvironmentVariable(name, value);
        return Value();
    }
    if (method == "ignoreSignal") {
        const int index = args.Int("signal", kInvalidSignal);
        const ffmpegkit::Signal signals[] = {ffmpegkit::SignalInt, ffmpegkit::SignalQuit, ffmpegkit::SignalPipe, ffmpegkit::SignalTerm, ffmpegkit::SignalXcpu};
        if (index < 0 || index > 4)
            throw ChannelError("INVALID_SIGNAL", "Signal value not supported.");
        Config::ignoreSignal(signals[index]);
        return Value();
    }
    if (method == "getLogLevel")
        return Value(static_cast<int>(Config::getLogLevel()));
    if (method == "setLogLevel") {
        Config::setLogLevel(static_cast<ffmpegkit::Level>(args.Int("level", kInvalidLevel)));
        return Value();
    }
    if (method == "getSessionHistorySize")
        return Value(Config::getSessionHistorySize());
    if (method == "setSessionHistorySize") {
        Config::setSessionHistorySize(args.Int("sessionHistorySize", kInvalidSize));
        return Value();
    }
    if (method == "getLogRedirectionStrategy")
        return Value(static_cast<int>(Config::getLogRedirectionStrategy()));
    if (method == "setLogRedirectionStrategy") {
        const int strategy = args.Int("strategy", kInvalidLogRedirectionStrategy);
        if (strategy < 0 || strategy > 4)
            throw ChannelError("INVALID_LOG_REDIRECTION_STRATEGY", "Log redirection strategy value not supported.");
        Config::setLogRedirectionStrategy(static_cast<ffmpegkit::LogRedirectionStrategy>(strategy));
        return Value();
    }
    if (method == "messagesInTransmit")
        return Value(Config::messagesInTransmit(args.Int("sessionId", kInvalidSession)));
    if (method == "getSession")
        return SessionValue(Config::getSession(args.Int("sessionId", kInvalidSession)));
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
        const int filter = args.Int("state", kInvalidSessionState);
        if (filter < 0 || filter > 3)
            throw ChannelError("INVALID_SESSION_STATE", "Session state value not supported.");
        return ListValue(Config::getSessionsByState(static_cast<ffmpegkit::SessionState>(filter)), SessionValue);
    }
    if (method == "clearSessions") {
        Config::clearSessions();
        std::lock_guard<std::mutex> lock(state->mutex);
        state->shutdown_sessions.clear();
        return Value();
    }
    if (method == "deleteSession") {
        const long id = args.Int("sessionId", kInvalidSession);
        Config::deleteSession(id);
        std::lock_guard<std::mutex> lock(state->mutex);
        state->shutdown_sessions.erase(id);
        return Value();
    }
    if (method == "cancel") {
        ffmpegkit::FFmpegKit::cancel();
        return Value();
    }
    if (method == "cancelSession") {
        ffmpegkit::FFmpegKit::cancel(args.Int("sessionId", kInvalidSession));
        return Value();
    }
    if (method == "selectDocument" || method == "getSafParameter" || method == "unregisterSafProtocolUrl" || method == "getSupportedCameraIds") {
        throw ChannelError("NOT_SUPPORTED", "Not supported on Windows platform.");
    }
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
  if (method == "registerNewFFmpegPipe") {
    const auto pipe = Config::registerNewFFmpegPipe();
    return pipe ? Value(*pipe) : Value();
  }
  if (method == "closeFFmpegPipe") {
      Config::closeFFmpegPipe(args.String("ffmpegPipePath", kInvalidPipePath));
      return Value();
  }
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
  if (method == "writeToPipe") {
    throw ChannelError("NOT_SUPPORTED", "Named pipes are not supported on Windows.");
  }
  if (method == "inputBufferFromByteArray" || method == "outputBufferCreate" ||
      method == "streamInputCreate" || method == "streamOutputCreate") {
      // Like mobile, the data check runs before the unvalidated extension.
      const auto *data = method == "inputBufferFromByteArray" ? &args.Bytes("data", kInvalidData) : nullptr;
      const auto extension = args.OptionalString("extension");
      Resource resource;
      if (data) {
          resource = ffmpegkit::FFmpegKitInputBuffer::fromByteArray(*data, extension);
      } else if (method == "outputBufferCreate") {
          resource = args.Find("initialCapacity") && args.Find("maxCapacity") ? ffmpegkit::FFmpegKitOutputBuffer::create(extension, args.Int("initialCapacity"), args.Int("maxCapacity")) : ffmpegkit::FFmpegKitOutputBuffer::create(extension);
      } else if (method == "streamInputCreate") {
          resource = args.Find("capacity") ? ffmpegkit::FFmpegKitStreamInput::create(extension, args.Int("capacity")) : ffmpegkit::FFmpegKitStreamInput::create(extension);
      } else {
          resource = args.Find("capacity") ? ffmpegkit::FFmpegKitStreamOutput::create(extension, args.Int("capacity")) : ffmpegkit::FFmpegKitStreamOutput::create(extension);
      }
    const auto url = std::visit([](const auto &item) { return item->getUrl(); },
                                resource);
    std::lock_guard<std::mutex> lock(state->mutex);
    state->resources[url] = std::move(resource);
    return Value(url);
  }
  if (method == "inputBufferClose" || method == "outputBufferClose" ||
      method == "streamInputClose" || method == "streamOutputClose") {
      const auto &check = method == "inputBufferClose" ? kInvalidInputBufferUrl : method == "outputBufferClose" ? kInvalidOutputBufferUrl : method == "streamInputClose" ? kInvalidStreamInputUrl : kInvalidStreamOutputUrl;
      const auto url = args.String("url", check);
      std::lock_guard<std::mutex> lock(state->mutex);
      const auto found = state->resources.find(url);
      if (found != state->resources.end()) {
          std::visit([](const auto &item) { item->close(); }, found->second);
          state->resources.erase(found);
      }
    return Value();
  }
  if (method == "outputBufferGetSize") {
      const auto buffer = state->FindResource<ffmpegkit::FFmpegKitOutputBuffer>(args.String("url", kInvalidOutputBufferUrl), "Output buffer not found.");
      return Value(static_cast<int64_t>(buffer->getSize()));
  }
  if (method == "outputBufferToByteArray") {
      const auto buffer = state->FindResource<ffmpegkit::FFmpegKitOutputBuffer>(args.String("url", kInvalidOutputBufferUrl), "Output buffer not found.");
      return BytesResult(buffer->toByteArray());
  }
  if (method == "streamInputCloseInput") {
      const auto url = args.String("url", kInvalidStreamInputUrl);
      std::shared_ptr<ffmpegkit::FFmpegKitStreamInput> stream;
      {
          std::lock_guard<std::mutex> lock(state->mutex);
          const auto found = state->resources.find(url);
          if (found != state->resources.end()) {
              if (const auto *resource = std::get_if<std::shared_ptr<ffmpegkit::FFmpegKitStreamInput>>(&found->second))
                  stream = *resource;
          }
      }
      // Like Linux, input closure is a no-op once the input stream is absent.
      if (stream)
          stream->closeInput();
      return Value();
  }
  throw ChannelError("METHOD_NOT_IMPLEMENTED", method);
}

struct PreparedCall {
    bool background;
    std::function<Value()> invoke;
    // Code for a std::exception that escapes invoke. Like Linux, background
    // work fails with EXECUTION_FAILED and shutdown with SHUTDOWN_FAILED.
    const char *failure_code;
};

constexpr char kExecutionFailed[] = "EXECUTION_FAILED";
constexpr char kNativeError[] = "NATIVE_ERROR";

PreparedCall PrepareCall(const std::shared_ptr<PluginData> &state, const std::string &method, const Value &arguments) {
    const Arguments args(arguments);
    // Shutdown can block, so it runs in the background like Linux. Its
    // required waitTimeout is checked before the optional one below.
    if (method == "shutdown") {
        const int budget = args.Int("waitTimeout", kInvalidShutdownTimeout);
        return {true,
                [state, budget] {
                    {
                        std::lock_guard<std::mutex> lock(state->mutex);
                        state->retain_shutdown_sessions = true;
                    }
                    auto sessions = Config::getSessions();
                    {
                        std::lock_guard<std::mutex> lock(state->mutex);
                        if (sessions)
                            for (const auto &session : *sessions)
                                state->shutdown_sessions[session->getSessionId()] = session;
                    }
                    const auto result = Config::shutdown(budget);
                    state->executions.PruneAsync();
                    return Value(result);
                },
                "SHUTDOWN_FAILED"};
    }
    const int default_timeout = ffmpegkit::AbstractSession::DefaultTimeoutForAsynchronousMessagesInTransmit;
    const int wait = args.OptionalInt("waitTimeout", default_timeout);
    const int timeout = wait > 0 ? wait : default_timeout;
    if (method == "ffmpegSessionExecute") {
        auto session = GetFFmpegSession(state, args);
        return {true,
                [state, session] {
                    auto execution = state->StartExecution(session);
                    Config::ffmpegExecute(session);
                    return Value();
                },
                kExecutionFailed};
    }
    if (method == "ffprobeSessionExecute") {
        auto session = GetFFprobeSession(state, args);
        return {true,
                [state, session] {
                    auto execution = state->StartExecution(session);
                    Config::ffprobeExecute(session);
                    return Value();
                },
                kExecutionFailed};
    }
    if (method == "mediaInformationSessionExecute") {
        auto session = GetMediaSession(state, args);
        return {true,
                [state, session, timeout] {
                    auto execution = state->StartExecution(session);
                    Config::getMediaInformationExecute(session, timeout);
                    return Value();
                },
                kExecutionFailed};
    }

    if (method == "abstractSessionGetAllLogs" || method == "abstractSessionGetAllLogsAsString") {
        auto session = GetSession(state, args);
        return {true,
                [session, method, timeout] {
                    if (method == "abstractSessionGetAllLogs")
                        return ListValue(session->getAllLogsWithTimeout(timeout), LogValue);
                    return Value(Utf8(session->getAllLogsAsStringWithTimeout(timeout)));
                },
                kExecutionFailed};
    }
    if (method == "ffmpegSessionGetAllStatistics") {
        auto session = GetFFmpegSession(state, args);
        return {true, [session, timeout] { return ListValue(session->getAllStatisticsWithTimeout(timeout), StatisticsValue); }, kExecutionFailed};
    }
    // Like mobile, stream I/O validates every argument before the lookup.
    if (method == "streamInputWrite") {
        const auto url = args.String("url", kInvalidStreamInputArguments);
        const auto &data = args.Bytes("data", kInvalidStreamInputArguments);
        const bool timed = args.Find("timeoutMs") != nullptr;
        const int timeout_ms = timed ? args.Int("timeoutMs", kInvalidStreamInputArguments) : 0;
        auto stream = state->FindResource<ffmpegkit::FFmpegKitStreamInput>(url, "Stream input not found.");
        // The worker outlives the method call, so it owns its one copy.
        std::vector<uint8_t> bytes = data;
        const long stream_id = StreamId(url);
        return {true,
                [state, stream, stream_id, bytes = std::move(bytes), timed, timeout_ms] {
                    auto execution = state->StartStreamIo(stream_id);
                    try {
                        return Value(timed ? stream->write(bytes, timeout_ms) : stream->write(bytes));
                    } catch (const std::exception &error) {
                        throw ChannelError("STREAM_INPUT_WRITE_FAILED", error.what());
                    }
                },
                kExecutionFailed};
    }
    if (method == "streamOutputRead") {
        const auto url = args.String("url", kInvalidStreamOutputArguments);
        const int max_bytes = args.Int("maxBytes", kInvalidStreamOutputArguments);
        const bool timed = args.Find("timeoutMs") != nullptr;
        const int timeout_ms = timed ? args.Int("timeoutMs", kInvalidStreamOutputArguments) : 0;
        auto stream = state->FindResource<ffmpegkit::FFmpegKitStreamOutput>(url, "Stream output not found.");
        const long stream_id = StreamId(url);
        return {true,
                [state, stream, stream_id, max_bytes, timed, timeout_ms] {
                    auto execution = state->StartStreamIo(stream_id);
                    std::shared_ptr<std::vector<uint8_t>> bytes;
                    try {
                        bytes = timed ? stream->read(max_bytes, timeout_ms) : stream->read(max_bytes);
                    } catch (const std::exception &error) {
                        throw ChannelError("STREAM_OUTPUT_READ_FAILED", error.what());
                    }
                    return BytesResult(bytes);
                },
                kExecutionFailed};
    }
    // The remaining methods run on the platform thread before the handler
    // returns, so they read the call's arguments in place instead of copying.
    return {false, [state, &method, &arguments] { return Dispatch(state, method, Arguments(arguments)); }, kNativeError};
}

class WindowsPlugin : public flutter::Plugin {
public:
  explicit WindowsPlugin(flutter::PluginRegistrarWindows *registrar) : state_(std::make_shared<PluginData>()) {
      auto *codec = &flutter::StandardMethodCodec::GetInstance();
      methods_ = std::make_unique<flutter::MethodChannel<Value>>(registrar->messenger(), kMethodChannelName, codec);
      events_ = std::make_unique<flutter::EventChannel<Value>>(registrar->messenger(), kEventChannelName, codec);
      events_->SetStreamHandler(std::make_unique<flutter::StreamHandlerFunctions<Value>>(
          [state = state_](const Value *, std::unique_ptr<flutter::EventSink<Value>> &&sink) -> std::unique_ptr<flutter::StreamHandlerError<Value>> {
              state->sink = std::move(sink);
              return nullptr;
          },
          [state = state_](const Value *) -> std::unique_ptr<flutter::StreamHandlerError<Value>> {
              state->sink.reset();
              return nullptr;
          }));
      state_->delete_listener = std::make_shared<DeleteListener>(state_);
      Config::addSessionDeleteListener(state_->delete_listener);
      methods_->SetMethodCallHandler([state = state_](const flutter::MethodCall<Value> &call, std::unique_ptr<Result> response) {
          auto result = std::shared_ptr<Result>(std::move(response));
          auto pending = std::make_shared<PendingResponse>(result);
          if (!state->executions.AddPendingResponse(pending)) {
              result->Error("PLUGIN_SHUTDOWN", kPluginShutdownMessage);
              return;
          }
          const auto method = call.method_name();
          // Borrowed from the call: only valid until this handler returns.
          static const Value kNoArguments;
          const Value &arguments = call.arguments() ? *call.arguments() : kNoArguments;
          PreparedCall prepared;
          try {
              prepared = PrepareCall(state, method, arguments);
          } catch (const ChannelError &error) {
              if (state->executions.ClaimPendingResponse(pending))
                  result->Error(error.code, Utf8(error.what()));
              return;
          } catch (const ArgumentError &error) {
              if (state->executions.ClaimPendingResponse(pending))
                  result->Error(error.code, Utf8(error.what()));
              return;
          } catch (const std::exception &error) {
              if (state->executions.ClaimPendingResponse(pending))
                  result->Error(kNativeError, Utf8(error.what()));
              return;
          } catch (...) {
              if (state->executions.ClaimPendingResponse(pending))
                  result->Error(kNativeError, Utf8("Unknown native error."));
              return;
          }
          const bool background = prepared.background;
          auto run = [state, invoke = std::move(prepared.invoke), failure_code = prepared.failure_code, background, pending](bool post_response) {
              // Teardown owns the pending response once the state is dead.
              // A detached worker scheduled late must not enter native code.
              if (state->executions.IsStopping())
                  return;
              Value value;
              std::string code, message;
              try {
                  value = invoke();
              } catch (const ChannelError &error) {
                  code = error.code;
                  message = error.what();
              } catch (const ArgumentError &error) {
                  code = error.code;
                  message = error.what();
              } catch (const std::exception &error) {
                  code = failure_code;
                  message = error.what();
              } catch (...) {
                  code = background ? kExecutionFailed : kNativeError;
                  message = background ? "Native execution failed." : "Unknown native error.";
              }
              auto respond = [state, pending, value = std::move(value), code, message] {
                  if (!state->executions.ClaimPendingResponse(pending))
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
          if (background) {
              try {
                  std::thread([run = std::move(run)] { run(true); }).detach();
              } catch (const std::exception &error) {
                  if (state->executions.ClaimPendingResponse(pending))
                      pending->result->Error(kExecutionFailed, Utf8(error.what()));
              }
          } else
              run(false);
      });
  }

  ~WindowsPlugin() override {
      // Begin shutdown before releasing channel-owned objects so detached
      // workers cannot access them during shutdown.
      state_->executions.BeginShutdown();
      state_->executions.CompletePendingResponses("PLUGIN_SHUTDOWN", kPluginShutdownMessage);
      state_->sink.reset();
      try {
          Config::removeSessionDeleteListener(state_->delete_listener);
      } catch (...) {
      }
      state_->executions.CancelAsync();
      std::map<std::string, Resource> resources;
      {
          std::lock_guard<std::mutex> lock(state_->mutex);
          resources.swap(state_->resources);
      }
      // Wake blocked stream I/O now through the native registry. Buffers and
      // stream handles close after executions drain, as on Linux.
      for (const auto &entry : resources) {
          const bool stream = std::holds_alternative<std::shared_ptr<ffmpegkit::FFmpegKitStreamInput>>(entry.second) || std::holds_alternative<std::shared_ptr<ffmpegkit::FFmpegKitStreamOutput>>(entry.second);
          if (stream) {
              try {
                  Config::unregisterFFmpegKitStream(StreamId(entry.first));
              } catch (...) {
              }
          }
      }
      // FFprobe and media information executions cannot be cancelled. Cancel and
      // wait off the platform thread so closing the app never blocks it; workers
      // never need a platform reply to release their lease.
      auto drain = [state = state_, resources = std::move(resources)] {
          state->executions.CancelAndWait();
          for (const auto &entry : resources) {
              try {
                  std::visit([](const auto &item) { item->close(); }, entry.second);
              } catch (...) {
              }
          }
      };
      try {
          std::thread(drain).detach();
      } catch (...) {
          drain();
      }
      // The dispatcher window belongs to this thread. Replies posted after this
      // are dropped; teardown already answered every pending call.
      state_->dispatcher->Close();
  }

private:
  std::shared_ptr<PluginData> state_;
  std::unique_ptr<flutter::MethodChannel<Value>> methods_;
  std::unique_ptr<flutter::EventChannel<Value>> events_;
};
} // namespace ffmpeg_kit_windows

void FFmpegKitFlutterPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  try {
    auto *wrapper =
        flutter::PluginRegistrarManager::GetInstance()
            ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar);
    wrapper->AddPlugin(
        std::make_unique<ffmpeg_kit_windows::WindowsPlugin>(wrapper));
  } catch (const std::exception &error) {
    std::fprintf(stderr,
                 "Failed to register ffmpeg-kit-next-flutter Windows plugin: %s\n",
                 error.what());
  } catch (...) {
    std::fprintf(stderr,
                 "Failed to register ffmpeg-kit-next-flutter Windows plugin: "
                 "unknown exception.\n");
  }
}
