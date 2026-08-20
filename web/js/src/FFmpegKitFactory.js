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

// Internal conduit between the native-named public classes and the wasm module.
// This is the web analog of the platform channel used on the mobile plugins (and
// is named after Flutter's internal FFmpegKitFactory): it owns the Worker, the
// request/response protocol, the global + per-session callback registries, and the
// reconstruction of public session/value objects from the worker's serialized
// data. App code never imports this — it talks to FFmpegKit/FFprobeKit/
// FFmpegKitConfig, which delegate here. The raw embind Module lives only in the
// worker and never crosses this boundary.
//
// Dependency rule: this module imports only leaf modules and SessionRegistry. It
// must NEVER import a session class — the session classes call getFactory(), so a
// direct import would close a cycle. Sessions are instantiated through
// createSession(), which resolves the constructor at call time.

import {
    DEFAULT_LOG_REDIRECTION_STRATEGY,
    LogRedirectionStrategy,
    SessionState,
    SessionType,
} from './Constants.js';
import {Level} from './Level.js';
import {Log} from './Log.js';
import {createSession} from './SessionRegistry.js';
import {Statistics} from './Statistics.js';

const MSG_BOOT = 0;
const MSG_FATAL = 1;
const MSG_RESULT = 2;
const MSG_ERROR = 3;
const MSG_LOG = 4;
const MSG_STATISTICS = 5;
const MSG_SESSION_DELETED = 6;

function copyUint8Array(data) {
    return data instanceof Uint8Array ? new Uint8Array(data) : data;
}

function transferForUint8Array(data) {
    return data instanceof Uint8Array ? [data.buffer] : [];
}

// Session-map fields that are sourced from the live MSG_LOG / MSG_STATISTICS event
// stream while an execution is in flight. See _mapToSession for why they are dropped
// when a history snapshot is merged into a session that is still executing.
const LIVE_EVENT_FIELDS = ['logs', 'logEntries', 'statistics'];

// Accepted values for getSessionsByState(). Frozen SessionState is an object literal, so
// this stays a plain numeric list.
const SESSION_STATES = Object.freeze(Object.values(SessionState));

function withoutLiveEventFields(sessionMap) {
    const merged = {...sessionMap};
    for (const field of LIVE_EVENT_FIELDS) delete merged[field];
    return merged;
}

// Maps the worker's serialized session-type discriminator onto a SessionType,
// defaulting to FFMPEG exactly like the Flutter/React Native factories.
function sessionTypeFromMap(type) {
    switch (type) {
        case SessionType.FFPROBE:
            return SessionType.FFPROBE;
        case SessionType.MEDIA_INFORMATION:
            return SessionType.MEDIA_INFORMATION;
        case SessionType.FFMPEG:
        default:
            return SessionType.FFMPEG;
    }
}

class FFmpegKitFactory {
    constructor() {
        this._worker = null;
        this._seq = 0;
        this._pending = new Map();
        this._logCallback = null; // global
        this._statisticsCallback = null; // global
        this._ffmpegSessionCompleteCallback = null;
        this._ffprobeSessionCompleteCallback = null;
        this._mediaInformationSessionCompleteCallback = null;
        this._logsEnabled = true;
        this._statisticsEnabled = true;
        this._activeLogLevel = Level.AV_LOG_TRACE;
        this._logLevelConfigured = false;
        // Global default log redirection strategy (matches the native default); sessions
        // inherit it unless they set their own.
        this._logRedirectionStrategy = DEFAULT_LOG_REDIRECTION_STRATEGY;
        this._info = null; // { version, ffmpegVersion, buildDate } once initialized

        // Live-session routing registry. Public history methods query the wasm
        // module's native FFmpegKitConfig history and reconstruct wrapper objects.
        this._sessionsById = new Map(); // sessionId -> live JS session
        this._logRedirectionStrategiesById = new Map(); // sessionId -> explicit strategy

        // Per-session callbacks, keyed by native session id rather than stored on the
        // session object - the web analog of Flutter's logCallbackMap /
        // statisticsCallbackMap / *CompleteCallbackMap. Keying by id is what makes a
        // session rebuilt from the native history report the callbacks registered when it
        // was created, and what lets deleteSession()/clearSessions() drop them the way
        // FFmpegKitFactory.deleteSession() does on the other platforms. Flutter keeps three
        // separate complete-callback maps, one per session type; one map is equivalent here
        // because a session id identifies exactly one session, and the session class
        // decides which callback type it exposes.
        this._logCallbacksById = new Map();
        this._statisticsCallbacksById = new Map();
        this._completeCallbacksById = new Map();

        this._initPromise = null;
        this._resolveInit = null;
        this._rejectInit = null;
    }

