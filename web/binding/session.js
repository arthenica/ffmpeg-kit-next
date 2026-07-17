// Public session types, mirroring the native FFmpegKitNext API. They are plain
// main-thread objects: FFmpegKitFactory creates one per call, fills it from the
// worker's serialized result, and appends live logs/statistics as events arrive.

import { SessionState } from './enums.js';
import { ReturnCode, Log, Statistics, MediaInformation } from './models.js';

export class AbstractSession {
  constructor(command, logCallback) {
    this._sessionId = null;
    this._command = command;
    this._state = SessionState.CREATED;
    this._returnCode = null;
    this._duration = 0;
    this._output = '';
    this._logsText = '';
    this._failStackTrace = null;
    this._logs = [];
    this._logCallback = logCallback || null;
  }

  getSessionId() {
    return this._sessionId;
  }
  getCommand() {
    return this._command;
  }
  getState() {
    return this._state;
  }
  getReturnCode() {
    return this._returnCode;
  }
  getDuration() {
    return this._duration;
  }
  getOutput() {
    return this._output;
  }
  getAllLogsAsString() {
    return this._logsText;
  }
  getLogs() {
    return this._logs;
  }
  getFailStackTrace() {
    return this._failStackTrace;
  }
  getLogCallback() {
    return this._logCallback;
  }

  // Populate from the worker's serialized session result.
  _apply(result) {
    if (!result) return;
    this._sessionId = result.sessionId ?? this._sessionId;
    this._command = result.command ?? this._command;
    this._state = result.state ?? this._state;
    this._returnCode =
      result.returnCode != null ? new ReturnCode(result.returnCode.value) : null;
    this._duration = result.duration ?? 0;
    this._output = result.output ?? '';
    this._logsText = result.logs ?? '';
    this._failStackTrace = result.failStackTrace || null;
    if (Array.isArray(result.logEntries)) {
      this._logs = result.logEntries.map((l) => new Log(l.sessionId, l.level, l.message));
    }
  }

  // Append a live log (called by FFmpegKitFactory as events arrive).
  _addLog(log) {
    this._logs.push(log);
  }
}

export class FFmpegSession extends AbstractSession {
  constructor(command, logCallback, statisticsCallback) {
    super(command, logCallback);
    this._statistics = [];
    this._statisticsCallback = statisticsCallback || null;
  }

  getStatistics() {
    return this._statistics;
  }
  getAllStatistics() {
    return this._statistics;
  }
  getLastReceivedStatistics() {
    return this._statistics[this._statistics.length - 1] || null;
  }
  getStatisticsCallback() {
    return this._statisticsCallback;
  }
  isFFmpeg() {
    return true;
  }
  isFFprobe() {
    return false;
  }
  isMediaInformation() {
    return false;
  }

  _apply(result) {
    super._apply(result);
    if (Array.isArray(result?.statistics)) {
      this._statistics = result.statistics.map((s) => new Statistics(s));
    }
  }

  _addStatistics(statistics) {
    this._statistics.push(statistics);
  }
}

export class FFprobeSession extends AbstractSession {
  isFFmpeg() {
    return false;
  }
  isFFprobe() {
    return true;
  }
  isMediaInformation() {
    return false;
  }
}

export class MediaInformationSession extends AbstractSession {
  constructor(command) {
    super(command, null);
    this._mediaInformation = null;
  }

  getMediaInformation() {
    return this._mediaInformation;
  }
  isFFmpeg() {
    return false;
  }
  isFFprobe() {
    return false;
  }
  isMediaInformation() {
    return true;
  }

  _apply(result) {
    super._apply(result);
    if (result?.media) {
      this._mediaInformation = new MediaInformation(result.media);
    }
  }
}
