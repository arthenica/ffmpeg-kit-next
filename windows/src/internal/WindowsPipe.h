#ifndef FFMPEG_KIT_WINDOWS_PIPE_H
#define FFMPEG_KIT_WINDOWS_PIPE_H

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <atomic>
#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace ffmpegkit {
namespace internal {
class WindowsPipe {
 public:
  WindowsPipe() {
    static std::atomic<unsigned long> counter{0};
    path_ = "\\\\.\\pipe\\ffmpeg-kit-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(++counter);
    pipe_ = CreateNamedPipeA(path_.c_str(), PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
                            PIPE_TYPE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 65536, 65536, 0, nullptr);
    if (pipe_ == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create FFmpeg named pipe.");
    stopped_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!stopped_) { CloseHandle(pipe_); throw std::runtime_error("Cannot create pipe cancellation event."); }
  }

  ~WindowsPipe() {
    Close();
    CloseHandle(stopped_);
    CloseHandle(pipe_);
  }

  std::string Path() const { return path_; }

  void Close() {
    SetEvent(stopped_);
    CancelIoEx(pipe_, nullptr);
    DisconnectNamedPipe(pipe_);
  }

  int WriteFileContents(const std::string& path) {
    std::unique_lock<std::mutex> writer_lock(writer_mutex_, std::try_to_lock);
    if (!writer_lock.owns_lock()) throw std::runtime_error("Pipe already has a writer.");
    if (WaitForSingleObject(stopped_, 0) == WAIT_OBJECT_0) throw std::runtime_error("Pipe is closed.");
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.c_str(), -1, nullptr, 0);
    if (!length) throw std::runtime_error("Invalid UTF-8 input path.");
    std::wstring wide(length, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.c_str(), -1, &wide[0], length);
    HANDLE input = CreateFileW(wide.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (input == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot open pipe input file.");
    bool success = false;
    try {
      Connect();
      success = true;
      std::array<unsigned char, 65536> bytes;
      DWORD count = 0;
      while (success) {
        if (WaitForSingleObject(stopped_, 0) == WAIT_OBJECT_0) { success = false; break; }
        if (!ReadFile(input, bytes.data(), static_cast<DWORD>(bytes.size()), &count, nullptr)) { success = false; break; }
        if (!count) break;
        WriteBytesLocked(bytes.data(), count);
      }
      if (success) success = FlushFileBuffers(pipe_) != FALSE;
    } catch (...) {
      success = false;
    }
    CloseHandle(input);
    DisconnectNamedPipe(pipe_);
    connected_ = false;
    if (!success) throw std::runtime_error("Pipe transfer failed or was cancelled.");
    return 0;
  }

  int WriteBytes(const uint8_t* bytes, size_t length) {
    if (bytes == nullptr && length > 0) throw std::runtime_error("Pipe bytes are required.");
    if (length > static_cast<size_t>((std::numeric_limits<int>::max)())) {
      throw std::runtime_error("Pipe byte chunk exceeds the supported size.");
    }
    std::unique_lock<std::mutex> writer_lock(writer_mutex_);
    if (WaitForSingleObject(stopped_, 0) == WAIT_OBJECT_0) throw std::runtime_error("Pipe is closed.");
    Connect();
    WriteBytesLocked(bytes, length);
    return static_cast<int>(length);
  }

 private:
  void Connect() {
    if (connected_) return;
    HANDLE event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!event) throw std::runtime_error("Cannot create pipe I/O event.");
    OVERLAPPED operation{};
    operation.hEvent = event;
    DWORD transferred = 0;
    const BOOL connected = ConnectNamedPipe(pipe_, &operation);
    const DWORD connect_error = connected ? ERROR_SUCCESS : GetLastError();
    const bool ready = connected || connect_error == ERROR_PIPE_CONNECTED ||
        (connect_error == ERROR_IO_PENDING && Await(operation, transferred));
    CloseHandle(event);
    if (!ready) throw std::runtime_error("Pipe connection failed or was cancelled.");
    connected_ = true;
  }

  void WriteBytesLocked(const uint8_t* bytes, size_t length) {
    size_t offset = 0;
    while (offset < length) {
      HANDLE event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
      if (!event) throw std::runtime_error("Cannot create pipe I/O event.");
      OVERLAPPED operation{};
      operation.hEvent = event;
      DWORD transferred = 0;
      const DWORD remaining = static_cast<DWORD>((std::min)(length - offset, static_cast<size_t>((std::numeric_limits<DWORD>::max)())));
      const BOOL written = WriteFile(pipe_, bytes + offset, remaining, &transferred, &operation);
      const bool completed = written || (GetLastError() == ERROR_IO_PENDING && Await(operation, transferred));
      CloseHandle(event);
      if (!completed || !transferred) throw std::runtime_error("Pipe transfer failed or was cancelled.");
      offset += transferred;
    }
  }

  bool Await(OVERLAPPED& operation, DWORD& transferred) {
    const HANDLE events[] = {stopped_, operation.hEvent};
    if (WaitForMultipleObjects(2, events, FALSE, INFINITE) != WAIT_OBJECT_0 + 1) {
      CancelIoEx(pipe_, &operation);
      GetOverlappedResult(pipe_, &operation, &transferred, TRUE);
      return false;
    }
    return GetOverlappedResult(pipe_, &operation, &transferred, FALSE) != FALSE;
  }

  std::string path_;
  HANDLE pipe_ = INVALID_HANDLE_VALUE;
  HANDLE stopped_ = nullptr;
  std::mutex writer_mutex_;
  bool connected_ = false;
};

class WindowsPipeRegistry {
 public:
  static std::string Create() {
    auto pipe = std::make_shared<WindowsPipe>();
    std::lock_guard<std::mutex> lock(Mutex());
    Pipes()[pipe->Path()] = pipe;
    return pipe->Path();
  }

  static void Close(const std::string& path) {
    std::shared_ptr<WindowsPipe> pipe;
    {
      std::lock_guard<std::mutex> lock(Mutex());
      auto found = Pipes().find(path);
      if (found == Pipes().end()) return;
      pipe = found->second;
      Pipes().erase(found);
    }
    pipe->Close();
  }

  static int Write(const std::string& input, const std::string& path) {
    std::shared_ptr<WindowsPipe> pipe;
    {
      std::lock_guard<std::mutex> lock(Mutex());
      auto found = Pipes().find(path);
      if (found == Pipes().end()) throw std::runtime_error("Pipe not found.");
      pipe = found->second;
    }
    return pipe->WriteFileContents(input);
  }

  static int WriteBytes(const uint8_t* bytes, size_t length, const std::string& path) {
    std::shared_ptr<WindowsPipe> pipe;
    {
      std::lock_guard<std::mutex> lock(Mutex());
      auto found = Pipes().find(path);
      if (found == Pipes().end()) throw std::runtime_error("Pipe not found.");
      pipe = found->second;
    }
    return pipe->WriteBytes(bytes, length);
  }

 private:
  static std::map<std::string, std::shared_ptr<WindowsPipe>>& Pipes() {
    static std::map<std::string, std::shared_ptr<WindowsPipe>> pipes;
    return pipes;
  }
  static std::mutex& Mutex() { static std::mutex mutex; return mutex; }
};
}
}

#endif
