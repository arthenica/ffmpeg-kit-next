// Internal conduit between the native-named public classes and the wasm module.
// This is the web analog of the platform channel used on the mobile plugins (and
// is named after Flutter's internal FFmpegKitFactory): it owns the Worker, the
// request/response protocol, the global + per-session callback registries, and the
// reconstruction of public session/value objects from the worker's serialized
// data. App code never imports this — it talks to FFmpegKit/FFprobeKit/
// FFmpegKitConfig, which delegate here. The raw embind Module lives only in the
// worker and never crosses this boundary.

import { SessionState } from './enums.js';
import { Log, Statistics } from './models.js';
import {
  FFmpegSession,
  FFprobeSession,
  MediaInformationSession,
} from './session.js';

class FFmpegKitFactory {
  constructor() {
    this._worker = new Worker(new URL('./ffmpegkit.worker.js', import.meta.url), {
      type: 'module',
    });
    this._seq = 0;
    this._pending = new Map();
    this._logCallback = null; // global
    this._statisticsCallback = null; // global
    this._info = null; // { version, ffmpegVersion, buildDate } once ready

    // Session history registry (mirrors Flutter's Dart-side FFmpegKitFactory):
    // the JS registry is the single source of truth for the public session-history
    // API. The wasm module keeps its own bounded C++ history, but we never read it.
    this._sessionHistory = []; // in creation order
    this._sessionsById = new Map(); // sessionId -> session (once the id is known)
    this._sessionHistorySize = 10; // matches the native default
    this._lastCompletedSession = null;

    this._ready = new Promise((resolve, reject) => {
      this._resolveReady = resolve;
      this._rejectReady = reject;
    });

    this._worker.onmessage = (event) => this._onMessage(event.data);
    this._worker.onerror = (event) => {
      const err = new Error(event.message || 'worker error');
      this._rejectReady(err);
      for (const { reject } of this._pending.values()) reject(err);
      this._pending.clear();
    };
  }

  ready() {
    return this._ready;
  }

  get info() {
    return this._info;
  }

  // ---- Global callback registry (FFmpegKitConfig.enableLog/StatisticsCallback) --
  setLogCallback(cb) {
    this._logCallback = cb || null;
  }
  setStatisticsCallback(cb) {
    this._statisticsCallback = cb || null;
  }

  // ---- Execution entry points (called by the public classes) --------------------
  executeFFmpegAsync(command, logCallback, statisticsCallback) {
    const session = new FFmpegSession(command, logCallback, statisticsCallback);
    return this._run('executeAsync', { command }, session);
  }
  executeFFmpeg(command, logCallback, statisticsCallback) {
    const session = new FFmpegSession(command, logCallback, statisticsCallback);
    return this._run('execute', { command }, session);
  }
  executeFFprobe(command) {
    const session = new FFprobeSession(command, null);
    return this._run('ffprobe', { command }, session);
  }
  getMediaInformation(path) {
    const session = new MediaInformationSession(`-i ${path}`);
    return this._run('getMediaInformation', { path }, session);
  }
  cancel(sessionId = null) {
    return this._call('cancel', { sessionId });
  }

  // ---- FFmpegKitConfig helpers --------------------------------------------------
  setLogLevel(level) {
    return this._call('setLogLevel', { level });
  }
  async setFontconfigConfigurationPath(path) {
    await this._call('setFontconfigConfigurationPath', { path });
  }
  async setFontDirectory(fontDirectoryPath, fontNameMapping = {}) {
    await this.setFontDirectoryList([fontDirectoryPath], fontNameMapping);
  }
  async setFontDirectoryList(fontDirectoryList, fontNameMapping = {}) {
    await this._call('setFontDirectoryList', {
      fontDirectoryList,
      fontNameMapping,
    });
  }
  getVersion() {
    return this._info?.version ?? null;
  }
  getFFmpegVersion() {
    return this._info?.ffmpegVersion ?? null;
  }
  getBuildDate() {
    return this._info?.buildDate ?? null;
  }

  // ---- Session history (FFmpegKitConfig.*, FFmpegKit/FFprobeKit.list*) ----------
  getSessions() {
    return [...this._sessionHistory];
  }
  getSession(sessionId) {
    return this._sessionsById.get(sessionId) ?? null;
  }
  getLastSession() {
    return this._sessionHistory[this._sessionHistory.length - 1] ?? null;
  }
  getLastCompletedSession() {
    return this._lastCompletedSession;
  }
  getSessionsByState(state) {
    return this._sessionHistory.filter((s) => s.getState() === state);
  }
  getSessionHistorySize() {
    return this._sessionHistorySize;
  }
  setSessionHistorySize(size) {
    this._sessionHistorySize = size;
    this._trimHistory();
  }
  clearSessions() {
    this._sessionHistory = [];
    this._sessionsById.clear();
    this._lastCompletedSession = null;
  }
  listFFmpegSessions() {
    return this._sessionHistory.filter((s) => s.isFFmpeg());
  }
  listFFprobeSessions() {
    return this._sessionHistory.filter((s) => s.isFFprobe());
  }

