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

// FFmpegKitWorker — the worker-thread host for the FFmpegKitNext wasm module.
//
// It is the ONLY place that imports libffmpegkit.js and holds the raw embind
// Module, and the ONLY caller of the internal event-bridge free functions
// (_ffmpegkitEnableEventBuffering / _ffmpegkitDrainLogEvents /
// _ffmpegkitDrainStatisticsEvents / _ffmpegkitSetSessionDeletedCallback). It runs
// execution off the UI thread, serializes sessions into plain data for
// FFmpegKitFactory, and forwards live log/statistics events drained from the
// buffer while native FFmpegKit runs on its own pthread.

import createFFmpegKitModule from '../lib/libffmpegkit.js';

const PRINT_LOAD_CONFIRMATION =
  new URL(self.location.href).searchParams.get('printLoadConfirmation') !== '0';

let Module = null;

// Matches the native AbstractSession default wait for asynchronous media-information.
const DEFAULT_WAIT_TIMEOUT = 5000;
// Poll step while waiting for a session's messages in transmit. Native sleeps in 100 ms
// steps; this thread can afford to look more often because a tick is two counter reads
// and a drain, and a shorter step keeps the common "nothing pending" case cheap.
const MESSAGES_IN_TRANSMIT_POLL_INTERVAL = 20;
const SESSION_TYPE_FFMPEG = 1;
const SESSION_TYPE_FFPROBE = 2;
const SESSION_TYPE_MEDIA_INFORMATION = 3;
const SESSION_STATE_CREATED = 0;
const SESSION_STATE_RUNNING = 1;
const SESSION_STATE_FAILED = 2;
const SESSION_STATE_COMPLETED = 3;
const LOG_REDIRECTION_STRATEGY_NEVER_PRINT_LOGS = 4;
const MSG_BOOT = 0;
const MSG_FATAL = 1;
const MSG_RESULT = 2;
const MSG_ERROR = 3;
const MSG_LOG = 4;
const MSG_STATISTICS = 5;
const MSG_SESSION_DELETED = 6;

// ffkitmem:/ffkitstream: I/O objects live here, keyed by an opaque handle returned
// to the app. The app never holds the embind object, only the handle + its URL.
const ioObjects = new Map();
let ioSeq = 0;

function mkdirTree(path) {
  try {
    Module.FS.mkdirTree(path);
  } catch {
    /* exists */
  }
}

function mkdirParent(path) {
  const index = path.lastIndexOf('/');
  if (index > 0) mkdirTree(path.slice(0, index));
}

function setFontDirectoryList(args) {
  const fontDirectoryList = Array.isArray(args.fontDirectoryList)
    ? args.fontDirectoryList
    : [];
  const fontNameMapping =
    args.fontNameMapping && typeof args.fontNameMapping === 'object'
      ? args.fontNameMapping
      : {};
  Module.FFmpegKitConfig.setFontDirectoryList(fontDirectoryList, fontNameMapping);
}

function setFontconfigConfigurationPath(args) {
  Module.FFmpegKitConfig.setFontconfigConfigurationPath(args.path);
}

function isLoadConfirmation(line) {
  return String(line).startsWith('Loaded ffmpeg-kit-next-');
}

function printStdout(line) {
  if (!PRINT_LOAD_CONFIRMATION && isLoadConfirmation(line)) return;
  console.log(line);
}

// Normalizes an enum-shaped value read from the module into the plain number the
// public API carries everywhere (Log.getLevel(), FFmpegKitConfig.getLogLevel(),
// Session.getState(), every payload posted from here) - the same representation the
// Flutter and React Native plugins put on their platform channels.
//
// The bindings expose every enum as an int (see the enum note in
// FFmpegKitBindings.cpp), so this is normally an identity. The value-object branch
// keeps an older module - one still binding the C++ enum directly, which embind hands
// over as a value object - from turning into `{}` on structured clone and silently
// disabling every comparison on the main thread.
function enumToNumber(value) {
  if (value == null) return null;
  if (value && typeof value === 'object') {
    value = value.value;
  }
  const numeric = Number(value);
  return Number.isFinite(numeric) ? numeric : null;
}

function levelToNumber(level) {
  return enumToNumber(level);
}

function currentLogLevel() {
  return levelToNumber(Module.FFmpegKitConfig.getLogLevel());
}

// Builds the worker-side embind I/O object for an ioCreate request.
function createIoObject(args) {
  const ext = args.extension || '';
  switch (args.kind) {
    case 'inputBuffer':
      return Module.FFmpegKitInputBuffer.fromByteArray(new Uint8Array(args.data), ext);
    case 'outputBuffer':
      return args.initialCapacity != null && args.maxCapacity != null
        ? Module.FFmpegKitOutputBuffer.createWithCapacity(
            ext,
            args.initialCapacity ?? 0,
            args.maxCapacity ?? 0
          )
        : Module.FFmpegKitOutputBuffer.create(ext);
    case 'streamInput':
      return args.capacity != null
        ? Module.FFmpegKitStreamInput.createWithCapacity(ext, args.capacity)
        : Module.FFmpegKitStreamInput.create(ext);
    case 'streamOutput':
      return args.capacity != null
        ? Module.FFmpegKitStreamOutput.createWithCapacity(ext, args.capacity)
        : Module.FFmpegKitStreamOutput.create(ext);
    default:
      throw new Error('Unknown I/O kind: ' + args.kind);
  }
}

