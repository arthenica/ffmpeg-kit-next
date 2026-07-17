// FFmpegKitWorker — the worker-thread host for the FFmpegKitNext wasm module.
//
// It is the ONLY place that imports libffmpegkit.js and holds the raw embind
// Module, and the ONLY caller of the internal event-bridge free functions
// (_ffmpegkitEnableEventBuffering / _ffmpegkitDrainLogEvents /
// _ffmpegkitDrainStatisticsEvents). It runs execute() off the UI thread, serializes
// sessions into plain data for FFmpegKitFactory, and forwards live log/statistics
// events drained from the buffer while executeAsync runs on its own pthread.

import createFFmpegKitModule from '../lib/libffmpegkit.js';

let Module = null;
let LEVELS = null; // numeric Level -> embind enum value

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

// Builds the worker-side embind I/O object for an ioCreate request.
function createIoObject(args) {
  const ext = args.extension || '';
  switch (args.kind) {
    case 'inputBuffer':
      return Module.FFmpegKitInputBuffer.fromByteArray(new Uint8Array(args.data), ext);
    case 'outputBuffer':
      return args.maxCapacity != null
        ? Module.FFmpegKitOutputBuffer.createWithCapacity(ext, args.initialCapacity, args.maxCapacity)
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

async function init() {
  try {
    Module = await createFFmpegKitModule({
      locateFile: (path) => new URL('../lib/' + path, import.meta.url).href,
    });
  } catch (err) {
    postMessage({ type: 'fatal', message: 'Failed to instantiate module: ' + errMessage(err) });
    return;
  }

  if (!Module.FFmpegKit || !Module.FFprobeKit || !Module.FFmpegKitConfig) {
    postMessage({
      type: 'fatal',
      message:
        'FFmpegKit bindings are not present on the module. The embind ' +
        'registration was likely stripped by MAIN_MODULE=2 DCE (check the anchor).',
    });
    return;
  }

  // Install the C++ log/statistics buffering before any execute, so no early logs
  // are missed. We drain that buffer from this thread; see drainAndForward.
  safe(() => Module._ffmpegkitEnableEventBuffering());

  // Fixed numeric-Level -> embind-enum table (setLogLevel takes the enum value).
  const L = Module.Level;
  LEVELS = {
    '-16': L.AVLogStdErr, '-8': L.AVLogQuiet, '0': L.AVLogPanic, '8': L.AVLogFatal,
    '16': L.AVLogError, '24': L.AVLogWarning, '32': L.AVLogInfo, '40': L.AVLogVerbose,
    '48': L.AVLogDebug, '56': L.AVLogTrace,
  };

  postMessage({
    type: 'ready',
    version: safe(() => Module.FFmpegKitConfig.getVersion()),
    ffmpegVersion: safe(() => Module.FFmpegKitConfig.getFFmpegVersion()),
    buildDate: safe(() => Module.FFmpegKitConfig.getBuildDate()),
  });
}

// ---- Serialization: embind objects -> plain data for postMessage ---------------

function stateToNumber(state) {
  const S = Module.SessionState;
  if (state === S.Completed) return 3;
  if (state === S.Failed) return 2;
  if (state === S.Running) return 1;
  return 0;
}

function serializeLogs(session) {
  const out = [];
  const arr = session.getLogs();
  for (let i = 0; i < arr.length; i++) {
    const l = arr[i];
    out.push({ sessionId: Number(l.getSessionId()), level: l.getLevel(), message: l.getMessage() });
    l.delete();
  }
  return out;
}

function serializeStatistics(session) {
  const out = [];
  const arr = session.getStatistics();
  for (let i = 0; i < arr.length; i++) {
    out.push(statObject(arr[i]));
    arr[i].delete();
  }
  return out;
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

function serializeMediaInformation(info) {
  const streams = [];
  const arr = info.getStreams();
  for (let i = 0; i < arr.length; i++) {
    const s = arr[i];
    streams.push({
      index: s.getIndex(),
      type: s.getType(),
      codec: s.getCodec(),
      codecLong: s.getCodecLong(),
      format: s.getFormat(),
      width: s.getWidth(),
      height: s.getHeight(),
      bitrate: s.getBitrate(),
      sampleRate: s.getSampleRate(),
      channelLayout: s.getChannelLayout(),
    });
    s.delete();
  }
  return {
    filename: info.getFilename(),
    format: info.getFormat(),
    longFormat: info.getLongFormat(),
    duration: info.getDuration(),
    startTime: info.getStartTime(),
    size: info.getSize(),
    bitrate: info.getBitrate(),
    streams,
  };
}

function serializeSession(session, { withStatistics = false } = {}) {
  const rc = session.getReturnCode();
  const result = {
    sessionId: Number(session.getSessionId()),
    command: session.getCommand(),
    state: stateToNumber(session.getState()),
    returnCode: rc ? { value: rc.getValue() } : null,
    output: session.getOutput(),
    logs: session.getAllLogsAsString(),
    failStackTrace: session.getFailStackTrace(),
    duration: Number(session.getDuration()),
    logEntries: serializeLogs(session),
  };
  if (rc) rc.delete();
  if (withStatistics) result.statistics = serializeStatistics(session);
  return result;
}

// ---- Live event draining -------------------------------------------------------

function drainAndForward(id) {
  if (!Module._ffmpegkitDrainLogEvents) return;

  const logs = Module._ffmpegkitDrainLogEvents();
  for (let i = 0; i < logs.length; i++) {
    const l = logs[i];
    postMessage({
      id,
      type: 'log',
      log: { sessionId: Number(l.getSessionId()), level: l.getLevel(), message: l.getMessage() },
    });
    l.delete();
  }

  const stats = Module._ffmpegkitDrainStatisticsEvents();
  for (let i = 0; i < stats.length; i++) {
    postMessage({ id, type: 'statistics', statistics: statObject(stats[i]) });
    stats[i].delete();
  }
}

function sessionDone(session) {
  const s = session.getState();
  return s === Module.SessionState.Completed || s === Module.SessionState.Failed;
}

// Poll a running async session to completion. Between ticks this thread returns to
// the event loop, which lets emscripten create FFmpeg's threads on demand instead
// of deadlocking on the exhausted prewarmed pool — and is the window in which we
// drain buffered events for live progress.
function waitForSession(session, id) {
  return new Promise((resolve) => {
    const tick = () => {
      drainAndForward(id);
      if (sessionDone(session)) resolve();
      else setTimeout(tick, 100);
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
      case 'executeAsync': {
        const session = Module.FFmpegKit.executeAsync(args.command);
        // Report the id immediately so getSession(id) works while it runs.
        postMessage({ id, type: 'started', sessionId: Number(session.getSessionId()) });
        await waitForSession(session, id);
        drainAndForward(id); // final flush for events just before completion
        const result = serializeSession(session, { withStatistics: true });
        session.delete();
        postMessage({ id, type: 'result', result });
        break;
      }
      case 'execute': {
        const session = Module.FFmpegKit.execute(args.command);
        drainAndForward(id); // sync run blocked this thread; flush post-hoc
        const result = serializeSession(session, { withStatistics: true });
        session.delete();
        postMessage({ id, type: 'result', result });
        break;
      }
      case 'ffprobe': {
        const session = Module.FFprobeKit.execute(args.command);
        drainAndForward(id); // clear buffered logs so they don't leak into later ops
        const result = serializeSession(session);
        session.delete();
        postMessage({ id, type: 'result', result });
        break;
      }
      case 'getMediaInformation': {
        const session = Module.FFprobeKit.getMediaInformation(args.path);
        drainAndForward(id);
        const info = session.getMediaInformation();
        const result = serializeSession(session);
        if (info) {
          result.media = serializeMediaInformation(info);
          info.delete();
        }
        session.delete();
        postMessage({ id, type: 'result', result });
        break;
      }
      case 'cancel': {
        if (args.sessionId != null) Module.FFmpegKit.cancelSession(args.sessionId);
        else Module.FFmpegKit.cancel();
        postMessage({ id, type: 'result', result: { ok: true } });
        break;
      }
      case 'writeFile': {
        mkdirParent(args.path);
        Module.FS.writeFile(args.path, new Uint8Array(args.data));
        postMessage({ id, type: 'result', result: { ok: true } });
        break;
      }
      case 'readFile': {
        const data = readOutput(args.path);
        postMessage(
          { id, type: 'result', result: { data } },
          data ? [data.buffer] : []
        );
        break;
      }
      case 'setLogLevel': {
        const level = LEVELS[String(args.level)];
        if (level !== undefined) Module.FFmpegKitConfig.setLogLevel(level);
        postMessage({ id, type: 'result', result: { ok: true } });
        break;
      }
      case 'setFontDirectoryList': {
        setFontDirectoryList(args);
        postMessage({ id, type: 'result', result: { ok: true } });
        break;
      }
      case 'setFontconfigConfigurationPath': {
        setFontconfigConfigurationPath(args);
        postMessage({ id, type: 'result', result: { ok: true } });
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
        postMessage({ id, type: 'result', result: { ok: true } });
        break;
      }
      case 'ioCreate': {
        const obj = createIoObject(args);
        const handle = ++ioSeq;
        ioObjects.set(handle, obj);
        postMessage({ id, type: 'result', result: { handle, url: obj.getUrl() } });
        break;
      }
      case 'ioOutputBytes': {
        const obj = ioObjects.get(args.handle);
        const data = obj ? obj.toByteArray() : null; // Uint8Array
        postMessage({ id, type: 'result', result: { data } }, data ? [data.buffer] : []);
        break;
      }
      case 'ioStreamWrite': {
        const obj = ioObjects.get(args.handle);
        // timeoutMs 0 = non-blocking; blocking here would stall the worker event loop.
        const written = obj ? obj.write(new Uint8Array(args.data), 0) : 0;
        postMessage({ id, type: 'result', result: { written } });
        break;
      }
      case 'ioStreamCloseInput': {
        const obj = ioObjects.get(args.handle);
        if (obj) obj.closeInput();
        postMessage({ id, type: 'result', result: { ok: true } });
        break;
      }
      case 'ioStreamRead': {
        const obj = ioObjects.get(args.handle);
        const data = obj ? obj.read(args.maxBytes, 0) : null; // Uint8Array | null (timeout)
        postMessage({ id, type: 'result', result: { data } }, data ? [data.buffer] : []);
        break;
      }
      case 'ioClose': {
        const obj = ioObjects.get(args.handle);
        if (obj) {
          obj.close();
          obj.delete();
          ioObjects.delete(args.handle);
        }
        postMessage({ id, type: 'result', result: { ok: true } });
        break;
      }
      default:
        postMessage({ id, type: 'error', message: 'Unknown op: ' + op });
    }
  } catch (err) {
    postMessage({ id, type: 'error', message: errMessage(err) });
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

init();