    initialize(printLoadConfirmation = true) {
        if (this._initPromise) return this._initPromise;

        this._initPromise = new Promise((resolve, reject) => {
            this._resolveInit = resolve;
            this._rejectInit = reject;
        });
        const initPromise = this._initPromise;

        // Attached before anything below can fail, so a rejection always has a handler
        // and a failed attempt is cleared rather than cached - the next call then starts
        // a fresh worker instead of awaiting the one that died.
        initPromise.catch(() => {
            if (this._initPromise === initPromise) this._teardownWorker();
        });

        try {
            // The worker stays at the package root next to index.js, while this module
            // lives in src/ — hence the parent-relative specifier.
            const workerUrl = new URL('../ffmpegkit.worker.js', import.meta.url);
            workerUrl.searchParams.set(
                'printLoadConfirmation',
                printLoadConfirmation ? '1' : '0'
            );
            const worker = new Worker(workerUrl, {
                type: 'module',
            });
            this._worker = worker;

            worker.onmessage = (event) => {
                if (this._worker === worker) this._onMessage(event.data);
            };
            worker.onerror = (event) => {
                if (this._worker !== worker) return;
                const err = new Error(event.message || 'worker error');
                this._failRuntime(err);
            };
        } catch (e) {
            // A worker that cannot even be constructed - no Worker global, a Content
            // Security Policy that blocks the worker URL, module workers unsupported -
            // has to reject this attempt. Leaving _initPromise pending would wedge every
            // later call on a promise that can never settle.
            this._rejectInit?.(e);
        }

        return initPromise;
    }

    // Detaches and terminates the current Worker, if any, and clears the init state.
    // Every path that abandons a worker must go through here - a worker that failed is
    // still a live thread holding the wasm heap until it is terminated.
    _teardownWorker() {
        const worker = this._worker;
        if (worker) {
            worker.onmessage = null;
            worker.onerror = null;
            worker.terminate();
        }
        this._worker = null;
        this._initPromise = null;
        this._resolveInit = null;
        this._rejectInit = null;
    }

    // Abandons the current runtime: fails every in-flight request, terminates the worker
    // and drops all runtime-owned state. EVERY path that abandons a runtime comes through
    // here - uninit(), a worker that crashes (MSG_FATAL from onAbort) and a worker that
    // errors (onerror) - because they all leave the same wreckage behind.
    //
    // The complete callbacks run first, while _sessionsById can still resolve them; only
    // then is the per-session state dropped. It has to be dropped: the next worker starts
    // a fresh wasm module whose session ids restart from the beginning, so a stale entry -
    // a callback, or a log redirection strategy - would be handed to an unrelated session
    // that happens to be assigned the same id.
    _failRuntime(err) {
        this._rejectInit?.(err);
        for (const pending of this._pending.values()) {
            this._failPending(pending, err);
        }
        this._pending.clear();
        this._teardownWorker();
        this._forgetAllSessions();
        this._info = null;
        this._seq = 0;
    }

    // Mirrors native AbstractSession::fail(): records the stack trace, moves the
    // session to FAILED and stamps the end time, leaving the return code unset. The
    // duration follows from start/end exactly as native derives it, so a session that
    // failed no longer reports the end time or duration of an earlier run.
    _failSession(session, err) {
        session._state = SessionState.FAILED;
        session._failStackTrace = err?.message || String(err);
        session._returnCode = null;
        session._endTime = Date.now();
        session._duration =
            session._startTime > 0 ? Math.max(0, session._endTime - session._startTime) : 0;
    }

    _failPending(pending, err) {
        if (pending.session) {
            this._failSession(pending.session, err);
            if (pending.settled) {
                if (!pending.deleted) this._dispatchComplete(pending.session);
                return;
            }
        }

        pending.reject(err);
    }

    // Deliberate teardown of the runtime. Identical to losing it involuntarily, so this
    // adds nothing of its own - see _failRuntime for what is dropped and why.
    async uninit() {
        this._failRuntime(new Error('FFmpegKit web runtime was uninitialized.'));
    }

    // ---- Global callback registry (FFmpegKitConfig.enableLog/StatisticsCallback) --
    setLogCallback(cb) {
        this._logCallback = cb || null;
    }

    setStatisticsCallback(cb) {
        this._statisticsCallback = cb || null;
    }

    setFFmpegSessionCompleteCallback(cb) {
        this._ffmpegSessionCompleteCallback = cb || null;
    }

    getFFmpegSessionCompleteCallback() {
        return this._ffmpegSessionCompleteCallback;
    }

    setFFprobeSessionCompleteCallback(cb) {
        this._ffprobeSessionCompleteCallback = cb || null;
    }

    getFFprobeSessionCompleteCallback() {
        return this._ffprobeSessionCompleteCallback;
    }

    setMediaInformationSessionCompleteCallback(cb) {
        this._mediaInformationSessionCompleteCallback = cb || null;
    }

    getMediaInformationSessionCompleteCallback() {
        return this._mediaInformationSessionCompleteCallback;
    }

    getLogRedirectionStrategy() {
        return this._logRedirectionStrategy;
    }

    setLogRedirectionStrategy(strategy) {
        this._logRedirectionStrategy = strategy;
    }