function createNativeSession(args) {
  const commandArguments = Array.isArray(args.arguments) ? args.arguments : [];

  // Native log callbacks are permanently installed for event buffering. Keep native
  // sessions silent and apply the user's strategy only in FFmpegKitFactory._dispatchLog.
  switch (args.kind) {
    case 'ffmpeg':
      return Module.FFmpegSession.create(
        commandArguments,
        LOG_REDIRECTION_STRATEGY_NEVER_PRINT_LOGS
      );
    case 'ffprobe':
      return Module.FFprobeSession.create(
        commandArguments,
        LOG_REDIRECTION_STRATEGY_NEVER_PRINT_LOGS
      );
    case 'mediaInformation':
      return Module.MediaInformationSession.create(commandArguments);
    default:
      throw new Error('Unknown session kind: ' + args.kind);
  }
}

function getNativeSession(kind, sessionId) {
  let session = null;
  if (kind === 'ffmpeg') {
    session = Module.FFmpegKitConfig.getFFmpegSession(sessionId);
  } else if (kind === 'ffprobe') {
    session = Module.FFmpegKitConfig.getFFprobeSession(sessionId);
  } else if (kind === 'mediaInformation') {
    session = Module.FFmpegKitConfig.getMediaInformationSession(sessionId);
  }

  if (!session) {
    throw new Error(`Native ${kind} session not found: ${sessionId}`);
  }
  return session;
}

// Looks up an I/O object by handle, throwing when it is unknown - a handle that was
// already closed, or one from a previous wasm runtime. Every op except ioClose (which
// stays idempotent) goes through here: returning a neutral value instead would be
// indistinguishable from backpressure on the streaming ops, where 0 bytes written and a
// null read both mean "retry", so a caller would spin forever on a dead handle.
function requireIoObject(handle) {
  const object = ioObjects.get(handle);
  if (!object) {
    throw new Error('I/O object not found: ' + handle);
  }
  return object;
}

function requireSessionId(args, op) {
  if (args.sessionId == null) {
    throw new Error(op + ' requires an existing sessionId.');
  }
  return args.sessionId;
}

// waitTimeout as the native getAll* overloads expect it: the caller's value when given,
// otherwise the native AbstractSession default.
function waitTimeout(args) {
  const timeout = Number(args.waitTimeout);
  return Number.isFinite(timeout) ? timeout : DEFAULT_WAIT_TIMEOUT;
}

// Waits for the session's messages in transmit, then runs `read` against the native
// history session named by args.sessionId and posts what it returns. Backs the getAll*
// ops, which read a live or completed session by id.
//
// The wait belongs to this thread (see waitForMessagesInTransmit), so `read` must use the
// NON-waiting native getters - getLogs() / getLogsAsString() / getStatistics(). That is
// exactly what native's getAll*WithTimeout() does once its own wait returns; the only
// difference is which thread does the waiting.
async function withHistorySession(id, args, op, read) {
  const sessionId = requireSessionId(args, op);
  await waitForMessagesInTransmit(id, sessionId, waitTimeout(args));

  const session = getNativeHistorySession(sessionId);
  if (!session) {
    throw new Error(`Native session not found: ${sessionId}`);
  }
  try {
    postMessage({ id, type: MSG_RESULT, result: read(session) });
  } finally {
    session.delete();
  }
}

async function workerInit() {
  try {
    Module = await createFFmpegKitModule({
      locateFile: (path) => new URL('../lib/' + path, import.meta.url).href,
      print: printStdout,
      printErr: (line) => console.error(line),
      // Native execution runs on its own pthread. If that thread traps, the session
      // object is simply left in Running forever - nothing throws on this thread, so
      // waitForSession would poll indefinitely and its request would never settle.
      // Emscripten proxies a pthread abort here, which is the one reliable signal that
      // the runtime is dead; report it as fatal so the host fails every pending request.
      onAbort: (reason) =>
        postMessage({
          type: MSG_FATAL,
          message: 'wasm runtime aborted: ' + (reason ?? 'unknown reason'),
        }),
    });
  } catch (err) {
    postMessage({ type: MSG_FATAL, message: 'Failed to instantiate module: ' + errMessage(err) });
    return;
  }

  if (!Module.FFmpegKit || !Module.FFprobeKit || !Module.FFmpegKitConfig) {
    postMessage({
      type: MSG_FATAL,
      message:
        'FFmpegKit bindings are not present on the module. The embind ' +
        'registration was likely stripped by MAIN_MODULE=2 DCE (check the anchor).',
    });
    return;
  }

  // Install the C++ log/statistics buffering before any execute, so no early logs
  // are missed. We drain that buffer from this thread; see drainAndForward.
  safe(() => Module._ffmpegkitEnableEventBuffering());
  // Native session-history deletion is authoritative for main-thread callback
  // routing. Once a session id is deleted, later events for it are intentionally
  // ignored by FFmpegKitFactory.
  safe(() =>
    Module._ffmpegkitSetSessionDeletedCallback((sessionId) => {
      postMessage({
        type: MSG_SESSION_DELETED,
        sessionId: Number(sessionId),
      });
    })
  );

  postMessage({
    type: MSG_BOOT,
    version: safe(() => Module.FFmpegKitConfig.getVersion()),
    ffmpegVersion: safe(() => Module.FFmpegKitConfig.getFFmpegVersion()),
    buildDate: safe(() => Module.FFmpegKitConfig.getBuildDate()),
    logLevel: safe(() => currentLogLevel()),
  });
}