  // ---- Web-only virtual filesystem I/O (MEMFS lives inside the worker) ----------
  writeFile(path, data) {
    return this._call(
      'writeFile',
      { path, data },
      data instanceof Uint8Array ? [data.buffer] : []
    );
  }
  async readFile(path) {
    const msg = await this._call('readFile', { path });
    return msg.result ? msg.result.data : null;
  }

  /**
   * Mounts files/blobs read-only via WORKERFS at `mountPoint`, so FFmpeg reads them
   * by path without copying into the wasm heap. `files` are File objects; `blobs`
   * are `{ name, data: Blob }`. Available only because the module links WORKERFS.
   */
  mount(mountPoint, { files = [], blobs = [] } = {}) {
    return this._call('mount', { mountPoint, files, blobs });
  }

  // ---- ffkitmem:/ffkitstream: I/O objects (live in the worker; handled by id) ----
  async ioCreate(kind, params = {}) {
    const transfer = params.data instanceof Uint8Array ? [params.data.buffer] : [];
    const msg = await this._call('ioCreate', { kind, ...params }, transfer);
    return msg.result; // { handle, url }
  }
  async ioOutputBytes(handle) {
    const msg = await this._call('ioOutputBytes', { handle });
    return msg.result ? msg.result.data : null;
  }
  async ioStreamWrite(handle, data) {
    const transfer = data instanceof Uint8Array ? [data.buffer] : [];
    const msg = await this._call('ioStreamWrite', { handle, data }, transfer);
    return msg.result ? msg.result.written : 0;
  }
  ioStreamCloseInput(handle) {
    return this._call('ioStreamCloseInput', { handle });
  }
  async ioStreamRead(handle, maxBytes) {
    const msg = await this._call('ioStreamRead', { handle, maxBytes });
    return msg.result ? msg.result.data : null;
  }
  ioClose(handle) {
    return this._call('ioClose', { handle });
  }

  // ---- Internals ----------------------------------------------------------------

  // Runs an execute/probe op whose reply carries a serialized session. Resolves the
  // returned promise with the populated public session object.
  _run(op, args, session) {
    const id = ++this._seq;
    this._registerSession(session);
    return new Promise((resolve, reject) => {
      this._pending.set(id, { resolve, reject, session });
      this._worker.postMessage({ id, op, args });
    });
  }

  // Adds a freshly created session to the history (id not yet known), trimming the
  // oldest beyond the configured size — mirroring the native sessionHistoryList.
  _registerSession(session) {
    this._sessionHistory.push(session);
    this._trimHistory();
  }

  // Indexes a session by id once the worker has reported it.
  _indexSession(session) {
    const id = session.getSessionId();
    if (id != null) this._sessionsById.set(id, session);
  }

  _trimHistory() {
    while (this._sessionHistory.length > this._sessionHistorySize) {
      const removed = this._sessionHistory.shift();
      const id = removed?.getSessionId();
      if (id != null) this._sessionsById.delete(id);
    }
  }

  // Runs a plain op whose reply is a raw result payload.
  _call(op, args, transfer = []) {
    const id = ++this._seq;
    return new Promise((resolve, reject) => {
      this._pending.set(id, { resolve, reject });
      this._worker.postMessage({ id, op, args }, transfer);
    });
  }

  _onMessage(msg) {
    if (msg.type === 'ready') {
      this._info = {
        version: msg.version,
        ffmpegVersion: msg.ffmpegVersion,
        buildDate: msg.buildDate,
      };
      this._resolveReady(this._info);
      return;
    }
    if (msg.type === 'fatal') {
      this._rejectReady(new Error(msg.message));
      return;
    }

    const pending = this._pending.get(msg.id);
    if (!pending) return;

    // Live events: route to the session, its per-call callback, and the global one.
    if (msg.type === 'log') {
      const log = new Log(msg.log.sessionId, msg.log.level, msg.log.message);
      pending.session?._addLog(log);
      pending.session?.getLogCallback()?.(log);
      this._logCallback?.(log);
      return;
    }
    if (msg.type === 'statistics') {
      const statistics = new Statistics(msg.statistics);
      pending.session?._addStatistics(statistics);
      pending.session?.getStatisticsCallback?.()?.(statistics);
      this._statisticsCallback?.(statistics);
      return;
    }
    // The worker reports the session id as soon as the run starts, so getSession(id)
    // works while an async session is still running (matching native, where the id
    // is assigned at creation).
    if (msg.type === 'started') {
      if (pending.session) {
        pending.session._sessionId = msg.sessionId;
        this._indexSession(pending.session);
      }
      return;
    }

    // Terminal messages.
    this._pending.delete(msg.id);
    if (msg.type === 'error') {
      pending.reject(new Error(msg.message));
    } else if (pending.session) {
      pending.session._apply(msg.result);
      this._indexSession(pending.session);
      if (pending.session.getState() === SessionState.COMPLETED) {
        this._lastCompletedSession = pending.session;
      }
      pending.resolve(pending.session);
    } else {
      pending.resolve(msg);
    }
  }
}

// Single shared instance — created lazily so importing a value type doesn't spin
// up a Worker. All public classes route through this.
let _instance = null;
export function getFactory() {
  if (_instance === null) _instance = new FFmpegKitFactory();
  return _instance;
}