    // Whether a worker is currently running. Lets the session getters answer from the
    // record they already hold rather than starting a runtime - see _getAll.
    hasRuntime() {
        return this._worker != null;
    }

    // ---- Per-session callback registry (FFmpeg/FFprobe/MediaInformationSession.create) --
    // Registered by the session create() statics and read back by the session getters, so
    // the registry - not the session object - is the source of truth while a session id
    // exists. Anything omitted or passed as a non-function clears that slot.
    setSessionCallbacks(sessionId, {completeCallback, logCallback, statisticsCallback} = {}) {
        const id = Number(sessionId);
        if (!Number.isFinite(id)) return;

        const record = (registry, callback) => {
            if (typeof callback === 'function') registry.set(id, callback);
            else registry.delete(id);
        };
        record(this._completeCallbacksById, completeCallback);
        record(this._logCallbacksById, logCallback);
        record(this._statisticsCallbacksById, statisticsCallback);
    }

    getSessionLogCallback(sessionId) {
        return this._sessionCallback(this._logCallbacksById, sessionId);
    }

    getSessionStatisticsCallback(sessionId) {
        return this._sessionCallback(this._statisticsCallbacksById, sessionId);
    }

    getSessionCompleteCallback(sessionId) {
        return this._sessionCallback(this._completeCallbacksById, sessionId);
    }

    _sessionCallback(registry, sessionId) {
        if (sessionId == null) return null;
        return registry.get(Number(sessionId)) ?? null;
    }

    // ---- Session creation primitives (AbstractSession.create*) --------------------
    async createFFmpegSession(commandArguments, logRedirectionStrategy = null) {
        const effectiveLogRedirectionStrategy =
            logRedirectionStrategy ?? this._logRedirectionStrategy;
        const sessionMap = await this._createNativeSession(
            'ffmpeg',
            commandArguments
        );
        const session = createSession(SessionType.FFMPEG);
        session._apply(sessionMap);
        // The caller's array is the source of truth for the arguments, exactly as Flutter's
        // AbstractSession.createFFmpegSession() assigns _argumentsArray directly instead of
        // reading it back out of the native session map. Identical to the round trip with a
        // current module, and the only correct answer without one - these create paths call
        // _apply() rather than _applySessionMap(), so they have no parseArguments fallback.
        session._arguments = Array.isArray(commandArguments) ? commandArguments : [];
        session._logRedirectionStrategy = effectiveLogRedirectionStrategy;
        this._indexSession(session);
        this._recordLogRedirectionStrategy(session, logRedirectionStrategy);
        return session;
    }

    async createFFprobeSession(commandArguments, logRedirectionStrategy = null) {
        const effectiveLogRedirectionStrategy =
            logRedirectionStrategy ?? this._logRedirectionStrategy;
        const sessionMap = await this._createNativeSession(
            'ffprobe',
            commandArguments
        );
        const session = createSession(SessionType.FFPROBE);
        session._apply(sessionMap);
        // See createFFmpegSession().
        session._arguments = Array.isArray(commandArguments) ? commandArguments : [];
        session._logRedirectionStrategy = effectiveLogRedirectionStrategy;
        this._indexSession(session);
        this._recordLogRedirectionStrategy(session, logRedirectionStrategy);
        return session;
    }

    async createMediaInformationSession(commandArguments) {
        const sessionMap = await this._createNativeSession(
            'mediaInformation',
            commandArguments
        );
        const session = createSession(SessionType.MEDIA_INFORMATION);
        session._apply(sessionMap);
        // See createFFmpegSession().
        session._arguments = Array.isArray(commandArguments) ? commandArguments : [];
        this._indexSession(session);
        this._recordLogRedirectionStrategy(
            session,
            session.getLogRedirectionStrategy?.()
        );
        return session;
    }

    // ---- Execution primitives (FFmpegKitConfig.*Execute) --------------------------
    // Each runs an already-created native session by id and populates the public JS
    // session. The async variants resolve after the request is posted and fire the
    // session's completeCallback on completion or asynchronous failure.
    //
    // These are all `async` so that a bad session rejects the returned promise instead
    // of throwing out of the call itself - Flutter and React Native always hand back a
    // Future/Promise here, so a synchronous throw would slip past a caller's .catch().
    _execPayload(session) {
        const sessionId = session.getSessionId();
        if (sessionId == null) {
            throw new Error('Session must be created before execution.');
        }
        return {sessionId};
    }

    async ffmpegExecute(session) {
        return this._run('execute', this._execPayload(session), session);
    }

    async asyncFFmpegExecute(session) {
        return this._runAsync('executeAsync', this._execPayload(session), session);
    }

    async ffprobeExecute(session) {
        return this._run('ffprobe', this._execPayload(session), session);
    }

    async asyncFFprobeExecute(session) {
        return this._runAsync('ffprobeAsync', this._execPayload(session), session);
    }