// ---- Serialization: embind objects -> plain data for postMessage ---------------

// getState() is bound as a plain int, so this is normally an identity. Anything the
// module cannot express as one of the four states is reported as CREATED, which is what
// a default-constructed native session holds.
function stateToNumber(state) {
  const numeric = enumToNumber(state);
  switch (numeric) {
    case SESSION_STATE_CREATED:
    case SESSION_STATE_RUNNING:
    case SESSION_STATE_FAILED:
    case SESSION_STATE_COMPLETED:
      return numeric;
    default:
      return SESSION_STATE_CREATED;
  }
}

function sessionType(session) {
  if (session.isFFprobe()) return SESSION_TYPE_FFPROBE;
  if (session.isMediaInformation()) return SESSION_TYPE_MEDIA_INFORMATION;
  return SESSION_TYPE_FFMPEG;
}

function serializeLogList(arr) {
  const out = [];
  for (let i = 0; i < arr.length; i++) {
    const l = arr[i];
    try {
      out.push({
        sessionId: Number(l.getSessionId()),
        level: levelToNumber(l.getLevel()),
        message: l.getMessage(),
      });
    } finally {
      l.delete();
    }
  }
  return out;
}

function serializeStatisticsList(arr) {
  const out = [];
  for (let i = 0; i < arr.length; i++) {
    const s = arr[i];
    try {
      out.push(statObject(s));
    } finally {
      s.delete();
    }
  }
  return out;
}

// The non-waiting native getters, deliberately: the getAll* family blocks the calling
// thread, and blocking this one stalls the whole worker (see waitForMessagesInTransmit).
// Every caller is already past the point where waiting could add anything - the execute
// ops serialize only after waitForSession() has seen the session finish and drained a
// final time, and a history snapshot taken while a session is still running has its
// log/statistics fields dropped by the host anyway (withoutLiveEventFields in
// FFmpegKitFactory.js).
function serializeLogs(session) {
  return serializeLogList(session.getLogs());
}

function serializeStatistics(session) {
  return serializeStatisticsList(session.getStatistics());
}

function statObject(s) {
  return {
    sessionId: Number(s.getSessionId()),
    frame: s.getVideoFrameNumber(),
    fps: s.getVideoFps(),
    quality: s.getVideoQuality(),
    size: Number(s.getSize()),
    time: s.getTime(),
    bitrate: s.getBitrate(),
    speed: s.getSpeed(),
  };
}

function objectOrEmpty(value) {
  return value && typeof value === 'object' && !Array.isArray(value) ? value : {};
}

function setFallback(object, key, value) {
  if (object[key] === undefined && value !== undefined && value !== null) {
    object[key] = value;
  }
}

function serializeStreamInformation(stream) {
  const data = objectOrEmpty(safe(() => stream.getAllProperties()));
  setFallback(data, 'index', stream.getIndex());
  setFallback(data, 'codec_type', stream.getType());
  setFallback(data, 'codec_name', stream.getCodec());
  setFallback(data, 'codec_long_name', stream.getCodecLong());
  setFallback(data, 'pix_fmt', stream.getFormat());
  setFallback(data, 'width', stream.getWidth());
  setFallback(data, 'height', stream.getHeight());
  setFallback(data, 'bit_rate', stream.getBitrate());
  setFallback(data, 'sample_rate', stream.getSampleRate());
  setFallback(data, 'sample_fmt', safe(() => stream.getSampleFormat()));
  setFallback(data, 'channel_layout', stream.getChannelLayout());
  setFallback(data, 'sample_aspect_ratio', safe(() => stream.getSampleAspectRatio()));
  setFallback(data, 'display_aspect_ratio', safe(() => stream.getDisplayAspectRatio()));
  setFallback(data, 'avg_frame_rate', safe(() => stream.getAverageFrameRate()));
  setFallback(data, 'r_frame_rate', safe(() => stream.getRealFrameRate()));
  setFallback(data, 'time_base', safe(() => stream.getTimeBase()));
  setFallback(data, 'codec_time_base', safe(() => stream.getCodecTimeBase()));
  setFallback(data, 'tags', safe(() => stream.getTags()));
  return data;
}

