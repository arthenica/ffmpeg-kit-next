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

import {argumentsToString, parseArguments} from './Arguments.js';
import {FFMPEG_KIT_VERSION, SessionState} from './Constants.js';
import {getFactory} from './FFmpegKitFactory.js';

export class FFmpegKitConfig {
    static async init(printLoadConfirmation = true) {
        await getFactory().initialize(printLoadConfirmation);
    }

    static uninit() {
        return getFactory().uninit();
    }

    // Each enable*Callback() clears the global callback when called with no argument,
    // matching the optional parameters the Flutter and React Native plugins expose.
    static enableLogCallback(logCallback = null) {
        getFactory().setLogCallback(logCallback);
    }

    /**
     * Adds a listener called with the id of every session removed from the session history. Any
     * number of listeners can be added; adding the same listener again has no effect.
     */
    static addSessionDeleteListener(listener) {
        getFactory().addSessionDeleteListener(listener);
    }

    /** Removes a listener added with addSessionDeleteListener(). */
    static removeSessionDeleteListener(listener) {
        getFactory().removeSessionDeleteListener(listener);
    }

    static enableStatisticsCallback(statisticsCallback = null) {
        getFactory().setStatisticsCallback(statisticsCallback);
    }

    static enableFFmpegSessionCompleteCallback(ffmpegSessionCompleteCallback = null) {
        getFactory().setFFmpegSessionCompleteCallback(ffmpegSessionCompleteCallback);
    }

    static getFFmpegSessionCompleteCallback() {
        return getFactory().getFFmpegSessionCompleteCallback();
    }

    static enableFFprobeSessionCompleteCallback(ffprobeSessionCompleteCallback = null) {
        getFactory().setFFprobeSessionCompleteCallback(ffprobeSessionCompleteCallback);
    }

    static getFFprobeSessionCompleteCallback() {
        return getFactory().getFFprobeSessionCompleteCallback();
    }

    static enableMediaInformationSessionCompleteCallback(
        mediaInformationSessionCompleteCallback = null
    ) {
        getFactory().setMediaInformationSessionCompleteCallback(
            mediaInformationSessionCompleteCallback
        );
    }

    static getMediaInformationSessionCompleteCallback() {
        return getFactory().getMediaInformationSessionCompleteCallback();
    }

    // Global default log redirection strategy, used by sessions that don't set their own.
    static getLogRedirectionStrategy() {
        return getFactory().getLogRedirectionStrategy();
    }

    static setLogRedirectionStrategy(logRedirectionStrategy) {
        getFactory().setLogRedirectionStrategy(logRedirectionStrategy);
    }

    static getLogLevel() {
        return getFactory().getLogLevel();
    }

    /**
     * Sets the active log level.
     *
     * The JS cache updates immediately. On web, native FFmpeg/FFprobe reads the
     * configured level when a run starts, so changing it mid-run may not affect
     * native filtering for that already-running command.
     *
     * @param {number} level Log level, one of the Level.AV_LOG_* constants.
     * @returns {Promise<void>} resolves after the worker accepts the level.
     */
    static setLogLevel(level) {
        return getFactory().setLogLevel(level);
    }

    static enableLogs() {
        return getFactory().enableLogs();
    }

    static disableLogs() {
        return getFactory().disableLogs();
    }

    static enableStatistics() {
        return getFactory().enableStatistics();
    }

    static disableStatistics() {
        return getFactory().disableStatistics();
    }

    // Enables/disables native log/statistics redirection. When disabled, JS log and
    // statistics callbacks stop receiving events. enableRedirection() does nothing after
    // shutdown.
    static enableRedirection() {
        return getFactory().enableRedirection();
    }

    static disableRedirection() {
        return getFactory().disableRedirection();
    }

    /**
     * Permanently shuts FFmpegKit down and immediately refuses new executions.
     * Running FFmpeg sessions are cancelled, streams closed, and undelivered log
     * and statistics messages dropped. Native results are preserved; output and
     * completion callbacks are not guaranteed. Cancel and await required results
     * before shutdown when predictable results matter.
     *
     * The first call allows completion callbacks for at most two seconds, then
     * up to one second for history cleanup, within its total wait budget. Queued
     * work stays on its original native worker; if served after shutdown it fails
     * without execution, and after grace its completion callbacks are dropped.
     * New submissions fail with "FFmpegKit has been shut down" without callbacks.
     * Session and global completion callbacks have separate cutoff checks.
     *
     * Running FFprobe/media information cannot be cancelled and may outlast grace.
     * Retries only wait with a fresh deadline; they never restart grace. True means
     * counted native work/history cleanup finished and running results reached the
     * page; queued workers may still hold data and later release consumer captures.
     * Keep cleanup code and state alive until all owners have released their data.
     *
     * The worker remains available until uninit(), after which no worker restarts.
     * @param {number} waitTimeout Total wait budget in milliseconds; zero provides
     * no grace on the first call and makes one nonblocking worker round trip;
     * negative waits indefinitely for counted work, with grace still bounded.
     * @returns {Promise<boolean>} Whether shutdown finished within the budget.
     */
    static shutdown(waitTimeout) { return getFactory().shutdown(waitTimeout); }

