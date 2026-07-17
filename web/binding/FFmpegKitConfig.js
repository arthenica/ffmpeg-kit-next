// Public FFmpegKitConfig — same name and methods as the native platforms. The
// log/statistics callbacks register a plain JS function on the main thread;
// FFmpegKitFactory dispatches the worker's forwarded events to it, so the
// cross-thread event bridge stays entirely internal. Version getters return values
// captured when the module became ready (available after init() resolves).

import { getFactory } from './FFmpegKitFactory.js';

export class FFmpegKitConfig {
  static enableLogCallback(logCallback) {
    getFactory().setLogCallback(logCallback);
  }

  static enableStatisticsCallback(statisticsCallback) {
    getFactory().setStatisticsCallback(statisticsCallback);
  }

  static setLogLevel(level) {
    return getFactory().setLogLevel(level);
  }

  static setFontconfigConfigurationPath(path) {
    return getFactory().setFontconfigConfigurationPath(path);
  }

  static setFontDirectory(fontDirectoryPath, fontNameMapping = {}) {
    return getFactory().setFontDirectory(fontDirectoryPath, fontNameMapping);
  }

  static setFontDirectoryList(fontDirectoryList, fontNameMapping = {}) {
    return getFactory().setFontDirectoryList(fontDirectoryList, fontNameMapping);
  }

  static getVersion() {
    return getFactory().getVersion();
  }

  static getFFmpegVersion() {
    return getFactory().getFFmpegVersion();
  }

  static getBuildDate() {
    return getFactory().getBuildDate();
  }

  // ---- Session history ----------------------------------------------------------
  static getSessions() {
    return getFactory().getSessions();
  }

  static getSession(sessionId) {
    return getFactory().getSession(sessionId);
  }

  static getLastSession() {
    return getFactory().getLastSession();
  }

  static getLastCompletedSession() {
    return getFactory().getLastCompletedSession();
  }

  static getSessionsByState(state) {
    return getFactory().getSessionsByState(state);
  }

  static getSessionHistorySize() {
    return getFactory().getSessionHistorySize();
  }

  static setSessionHistorySize(sessionHistorySize) {
    getFactory().setSessionHistorySize(sessionHistorySize);
  }

  static clearSessions() {
    getFactory().clearSessions();
  }
}