function serializeChapter(chapter) {
  const data = objectOrEmpty(safe(() => chapter.getAllProperties()));
  setFallback(data, 'id', chapter.getId());
  setFallback(data, 'time_base', safe(() => chapter.getTimeBase()));
  setFallback(data, 'start', chapter.getStart());
  setFallback(data, 'start_time', chapter.getStartTime());
  setFallback(data, 'end', chapter.getEnd());
  setFallback(data, 'end_time', chapter.getEndTime());
  setFallback(data, 'tags', safe(() => chapter.getTags()));
  return data;
}

function serializeMediaInformation(info) {
  const data = objectOrEmpty(safe(() => info.getAllProperties()));

  const streams = [];
  const arr = info.getStreams();
  for (let i = 0; i < arr.length; i++) {
    const s = arr[i];
    try {
      streams.push(serializeStreamInformation(s));
    } finally {
      s.delete();
    }
  }
  data.streams = streams;

  const chapters = [];
  const chapterArray = safe(() => info.getChapters()) || [];
  for (let i = 0; i < chapterArray.length; i++) {
    const chapter = chapterArray[i];
    try {
      chapters.push(serializeChapter(chapter));
    } finally {
      chapter.delete();
    }
  }
  data.chapters = chapters;

  if (!data.format || typeof data.format !== 'object') {
    data.format = {};
  }
  setFallback(data.format, 'filename', info.getFilename());
  setFallback(data.format, 'format_name', info.getFormat());
  setFallback(data.format, 'format_long_name', info.getLongFormat());
  setFallback(data.format, 'duration', info.getDuration());
  setFallback(data.format, 'start_time', info.getStartTime());
  setFallback(data.format, 'size', info.getSize());
  setFallback(data.format, 'bit_rate', info.getBitrate());
  setFallback(data.format, 'tags', safe(() => info.getTags()));

  return data;
}

// True when the parser produced media information that actually carries properties.
// Must be asked BEFORE serializeMediaInformation(), which fills in streams/chapters/
// format defaults - after that an empty parse result is indistinguishable from a real
// one with no top-level members.
function hasParsedProperties(info) {
  const properties = safe(() => info.getAllProperties());
  return (
    !!properties &&
    typeof properties === 'object' &&
    Object.keys(properties).length > 0
  );
}

// nullWhenEmpty implements the difference between the two parser entry points, which
// native does not make for us - it returns a MediaInformation for any input it can
// parse, so `{}` comes back as an empty one. MediaInformationJsonParser.from() reports
// that as "no media information" (null), exactly as the Flutter plugin does with its
// `properties.length == 0` check, while fromWithError() hands the empty object back and
// reserves failure for input that does not parse at all.
function serializeParsedMediaInformation(info, { nullWhenEmpty = false } = {}) {
  if (!info) return null;
  try {
    if (nullWhenEmpty && !hasParsedProperties(info)) return null;
    return serializeMediaInformation(info);
  } finally {
    info.delete();
  }
}

// Reads a numeric session timestamp (epoch millis). The getX...Millis accessors were
// added alongside getArguments; guarding by typeof keeps an older (un-rebuilt) wasm
// module from throwing here — it just reports 0 until the module is rebuilt.
function sessionMillis(session, name) {
  const fn = session[name];
  return typeof fn === 'function' ? Number(fn.call(session)) : 0;
}

// Serializes a media-information session and attaches its parsed media info (if any).
function serializeSessionWithMedia(session) {
  const info = session.getMediaInformation();
  try {
    const result = serializeSession(session);
    if (info) {
      result.mediaInformation = serializeMediaInformation(info);
    }
    return result;
  } finally {
    if (info) {
      info.delete();
    }
  }
}

function serializeSession(session, { withStatistics = false } = {}) {
  const rc = session.getReturnCode();
  const result = {
    type: sessionType(session),
    sessionId: Number(session.getSessionId()),
    command: session.getCommand(),
    state: stateToNumber(session.getState()),
    returnCode: rc ? { value: rc.getValue() } : null,
    // Native getOutput() is getAllLogsAsString(), so only the latter is sent; the
    // public session derives getOutput() from it. Non-waiting form, see serializeLogs().
    logs: session.getLogsAsString(),
    failStackTrace: session.getFailStackTrace(),
    duration: Number(session.getDuration()),
    createTime: sessionMillis(session, 'getCreateTimeMillis'),
    startTime: sessionMillis(session, 'getStartTimeMillis'),
    endTime: sessionMillis(session, 'getEndTimeMillis'),
    logEntries: serializeLogs(session),
  };
  // OMITTED, not sent as [], when the binding is missing. AbstractSession._apply()
  // overwrites the argument array only when this key IS an array, and _applySessionMap()
  // falls back to parseArguments(command) only when it is NOT one. An empty array passes
  // both tests, so it would wipe the arguments rather than trigger the fallback the guard
  // exists for. The getArguments binding landed alongside the getX...Millis accessors;
  // sessionMillis() reports 0 for the same reason - 0 is a meaningful "unset", [] is not.
  if (typeof session.getArguments === 'function') {
    result.arguments = Array.from(session.getArguments());
  }
  if (rc) rc.delete();
  if (withStatistics) result.statistics = serializeStatistics(session);
  return result;
}