    /**
     * @returns {boolean} whether shutdown() was called on this page.
     */
    static isShutdown() { return getFactory().isShutdown(); }

    static setFontconfigConfigurationPath(path) {
        return getFactory().setFontconfigConfigurationPath(path);
    }

    static setFontDirectory(fontDirectoryPath, fontNameMapping = {}) {
        return getFactory().setFontDirectory(fontDirectoryPath, fontNameMapping);
    }

    static setFontDirectoryList(fontDirectoryList, fontNameMapping = {}) {
        return getFactory().setFontDirectoryList(fontDirectoryList, fontNameMapping);
    }

    static async getVersion() {
        return FFMPEG_KIT_VERSION;
    }

    static async getFFmpegVersion() {
        await FFmpegKitConfig.init();
        return getFactory().getFFmpegVersion() ?? '';
    }

    static async getBuildDate() {
        await FFmpegKitConfig.init();
        return getFactory().getBuildDate() ?? '';
    }

    static setEnvironmentVariable(variableName, variableValue) {
        return getFactory().setEnvironmentVariable(variableName, variableValue);
    }

    static async getPlatform() {
        return 'web';
    }

    // ---- Session history ----------------------------------------------------------
    // Every getter below reads the native session history and returns public session
    // objects built from it. Where this binding still holds the live object for a
    // session id - the instance an execute call handed back - that SAME instance is
    // refreshed from the snapshot and returned, rather than a second wrapper around
    // the same session. The Flutter and React Native plugins always mint a fresh object
    // here, so portable code should treat the result as a snapshot either way and not
    // rely on `getSession(id) === session` (true on web, false there) or on the reverse.
    //
    // Returning the live object is what keeps a running session's logs and statistics
    // intact: they are accumulated from the event stream on that instance, and a fresh
    // wrapper built from a mid-run snapshot would not have them. See _mapToSession.
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
        return getFactory().setSessionHistorySize(sessionHistorySize);
    }

    static clearSessions() {
        return getFactory().clearSessions();
    }

    static deleteSession(sessionId) {
        return getFactory().deleteSession(sessionId);
    }

    static getFFmpegSessions() {
        return getFactory().listFFmpegSessions();
    }

    static getFFprobeSessions() {
        return getFactory().listFFprobeSessions();
    }

    static getMediaInformationSessions() {
        return getFactory().listMediaInformationSessions();
    }

    static messagesInTransmit(sessionId) {
        return getFactory().messagesInTransmit(sessionId);
    }

    static sessionStateToString(state) {
        switch (state) {
            case SessionState.CREATED:
                return 'CREATED';
            case SessionState.RUNNING:
                return 'RUNNING';
            case SessionState.FAILED:
                return 'FAILED';
            case SessionState.COMPLETED:
                return 'COMPLETED';
            default:
                return '';
        }
    }

    // ---- Low-level session execute primitives -------------------------------------
    // Run an already-created session. The sync forms resolve once the run COMPLETES; the
    // async forms resolve after the request is posted (completion arrives via callback).
    // After shutdown the session fails with "FFmpegKit has been shut down" and does not
    // run or invoke completion callbacks.
    static ffmpegExecute(ffmpegSession) {
        return getFactory().ffmpegExecute(ffmpegSession);
    }

    static asyncFFmpegExecute(ffmpegSession) {
        return getFactory().asyncFFmpegExecute(ffmpegSession);
    }

    static ffprobeExecute(ffprobeSession) {
        return getFactory().ffprobeExecute(ffprobeSession);
    }

    static asyncFFprobeExecute(ffprobeSession) {
        return getFactory().asyncFFprobeExecute(ffprobeSession);
    }

    static getMediaInformationExecute(mediaInformationSession, waitTimeout = null) {
        return getFactory().getMediaInformationExecute(mediaInformationSession, waitTimeout);
    }

    static asyncGetMediaInformationExecute(mediaInformationSession, waitTimeout = null) {
        return getFactory().asyncGetMediaInformationExecute(mediaInformationSession, waitTimeout);
    }

    // ---- Argument helpers ---------------------------------------------------------
    // Implemented in Arguments.js so lower-level types can use them without depending
    // on this facade.
    static parseArguments(command) {
        return parseArguments(command);
    }

    static argumentsToString(commandArguments) {
        return argumentsToString(commandArguments);
    }
}