    async getMediaInformationExecute(session, waitTimeout = null) {
        const payload = this._execPayload(session);
        if (waitTimeout != null) payload.waitTimeout = waitTimeout;
        return this._run('getMediaInformation', payload, session);
    }

    async asyncGetMediaInformationExecute(session, waitTimeout = null) {
        const payload = this._execPayload(session);
        if (waitTimeout != null) payload.waitTimeout = waitTimeout;
        return this._runAsync('getMediaInformationAsync', payload, session);
    }

    // ---- Redirection ---------------------------------------------------------------
    async enableRedirection() {
        await this._call('enableRedirection', {});
        this._logsEnabled = true;
        this._statisticsEnabled = true;
    }

    async disableRedirection() {
        await this._call('disableRedirection', {});
    }

    async cancel(sessionId = null) {
        await this._call('cancel', {sessionId});
    }

    async getArch() {
        const msg = await this._call('getArch', {});
        return msg.result ? msg.result.arch : '';
    }

    async getPackageName() {
        const msg = await this._call('getPackageName', {});
        return msg.result ? msg.result.packageName : '';
    }

    async getExternalLibraries() {
        const msg = await this._call('getExternalLibraries', {});
        return msg.result ? msg.result.externalLibraries : [];
    }

    // ---- FFmpegKitConfig helpers --------------------------------------------------
    getLogLevel() {
        return this._activeLogLevel;
    }

    async setLogLevel(level) {
        const numericLevel = Number(level);
        if (Number.isFinite(numericLevel)) {
            this._activeLogLevel = numericLevel;
            this._logLevelConfigured = true;
        }
        const msg = await this._call('setLogLevel', {level: numericLevel});
        const appliedLevel = Number(msg.result?.level);
        if (msg.result?.level != null && Number.isFinite(appliedLevel)) {
            this._activeLogLevel = appliedLevel;
        }
    }

    // Native setEnvironmentVariable() is setenv(), which returns 0 on success and -1 on
    // failure. Surface that instead of silently reporting success for an env var that
    // was never set.
    async setEnvironmentVariable(variableName, variableValue) {
        const msg = await this._call('setEnvironmentVariable', {
            variableName,
            variableValue,
        });
        const returnCode = msg.result?.returnCode;
        if (returnCode != null && returnCode !== 0) {
            throw new Error(
                `Setting environment variable ${variableName} failed with return code ${returnCode}.`
            );
        }
    }

    // Web-only event switches: these gate JS callback dispatch from drained worker
    // events. Native session log/statistics storage remains enabled, so final
    // session getters still expose the complete native record.
    async enableLogs() {
        await this.initialize();
        this._logsEnabled = true;
    }

    async disableLogs() {
        await this.initialize();
        this._logsEnabled = false;
    }

    async enableStatistics() {
        await this.initialize();
        this._statisticsEnabled = true;
    }

    async disableStatistics() {
        await this.initialize();
        this._statisticsEnabled = false;
    }