function getNativeHistorySession(sessionId) {
  if (sessionId == null) return null;

  let session = Module.FFmpegKitConfig.getFFmpegSession(sessionId);
  if (session) return session;

  session = Module.FFmpegKitConfig.getFFprobeSession(sessionId);
  if (session) return session;

  session = Module.FFmpegKitConfig.getMediaInformationSession(sessionId);
  if (session) return session;

  return null;
}

function serializeHistorySession(sessionId) {
  const session = getNativeHistorySession(sessionId);
  if (!session) return null;

  try {
    if (session.isMediaInformation()) {
      return serializeSessionWithMedia(session);
    }
    return serializeSession(session, { withStatistics: session.isFFmpeg() });
  } finally {
    session.delete();
  }
}

function serializeHistorySessions(sessionIds) {
  const sessions = [];
  for (const sessionId of Array.from(sessionIds || [])) {
    const session = serializeHistorySession(sessionId);
    if (session) sessions.push(session);
  }
  return sessions;
}

// ---- Live event draining -------------------------------------------------------

function drainAndForward(id) {
  if (!Module._ffmpegkitDrainLogEvents) return;

  const logs = Module._ffmpegkitDrainLogEvents();
  for (let i = 0; i < logs.length; i++) {
    const l = logs[i];
    postMessage({
      id,
      type: MSG_LOG,
      log: {
        sessionId: Number(l.getSessionId()),
        level: levelToNumber(l.getLevel()),
        message: l.getMessage(),
      },
    });
    l.delete();
  }

  const stats = Module._ffmpegkitDrainStatisticsEvents();
  for (let i = 0; i < stats.length; i++) {
    postMessage({ id, type: MSG_STATISTICS, statistics: statObject(stats[i]) });
    stats[i].delete();
  }
}

function sessionDone(session) {
  const state = stateToNumber(session.getState());
  return state === SESSION_STATE_COMPLETED || state === SESSION_STATE_FAILED;
}

// Events already buffered in C++ but not yet drained to this thread. Added to the
// native in-transit count so messagesInTransmit() keeps meaning "not delivered to the
// JS callbacks yet" - see config_pendingEventCount in FFmpegKitBindings.cpp. Guarded by
// typeof so an older (un-rebuilt) module reports the native count alone.
function pendingEventCount(sessionId) {
  if (typeof Module._ffmpegkitPendingEventCount !== 'function') return 0;
  return Number(Module._ffmpegkitPendingEventCount(sessionId)) || 0;
}

function messagesInTransmit(sessionId) {
  return (
    Number(Module.FFmpegKitConfig.messagesInTransmit(sessionId)) +
    pendingEventCount(sessionId)
  );
}

// The JS-thread equivalent of native
// AbstractSession::waitForAsynchronousMessagesInTransmit(): resolves once this session has
// nothing left in transmit, or once the timeout expires. Same shape as native's loop
// (AbstractSession.cpp), except it yields to the event loop between ticks instead of
// sleeping on a condition variable.
//
// Polling here rather than calling native's getAll*WithTimeout() is required, not merely
// polite, for two reasons:
//
//  1. Native's wait blocks this thread, and this is the only thread running the worker
//     event loop. While it blocks there is no cancel handling, no ioStreamWrite/
//     ioStreamRead servicing for ffkitstream: I/O, and no drainAndForward() - so a
//     getAllLogs() on a running session would freeze live log and statistics delivery for
//     the length of the wait.
//  2. It cannot even do its own job here. On web "messages in transmit" also counts the
//     events buffered in C++ that this thread has not drained yet (config_pendingEventCount),
//     and only this thread can drain them. Blocking would hold the one thread able to make
//     that count fall, so the wait would sleep out the full timeout whenever there was
//     anything to wait for. Draining on every tick is what lets it actually reach zero.
function waitForMessagesInTransmit(id, sessionId, timeout) {
  const expireTime = Date.now() + timeout;
  return new Promise((resolve, reject) => {
    const tick = () => {
      try {
        drainAndForward(id);
        if (messagesInTransmit(sessionId) <= 0 || Date.now() >= expireTime) resolve();
        else setTimeout(tick, MESSAGES_IN_TRANSMIT_POLL_INTERVAL);
      } catch (err) {
        // tick runs from a timer for every attempt after the first, outside the
        // onmessage try/catch. Reject so the awaiting handler reports the failure
        // against this request id rather than letting it escape as a worker error.
        reject(err);
      }
    };
    tick();
  });
}