    async setFontconfigConfigurationPath(path) {
        await this._call('setFontconfigConfigurationPath', {path});
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

    // No getVersion() here on purpose: FFmpegKitConfig.getVersion() reports the
    // package version constant without starting the worker, exactly as the Flutter and
    // React Native plugins report theirs. The native library version from MSG_BOOT is
    // kept in _info.version, which initialize() resolves with.
    getFFmpegVersion() {
        return this._info?.ffmpegVersion ?? null;
    }

    getBuildDate() {
        return this._info?.buildDate ?? null;
    }

    // ---- Session history (FFmpegKitConfig.*, FFmpegKit/FFprobeKit.list*) ----------
    async getSessions() {
        const msg = await this._call('getSessions', {});
        return this._mapToSessions(msg.result?.sessions);
    }

    async getSession(sessionId) {
        if (sessionId == null) return null;
        const msg = await this._call('getSession', {sessionId});
        return this._mapToSession(msg.result?.session);
    }

    async getLastSession() {
        const msg = await this._call('getLastSession', {});
        return this._mapToSession(msg.result?.session);
    }

    async getLastCompletedSession() {
        const msg = await this._call('getLastCompletedSession', {});
        return this._mapToSession(msg.result?.session);
    }

    // Native sessionStateFromInt() silently coerces an unrecognised state to Completed,
    // which would answer a nonsense query with a plausible-looking list. The other
    // platforms cannot reach that path because they pass an enum index, so validate here
    // and reject instead. Strict membership rather than Number() coercion, because
    // Number(null) is 0 - a missing argument would become a valid query for CREATED.
    async getSessionsByState(state) {
        if (!SESSION_STATES.includes(state)) {
            throw new Error(
                `Unknown session state: ${state}. Use one of the SessionState values.`
            );
        }
        const msg = await this._call('getSessionsByState', {state});
        return this._mapToSessions(msg.result?.sessions);
    }

    async getSessionHistorySize() {
        const msg = await this._call('getSessionHistorySize', {});
        return msg.result?.size ?? 0;
    }

    async setSessionHistorySize(size) {
        await this._call('setSessionHistorySize', {size});
    }

    async clearSessions() {
        await this._call('clearSessions', {});
        this._forgetAllSessions();
    }

    async deleteSession(sessionId) {
        if (sessionId == null) return;
        await this._call('deleteSession', {sessionId});
        this._forgetSession(sessionId);
    }

    async messagesInTransmit(sessionId) {
        const msg = await this._call('messagesInTransmit', {sessionId});
        return msg.result ? msg.result.count : 0;
    }

    async listFFmpegSessions() {
        const msg = await this._call('getFFmpegSessions', {});
        return this._mapToSessions(msg.result?.sessions);
    }

    async listFFprobeSessions() {
        const msg = await this._call('getFFprobeSessions', {});
        return this._mapToSessions(msg.result?.sessions);
    }

    async listMediaInformationSessions() {
        const msg = await this._call('getMediaInformationSessions', {});
        return this._mapToSessions(msg.result?.sessions);
    }

    // ---- MediaInformationJsonParser --------------------------------------------
    async mediaInformationJsonParserFrom(ffprobeJsonOutput) {
        const msg = await this._call('mediaInformationJsonParserFrom', {ffprobeJsonOutput});
        return msg.result ? msg.result.media : null;
    }

    async mediaInformationJsonParserFromWithError(ffprobeJsonOutput) {
        const msg = await this._call('mediaInformationJsonParserFromWithError', {
            ffprobeJsonOutput,
        });
        return msg.result ? msg.result.media : null;
    }

    // ---- Web-only virtual filesystem I/O (MEMFS lives inside the worker) ----------
    async writeFile(path, data) {
        const copiedData = copyUint8Array(data);
        await this._call(
            'writeFile',
            {path, data: copiedData},
            transferForUint8Array(copiedData)
        );
    }

    async readFile(path) {
        const msg = await this._call('readFile', {path});
        return msg.result ? msg.result.data : null;
    }

    /**
     * Mounts files/blobs read-only via WORKERFS at `mountPoint`, so FFmpeg reads them
     * by path without copying into the wasm heap. `files` are File objects; `blobs`
     * are `{ name, data: Blob }`. Available only because the module links WORKERFS.
     */
    async mount(mountPoint, {files = [], blobs = []} = {}) {
        await this._call('mount', {mountPoint, files, blobs});
    }

    // ---- ffkitmem:/ffkitstream: I/O objects (live in the worker; handled by id) ----
    async ioCreate(kind, params = {}) {
        const copiedData = copyUint8Array(params.data);
        const transfer = transferForUint8Array(copiedData);
        const msg = await this._call(
            'ioCreate',
            {...params, kind, data: copiedData},
            transfer
        );
        return msg.result; // { handle, url }
    }

    async ioOutputBytes(handle) {
        const msg = await this._call('ioOutputBytes', {handle});
        return msg.result?.data ?? new Uint8Array(0);
    }

    async ioGetSize(handle) {
        const msg = await this._call('ioGetSize', {handle});
        return msg.result ? msg.result.size : 0;
    }

    async ioStreamWrite(handle, data) {
        const copiedData = copyUint8Array(data);
        const transfer = transferForUint8Array(copiedData);
        const msg = await this._call('ioStreamWrite', {handle, data: copiedData}, transfer);
        return msg.result ? msg.result.written : 0;
    }

    async ioStreamCloseInput(handle) {
        await this._call('ioStreamCloseInput', {handle});
    }

    async ioStreamRead(handle, maxBytes) {
        const msg = await this._call('ioStreamRead', {handle, maxBytes});
        return msg.result ? msg.result.data : null;
    }

    async ioClose(handle) {
        await this._call('ioClose', {handle});
    }

    // ---- Internals ----------------------------------------------------------------

    // Runs an execute/probe op whose reply carries a serialized session. Resolves the
    // returned promise with the populated public session object.
    async _run(op, args, session) {
        await this.initialize();
        const id = ++this._seq;
        return new Promise((resolve, reject) => {
            this._pending.set(id, {resolve, reject, session});
            this._indexSession(session);
            try {
                this._worker.postMessage({id, op, args});
                // Both fields, as native AbstractSession::startRunning() sets them. The
                // start time is not cosmetic: _failSession derives the duration from it, and
                // a session failed by a lost runtime would otherwise report a real end time
                // next to a zero duration. The terminal result overwrites it with native's
                // own value, which stays authoritative.
                session._state = SessionState.RUNNING;
                session._startTime = Date.now();
            } catch (e) {
                this._pending.delete(id);
                reject(e);
            }
        });
    }

    // Like _run, but for async execution: resolves after postMessage accepts the
    // request. The session's completeCallback is invoked later when the terminal
    // result or error arrives from the worker.
    async _runAsync(op, args, session) {
        await this.initialize();
        const id = ++this._seq;
        return new Promise((resolve, reject) => {
            this._pending.set(id, {resolve, reject, session, settled: true});
            this._indexSession(session);
            try {
                this._worker.postMessage({id, op, args});
                session._state = SessionState.RUNNING;
                session._startTime = Date.now();
                resolve(session);
            } catch (e) {
                this._pending.delete(id);
                reject(e);
            }
        });
    }

    // Indexes a session by id once the worker has reported it.
    _indexSession(session) {
        const id = session.getSessionId();
        if (id != null) {
            this._sessionsById.set(Number(id), session);
        }
    }

    _syncConfiguredLogLevel() {
        if (!this._logLevelConfigured || !this._worker) return;

        this._worker.postMessage({
            id: ++this._seq,
            op: 'setLogLevel',
            args: {level: this._activeLogLevel},
        });
    }

    // Set-or-delete, never set-only: a session created without an explicit strategy must
    // leave no entry behind, the way a fresh key in Flutter's logRedirectionStrategyMap
    // has none. Flutter can write unconditionally because its session ids never restart
    // within a process; ours restart with every wasm runtime, so an entry that was merely
    // left in place would be read back as this session's own strategy.
    _recordLogRedirectionStrategy(session, strategy) {
        const id = session?.getSessionId?.();
        if (id == null) return;

        if (strategy != null) {
            this._logRedirectionStrategiesById.set(Number(id), strategy);
        } else {
            this._logRedirectionStrategiesById.delete(Number(id));
        }
    }

    // Every registry keyed by session id, so forgetting a session can never miss one.
    _sessionRegistries() {
        return [
            this._sessionsById,
            this._logRedirectionStrategiesById,
            this._logCallbacksById,
            this._statisticsCallbacksById,
            this._completeCallbacksById,
        ];
    }

    // Drops everything recorded for a session id: its routing entry, its log redirection
    // strategy and its callbacks. Mirrors FFmpegKitFactory.deleteSession() on the other
    // platforms, which clears the same set of maps, so the session's getLogCallback() /
    // getStatisticsCallback() / getCompleteCallback() report null from here on.
    _forgetSession(sessionId) {
        const id = Number(sessionId);
        if (!Number.isFinite(id)) return;

        for (const registry of this._sessionRegistries()) registry.delete(id);
        for (const pending of this._pending.values()) {
            if (pending.session?.getSessionId?.() === id) pending.deleted = true;
        }
    }

    _forgetAllSessions() {
        for (const registry of this._sessionRegistries()) registry.clear();
        for (const pending of this._pending.values()) pending.deleted = true;
    }

    // True while an execute/probe request for this session is still outstanding, i.e.
    // while its log/statistics buffers are owned by the live event stream.
    _hasPendingExecution(sessionId) {
        for (const pending of this._pending.values()) {
            if (pending.session?.getSessionId?.() === sessionId) return true;
        }
        return false;
    }

    // Applies the strategy recorded for this session id, when one was registered at
    // creation. Mirrors Flutter's create*SessionFromMap, which reads the same map, and is
    // why AbstractSession's *FromMap statics delegate here rather than reimplementing it.
    // A session that already carries a strategy of its own (MediaInformationSession is
    // always NEVER_PRINT_LOGS) keeps it.
    applyRecordedLogRedirectionStrategy(session) {
        const id = session?.getSessionId?.();
        if (id == null) return session;
        const strategy = this._logRedirectionStrategiesById.get(Number(id));
        if (strategy != null && session.getLogRedirectionStrategy?.() == null) {
            session._logRedirectionStrategy = strategy;
        }
        return session;
    }

    // ---- The getAll* family (AbstractSession/FFmpegSession getAll*) ---------------
    // Each one calls the native method of the same name, which waits for the session's
    // messages in transmit up to waitTimeout and then returns the delivered record -
    // exactly what the Flutter and React Native plugins do: forward waitTimeout to native
    // and hand back what it returns, holding no opinion of their own.
    //
    // Each resolves null when there is nothing to ask: no session id, or no runtime. A
    // getter must never start one, and a freshly booted module would not know this
    // session id anyway - the session then answers from the record it already holds.
    async _getAll(op, session, waitTimeout) {
        const sessionId = session?.getSessionId?.();
        if (sessionId == null || !this._worker) return null;

        const payload = {sessionId};
        if (waitTimeout != null) payload.waitTimeout = waitTimeout;
        return (await this._call(op, payload)).result;
    }

    async getAllLogs(session, waitTimeout = null) {
        const logEntries = (await this._getAll('getAllLogs', session, waitTimeout))
            ?.logEntries;
        return Array.isArray(logEntries)
            ? logEntries.map((l) => new Log(l.sessionId, l.level, l.message))
            : null;
    }

    async getAllLogsAsString(session, waitTimeout = null) {
        const logs = (await this._getAll('getAllLogsAsString', session, waitTimeout))?.logs;
        return typeof logs === 'string' ? logs : null;
    }

    async getAllStatistics(session, waitTimeout = null) {
        const statistics = (await this._getAll('getAllStatistics', session, waitTimeout))
            ?.statistics;
        return Array.isArray(statistics) ? statistics.map((s) => new Statistics(s)) : null;
    }

    _mapToSession(sessionMap) {
        if (!sessionMap) return null;
        const sessionId = sessionMap.sessionId;
        const liveSession =
            sessionId == null ? null : this._sessionsById.get(Number(sessionId)) ?? null;
        if (liveSession) {
            // Merging a native snapshot into a session that is still executing must not
            // touch its log/statistics buffers. The worker's event poller drains on its
            // own schedule, independently of the op that produced this snapshot, so the
            // snapshot can already contain an entry whose MSG_LOG/MSG_STATISTICS is still
            // queued for delivery - applying both would record it twice. The terminal
            // result replaces both buffers wholesale once the run ends, and it is posted
            // after the final drain, so nothing is lost by skipping them here.
            liveSession._applySessionMap(
                this._hasPendingExecution(Number(sessionId))
                    ? withoutLiveEventFields(sessionMap)
                    : sessionMap
            );
            return liveSession;
        }
        const session = createSession(sessionTypeFromMap(sessionMap.type));
        session._applySessionMap(sessionMap);
        return this.applyRecordedLogRedirectionStrategy(session);
    }

    _mapToSessions(sessionMaps) {
        return (sessionMaps ?? [])
            .map((sessionMap) => this._mapToSession(sessionMap))
            .filter((session) => session != null);
    }

    async _createNativeSession(kind, commandArguments) {
        const payload = {
            kind,
            arguments: Array.isArray(commandArguments) ? commandArguments : [],
        };
        const msg = await this._call('createSession', payload);
        return msg.result;
    }

    // Runs a plain op whose reply is a raw result payload.
    async _call(op, args, transfer = []) {
        await this.initialize();
        const id = ++this._seq;
        return new Promise((resolve, reject) => {
            this._pending.set(id, {resolve, reject});
            try {
                this._worker.postMessage({id, op, args}, transfer);
            } catch (e) {
                this._pending.delete(id);
                reject(e);
            }
        });
    }

    _onMessage(msg) {
        if (msg.type === MSG_BOOT) {
            this._info = {
                version: msg.version,
                ffmpegVersion: msg.ffmpegVersion,
                buildDate: msg.buildDate,
            };
            const bootLogLevel = Number(msg.logLevel);
            if (
                !this._logLevelConfigured &&
                msg.logLevel != null &&
                Number.isFinite(bootLogLevel)
            ) {
                this._activeLogLevel = bootLogLevel;
            }
            this._syncConfiguredLogLevel();
            this._resolveInit?.(this._info.version);
            return;
        }
        if (msg.type === MSG_FATAL) {
            this._failRuntime(new Error(msg.message || 'worker fatal error'));
            return;
        }
        if (msg.type === MSG_SESSION_DELETED) {
            this._forgetSession(msg.sessionId);
            return;
        }

        // Live events are drained from a module-wide native buffer. The request id only
        // identifies which worker poll drained the batch; each event's sessionId is the
        // authority for routing it to the correct public session.
        if (msg.type === MSG_LOG) {
            const sessionId = msg.log?.sessionId;
            if (sessionId == null) {
                return;
            }
            const session = this._sessionsById.get(sessionId) ?? null;
            const log = new Log(sessionId, msg.log.level, msg.log.message);

            // Recorded ahead of both gates below, deliberately - this is not an ordering
            // slip. The session's log list mirrors its NATIVE record, and native has
            // already stored this entry by the time the event reaches us: process_log()
            // in FFmpegKitConfig.cpp applies the active level and returns BEFORE
            // session->addLog(), so everything that arrives here is something native
            // kept. The gates decide who is told about a log, not whether the session
            // has it - the same split as on Flutter and React Native, where the plugin's
            // logsEnabled check and the Dart/JS level re-check both run long after the
            // native session holds the entry and neither can take it back out of
            // getAllLogs(). Skipping the append would make getLogs() contradict
            // getAllLogs() mid-run, and then contradict itself the moment the terminal
            // result replaces the buffer with the full native record.
            session?._addLog(log);

            if (!this._logsEnabled) {
                return;
            }
            this._dispatchLog(log);
            return;
        }
        if (msg.type === MSG_STATISTICS) {
            const sessionId = msg.statistics?.sessionId;
            if (sessionId == null) {
                return;
            }
            const session = this._sessionsById.get(sessionId) ?? null;
            const statistics = new Statistics(msg.statistics);
            session?._addStatistics?.(statistics);
            if (!this._statisticsEnabled) {
                return;
            }
            this._dispatchStatistics(statistics);
            return;
        }

        const pending = this._pending.get(msg.id);
        if (!pending) return;

        // Terminal messages.
        this._pending.delete(msg.id);
        if (msg.type === MSG_ERROR) {
            this._failPending(pending, new Error(msg.message || 'worker error'));
            return;
        }
        if (msg.type !== MSG_RESULT) {
            this._failPending(pending, new Error('Unknown worker message type: ' + msg.type));
            return;
        }
        if (pending.session) {
            pending.session._apply(msg.result);
            if (!pending.deleted) this._indexSession(pending.session);
            if (pending.settled) {
                // executeAsync: promise already resolved after postMessage.
                if (!pending.deleted) this._dispatchComplete(pending.session);
            } else {
                pending.resolve(pending.session);
            }
        } else {
            pending.resolve(msg);
        }
    }

    _dispatchComplete(session) {
        if (!session) return;
        const sessionId = session.getSessionId();
        if (sessionId != null && this._sessionsById.get(sessionId) !== session) return;

        const sessionCallback = session.getCompleteCallback?.() || null;
        if (typeof sessionCallback === 'function') {
            try {
                sessionCallback(session);
            } catch (e) {
                console.log('Exception thrown inside session complete callback.', e);
            }
        }

        let globalCallback = null;
        if (session.isFFmpeg?.()) {
            globalCallback = this._ffmpegSessionCompleteCallback;
        } else if (session.isFFprobe?.()) {
            globalCallback = this._ffprobeSessionCompleteCallback;
        } else if (session.isMediaInformation?.()) {
            globalCallback = this._mediaInformationSessionCompleteCallback;
        }

        if (typeof globalCallback === 'function') {
            try {
                globalCallback(session);
            } catch (e) {
                console.log('Exception thrown inside global complete callback.', e);
            }
        }
    }

    // Delivers a log to the session + global callbacks, then prints it to the console when
    // the active LogRedirectionStrategy calls for it — mirroring the Flutter/RN plugins.
    // The native side gates logs using the level captured when a run starts. Keep
    // the JS callback/console path aligned with the live Flutter/RN filter too.
    //
    // Both the session callback and the strategy are resolved from the registries keyed by
    // session id, never from a session object: that is what Flutter and React Native do,
    // and it keeps a log that arrives for an unindexed session from being misjudged as
    // "no session callback defined" by the PRINT_LOGS_WHEN_*_NOT_DEFINED strategies.
    _dispatchLog(log) {
        const level = Number(log.getLevel());
        const activeLogLevel = this._activeLogLevel;
        if (
            Number.isFinite(level) &&
            ((activeLogLevel === Level.AV_LOG_QUIET &&
                level !== Level.AV_LOG_STDERR) ||
                level > activeLogLevel)
        ) {
            return;
        }

        const sessionId = log.getSessionId();
        const sessionCallback = this.getSessionLogCallback(sessionId);
        const globalCallback = this._logCallback || null;
        if (sessionCallback) {
            try {
                sessionCallback(log);
            } catch (e) {
                console.log('Exception thrown inside session log callback.', e);
            }
        }
        if (globalCallback) {
            try {
                globalCallback(log);
            } catch (e) {
                console.log('Exception thrown inside global log callback.', e);
            }
        }

        const strategy =
            (sessionId == null
                ? null
                : this._logRedirectionStrategiesById.get(Number(sessionId))) ??
            this._logRedirectionStrategy;
        switch (strategy) {
            case LogRedirectionStrategy.NEVER_PRINT_LOGS:
                return;
            case LogRedirectionStrategy.PRINT_LOGS_WHEN_GLOBAL_CALLBACK_NOT_DEFINED:
                if (globalCallback) return;
                break;
            case LogRedirectionStrategy.PRINT_LOGS_WHEN_SESSION_CALLBACK_NOT_DEFINED:
                if (sessionCallback) return;
                break;
            case LogRedirectionStrategy.PRINT_LOGS_WHEN_NO_CALLBACKS_DEFINED:
                if (globalCallback || sessionCallback) return;
                break;
            case LogRedirectionStrategy.ALWAYS_PRINT_LOGS:
            default:
                break;
        }
        if (log.getLevel() !== Level.AV_LOG_QUIET) console.log(log.getMessage());
    }

    // Registry-backed for the same reason as _dispatchLog.
    _dispatchStatistics(statistics) {
        const sessionCallback = this.getSessionStatisticsCallback(statistics.getSessionId());
        const globalCallback = this._statisticsCallback || null;
        if (sessionCallback) {
            try {
                sessionCallback(statistics);
            } catch (e) {
                console.log('Exception thrown inside session statistics callback.', e);
            }
        }
        if (globalCallback) {
            try {
                globalCallback(statistics);
            } catch (e) {
                console.log('Exception thrown inside global statistics callback.', e);
            }
        }
    }
}

// Single shared instance. The factory object is created lazily, and the Worker is
// started only by FFmpegKitConfig.init() or the first operation that needs wasm.
let _instance = null;

export function getFactory() {
    if (_instance === null) _instance = new FFmpegKitFactory();
    return _instance;
}