// Poll a running native session to completion. Between ticks this thread returns to
// the event loop, which lets emscripten create FFmpeg's threads on demand instead
// of deadlocking on the exhausted prewarmed pool — and is the window in which we
// drain buffered events for live progress.
//
// There is deliberately no wall-clock timeout here: a legitimate transcode can run for
// hours, so any cap would kill real work. Instead, the two ways this poll can stop
// making progress both settle the request - a trapped pthread reports through onAbort
// above, and a throw from the embind calls below rejects (see the try/catch).
function waitForSession(session, id) {
  return new Promise((resolve, reject) => {
    const tick = () => {
      try {
        drainAndForward(id);
        if (sessionDone(session)) resolve();
        else setTimeout(tick, 100);
      } catch (err) {
        // tick runs from a timer, outside the onmessage try/catch that turns failures
        // into a MSG_ERROR reply. Reject so the awaiting handler reports it against
        // this request id rather than letting it escape as an uncaught worker error.
        reject(err);
      }
    };
    setTimeout(tick, 50);
  });
}

// ---- Message handling ----------------------------------------------------------

self.onmessage = async (event) => {
  const { id, op, args = {} } = event.data || {};
  if (op === undefined) return;

  try {
    switch (op) {
      case 'createSession': {
        let session;
        try {
          session = createNativeSession(args);
          const result = serializeSession(session);
          postMessage({ id, type: MSG_RESULT, result });
        } finally {
          session?.delete?.();
        }
        break;
      }
      case 'executeAsync': {
        let session;
        try {
          session = getNativeSession('ffmpeg', requireSessionId(args, 'executeAsync'));
          Module.FFmpegKitConfig.asyncFFmpegExecuteSession(session);
          await waitForSession(session, id);
          drainAndForward(id); // final flush for events just before completion
          const result = serializeSession(session, { withStatistics: true });
          postMessage({ id, type: MSG_RESULT, result });
        } finally {
          session?.delete?.();
        }
        break;
      }
      case 'execute': {
        let session;
        try {
          session = getNativeSession('ffmpeg', requireSessionId(args, 'execute'));
          Module.FFmpegKitConfig.asyncFFmpegExecuteSession(session);
          await waitForSession(session, id);
          drainAndForward(id); // final flush for events just before completion
          const result = serializeSession(session, { withStatistics: true });
          postMessage({ id, type: MSG_RESULT, result });
        } finally {
          session?.delete?.();
        }
        break;
      }
      case 'ffprobe': {
        let session;
        try {
          session = getNativeSession('ffprobe', requireSessionId(args, 'ffprobe'));
          Module.FFmpegKitConfig.asyncFFprobeExecuteSession(session);
          await waitForSession(session, id);
          drainAndForward(id);
          const result = serializeSession(session);
          postMessage({ id, type: MSG_RESULT, result });
        } finally {
          session?.delete?.();
        }
        break;
      }
      case 'ffprobeAsync': {
        let session;
        try {
          session = getNativeSession('ffprobe', requireSessionId(args, 'ffprobeAsync'));
          Module.FFmpegKitConfig.asyncFFprobeExecuteSession(session);
          await waitForSession(session, id);
          drainAndForward(id);
          const result = serializeSession(session);
          postMessage({ id, type: MSG_RESULT, result });
        } finally {
          session?.delete?.();
        }
        break;
      }
      case 'getMediaInformation': {
        let session;
        try {
          session = getNativeSession(
            'mediaInformation',
            requireSessionId(args, 'getMediaInformation')
          );
          Module.FFmpegKitConfig.asyncGetMediaInformationExecuteSession(
            session,
            args.waitTimeout != null ? args.waitTimeout : DEFAULT_WAIT_TIMEOUT
          );
          await waitForSession(session, id);
          drainAndForward(id);
          const result = serializeSessionWithMedia(session);
          postMessage({ id, type: MSG_RESULT, result });
        } finally {
          session?.delete?.();
        }
        break;
      }
      case 'getMediaInformationAsync': {
        let session;
        try {
          session = getNativeSession(
            'mediaInformation',
            requireSessionId(args, 'getMediaInformationAsync')
          );
          Module.FFmpegKitConfig.asyncGetMediaInformationExecuteSession(
            session,
            args.waitTimeout != null ? args.waitTimeout : DEFAULT_WAIT_TIMEOUT
          );
          await waitForSession(session, id);
          drainAndForward(id);
          const result = serializeSessionWithMedia(session);
          postMessage({ id, type: MSG_RESULT, result });
        } finally {
          session?.delete?.();
        }
        break;
      }
      case 'mediaInformationJsonParserFrom': {
        const info = Module.MediaInformationJsonParser.from(args.ffprobeJsonOutput || '');
        postMessage({
          id,
          type: MSG_RESULT,
          result: { media: serializeParsedMediaInformation(info, { nullWhenEmpty: true }) },
        });
        break;
      }
      case 'mediaInformationJsonParserFromWithError': {
        const info = Module.MediaInformationJsonParser.fromWithError(args.ffprobeJsonOutput || '');
        postMessage({
          id,
          type: MSG_RESULT,
          result: { media: serializeParsedMediaInformation(info) },
        });
        break;
      }
      case 'enableRedirection': {
        Module.FFmpegKitConfig.enableRedirection();
        postMessage({ id, type: MSG_RESULT, result: { ok: true } });
        break;
      }
      case 'disableRedirection': {
        Module.FFmpegKitConfig.disableRedirection();
        postMessage({ id, type: MSG_RESULT, result: { ok: true } });
        break;
      }
      case 'setSessionHistorySize': {
        Module.FFmpegKitConfig.setSessionHistorySize(args.size);
        postMessage({ id, type: MSG_RESULT, result: { ok: true } });
        break;
      }
      case 'getSessionHistorySize': {
        postMessage({
          id,
          type: MSG_RESULT,
          result: { size: Number(Module.FFmpegKitConfig.getSessionHistorySize()) },
        });
        break;
      }
      case 'getSession': {
        postMessage({
          id,
          type: MSG_RESULT,
          result: { session: serializeHistorySession(args.sessionId) },
        });
        break;
      }
      case 'getLastSession': {
        postMessage({
          id,
          type: MSG_RESULT,
          result: {
            session: serializeHistorySession(Module.FFmpegKitConfig.getLastSessionId()),
          },
        });
        break;
      }
      case 'getLastCompletedSession': {
        postMessage({
          id,
          type: MSG_RESULT,
          result: {
            session: serializeHistorySession(
              Module.FFmpegKitConfig.getLastCompletedSessionId()
            ),
          },
        });
        break;
      }
      case 'getSessionIds': {
        postMessage({
          id,
          type: MSG_RESULT,
          result: { sessionIds: Array.from(Module.FFmpegKitConfig.getSessionIds() || []) },
        });
        break;
      }
      case 'getSessions': {
        postMessage({
          id,
          type: MSG_RESULT,
          result: {
            sessions: serializeHistorySessions(Module.FFmpegKitConfig.getSessionIds()),
          },
        });
        break;
      }
      case 'getSessionsByState': {
        postMessage({
          id,
          type: MSG_RESULT,
          result: {
            sessions: serializeHistorySessions(
              Module.FFmpegKitConfig.getSessionsByStateIds(args.state)
            ),
          },
        });
        break;
      }
      case 'getFFmpegSessions': {
        postMessage({
          id,
          type: MSG_RESULT,
          result: {
            sessions: serializeHistorySessions(
              Module.FFmpegKitConfig.getFFmpegSessionIds()
            ),
          },
        });
        break;
      }
      case 'getFFprobeSessions': {
        postMessage({
          id,
          type: MSG_RESULT,
          result: {
            sessions: serializeHistorySessions(
              Module.FFmpegKitConfig.getFFprobeSessionIds()
            ),
          },
        });
        break;
      }
      case 'getMediaInformationSessions': {
        postMessage({
          id,
          type: MSG_RESULT,
          result: {
            sessions: serializeHistorySessions(
              Module.FFmpegKitConfig.getMediaInformationSessionIds()
            ),
          },
        });
        break;
      }
      case 'clearSessions': {
        Module.FFmpegKitConfig.clearSessions();
        postMessage({ id, type: MSG_RESULT, result: { ok: true } });
        break;
      }
      case 'cancel': {
        if (args.sessionId != null) Module.FFmpegKit.cancelSession(args.sessionId);
        else Module.FFmpegKit.cancel();
        postMessage({ id, type: MSG_RESULT, result: { ok: true } });
        break;
      }
      case 'writeFile': {
        mkdirParent(args.path);
        Module.FS.writeFile(args.path, new Uint8Array(args.data));
        postMessage({ id, type: MSG_RESULT, result: { ok: true } });
        break;
      }
      case 'readFile': {
        const data = readOutput(args.path);
        postMessage(
          { id, type: MSG_RESULT, result: { data } },
          data ? [data.buffer] : []
        );
        break;
      }
      case 'getArch': {
        postMessage({
          id,
          type: MSG_RESULT,
          result: { arch: Module.ArchDetect.getArch() },
        });
        break;
      }
      case 'getPackageName': {
        postMessage({
          id,
          type: MSG_RESULT,
          result: { packageName: Module.Packages.getPackageName() },
        });
        break;
      }
      case 'getExternalLibraries': {
        const externalLibraries = Module.Packages.getExternalLibraries();
        postMessage({
          id,
          type: MSG_RESULT,
          result: {
            externalLibraries: Array.isArray(externalLibraries)
              ? externalLibraries
              : Array.from(externalLibraries || []),
          },
        });
        break;
      }
      case 'setLogLevel': {
        // The binding takes a plain int (see levelToNumber above); it must never be
        // handed an embind Level value object.
        const level = Number(args.level);
        if (Number.isFinite(level)) Module.FFmpegKitConfig.setLogLevel(level);
        // Report the level native actually holds now, so the host cache can never
        // drift away from it.
        postMessage({
          id,
          type: MSG_RESULT,
          result: { ok: true, level: currentLogLevel() },
        });
        break;
      }
      case 'setEnvironmentVariable': {
        const returnCode = Module.FFmpegKitConfig.setEnvironmentVariable(
          args.variableName ?? '',
          args.variableValue ?? ''
        );
        postMessage({ id, type: MSG_RESULT, result: { returnCode } });
        break;
      }
      case 'deleteSession': {
        if (args.sessionId != null) Module.FFmpegKitConfig.deleteSession(args.sessionId);
        postMessage({ id, type: MSG_RESULT, result: { ok: true } });
        break;
      }
      case 'messagesInTransmit': {
        const count = args.sessionId != null ? messagesInTransmit(args.sessionId) : 0;
        postMessage({ id, type: MSG_RESULT, result: { count } });
        break;
      }
      // The getAll* family: wait for this session's messages in transmit (up to
      // waitTimeout, native default 5000 ms), then return the delivered record - the same
      // contract Flutter and React Native get from their platform channels. The wait runs
      // on this thread as a poll rather than inside native, so the worker keeps draining
      // events and servicing cancel/stream I/O while it waits; see
      // waitForMessagesInTransmit and withHistorySession.
      case 'getAllLogs': {
        await withHistorySession(id, args, op, (session) => ({
          logEntries: serializeLogList(session.getLogs()),
        }));
        break;
      }
      case 'getAllLogsAsString': {
        await withHistorySession(id, args, op, (session) => ({
          logs: session.getLogsAsString(),
        }));
        break;
      }
      case 'getAllStatistics': {
        await withHistorySession(id, args, op, (session) => ({
          statistics: session.isFFmpeg()
            ? serializeStatisticsList(session.getStatistics())
            : [],
        }));
        break;
      }
      case 'setFontDirectoryList': {
        setFontDirectoryList(args);
        postMessage({ id, type: MSG_RESULT, result: { ok: true } });
        break;
      }
      case 'setFontconfigConfigurationPath': {
        setFontconfigConfigurationPath(args);
        postMessage({ id, type: MSG_RESULT, result: { ok: true } });
        break;
      }
      case 'mount': {
        // WORKERFS mounts File/Blob inputs read-only — no copy into the wasm heap.
        mkdirTree(args.mountPoint);
        Module.FS.mount(
          Module.FS.filesystems.WORKERFS,
          { files: args.files || [], blobs: args.blobs || [] },
          args.mountPoint
        );
        postMessage({ id, type: MSG_RESULT, result: { ok: true } });
        break;
      }
      case 'ioCreate': {
        const obj = createIoObject(args);
        const handle = ++ioSeq;
        ioObjects.set(handle, obj);
        postMessage({ id, type: MSG_RESULT, result: { handle, url: obj.getUrl() } });
        break;
      }
      case 'ioGetSize': {
        const size = Number(requireIoObject(args.handle).getSize());
        postMessage({ id, type: MSG_RESULT, result: { size } });
        break;
      }
      case 'ioOutputBytes': {
        const data = requireIoObject(args.handle).toByteArray(); // Uint8Array
        postMessage({ id, type: MSG_RESULT, result: { data } }, data ? [data.buffer] : []);
        break;
      }
      case 'ioStreamWrite': {
        // timeoutMs 0 = non-blocking; blocking here would stall the worker event loop.
        const written = requireIoObject(args.handle).write(new Uint8Array(args.data), 0);
        postMessage({ id, type: MSG_RESULT, result: { written } });
        break;
      }
      case 'ioStreamCloseInput': {
        requireIoObject(args.handle).closeInput();
        postMessage({ id, type: MSG_RESULT, result: { ok: true } });
        break;
      }
      case 'ioStreamRead': {
        // Uint8Array | null (nothing ready yet)
        const data = requireIoObject(args.handle).read(args.maxBytes, 0);
        postMessage({ id, type: MSG_RESULT, result: { data } }, data ? [data.buffer] : []);
        break;
      }
      // Idempotent on purpose: closing twice, or closing a handle from a terminated
      // runtime, must not throw.
      case 'ioClose': {
        const obj = ioObjects.get(args.handle);
        if (obj) {
          obj.close();
          obj.delete();
          ioObjects.delete(args.handle);
        }
        postMessage({ id, type: MSG_RESULT, result: { ok: true } });
        break;
      }
      default:
        postMessage({ id, type: MSG_ERROR, message: 'Unknown op: ' + op });
    }
  } catch (err) {
    postMessage({ id, type: MSG_ERROR, message: errMessage(err) });
  }
};

function readOutput(path) {
  try {
    return Module.FS.readFile(path); // Uint8Array
  } catch {
    return null;
  }
}

function errMessage(err) {
  if (err && err.message) return err.message;
  if (typeof err === 'number' && Module) {
    try {
      return Module.getExceptionMessage ? Module.getExceptionMessage(err) : 'error code ' + err;
    } catch {
      return 'error code ' + err;
    }
  }
  return String(err);
}

function safe(fn) {
  try {
    return fn();
  } catch {
    return null;
  }
}

workerInit();
