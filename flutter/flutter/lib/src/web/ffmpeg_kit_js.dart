/*
 * Copyright (c) 2019-2022, 2026 Taner Sener
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

// dart:js_interop bindings for the ffmpeg-kit-next-web JS package (web/js/index.d.ts). This is the
// only file that knows the JS API's shape; everything else in the web implementation goes through
// these types. Static JS methods are called on the class object, so a JS class appears here as an
// extension type over that object. A promise result the plugin never reads is declared JSAny?: a
// value that does not match a declared non-nullable result leaves the Dart future uncompleted.

import 'dart:js_interop';

/// The JS package's module namespace (dist/index.js).
extension type FFmpegKitJsModule._(JSObject _) implements JSObject {
  @JS('FFmpegKitConfig')
  external FFmpegKitConfigJs get config;
  @JS('FFmpegKit')
  external FFmpegKitJs get ffmpegKit;
  @JS('FFprobeKit')
  external FFprobeKitJs get ffprobeKit;
  @JS('FFmpegSession')
  external FFmpegSessionClassJs get ffmpegSession;
  @JS('FFprobeSession')
  external FFprobeSessionClassJs get ffprobeSession;
  @JS('MediaInformationSession')
  external MediaInformationSessionClassJs get mediaInformationSession;
  @JS('ArchDetect')
  external ArchDetectJs get archDetect;
  @JS('Packages')
  external PackagesJs get packages;
  @JS('MediaInformationJsonParser')
  external MediaInformationJsonParserJs get mediaInformationJsonParser;
  @JS('FFmpegKitInputBuffer')
  external InputBufferClassJs get inputBuffer;
  @JS('FFmpegKitOutputBuffer')
  external OutputBufferClassJs get outputBuffer;
  @JS('FFmpegKitStreamInput')
  external StreamInputClassJs get streamInput;
  @JS('FFmpegKitStreamOutput')
  external StreamOutputClassJs get streamOutput;
  @JS('FFmpegKitVirtualFile')
  external VirtualFileClassJs get virtualFile;
  @JS('FFmpegKitVirtualDirectory')
  external VirtualDirectoryClassJs get virtualDirectory;
  @JS('FFmpegKitLocalFile')
  external LocalFileClassJs get localFile;
}

extension type FFmpegKitConfigJs._(JSObject _) implements JSObject {
  external JSPromise<JSAny?> init(JSBoolean printLoadConfirmation);
  external void enableLogCallback(JSFunction? logCallback);
  external void enableStatisticsCallback(JSFunction? statisticsCallback);
  external void addSessionDeleteListener(JSFunction listener);
  external void enableFFmpegSessionCompleteCallback(JSFunction? callback);
  external void enableFFprobeSessionCompleteCallback(JSFunction? callback);
  external void enableMediaInformationSessionCompleteCallback(
      JSFunction? callback);
  external JSNumber getLogLevel();
  external void setLogRedirectionStrategy(JSNumber strategy);
  external JSPromise<JSAny?> setLogLevel(JSNumber level);
  external JSPromise<JSAny?> enableLogs();
  external JSPromise<JSAny?> disableLogs();
  external JSPromise<JSAny?> enableStatistics();
  external JSPromise<JSAny?> disableStatistics();
  external JSPromise<JSAny?> enableRedirection();
  external JSPromise<JSAny?> disableRedirection();
  external JSPromise<JSBoolean> shutdown(JSNumber waitTimeout);
  external JSBoolean isShutdown();
  external JSPromise<JSAny?> setFontconfigConfigurationPath(JSString path);
  external JSPromise<JSAny?> setFontDirectory(
      JSString path, JSObject? fontNameMapping);
  external JSPromise<JSAny?> setFontDirectoryList(
      JSArray<JSString> paths, JSObject? fontNameMapping);
  external JSPromise<JSString> getFFmpegVersion();
  external JSPromise<JSString> getBuildDate();
  external JSPromise<JSAny?> setEnvironmentVariable(
      JSString name, JSString value);
  external JSPromise<JSString> getPlatform();
  external JSPromise<JSArray<SessionJs>> getSessions();
  external JSPromise<SessionJs?> getSession(JSNumber sessionId);
  external JSPromise<SessionJs?> getLastSession();
  external JSPromise<SessionJs?> getLastCompletedSession();
  external JSPromise<JSArray<SessionJs>> getSessionsByState(JSNumber state);
  external JSPromise<JSNumber> getSessionHistorySize();
  external JSPromise<JSAny?> setSessionHistorySize(JSNumber size);
  external JSPromise<JSAny?> clearSessions();
  external JSPromise<JSAny?> deleteSession(JSNumber sessionId);
  external JSPromise<JSNumber> messagesInTransmit(JSNumber sessionId);
  external JSPromise<SessionJs> ffmpegExecute(SessionJs session);
  external JSPromise<SessionJs> asyncFFmpegExecute(SessionJs session);
  external JSPromise<SessionJs> ffprobeExecute(SessionJs session);
  external JSPromise<SessionJs> asyncFFprobeExecute(SessionJs session);
  external JSPromise<SessionJs> getMediaInformationExecute(
      SessionJs session, JSNumber? waitTimeout);
  external JSPromise<SessionJs> asyncGetMediaInformationExecute(
      SessionJs session, JSNumber? waitTimeout);
}

extension type FFmpegKitJs._(JSObject _) implements JSObject {
  external JSPromise<JSAny?> cancel(JSNumber? sessionId);
  external JSPromise<JSArray<SessionJs>> listSessions();
}

extension type FFprobeKitJs._(JSObject _) implements JSObject {
  external JSPromise<JSArray<SessionJs>> listFFprobeSessions();
  external JSPromise<JSArray<SessionJs>> listMediaInformationSessions();
}

extension type FFmpegSessionClassJs._(JSObject _) implements JSObject {
  external JSPromise<SessionJs> create(JSArray<JSString> argumentsArray);
}

extension type FFprobeSessionClassJs._(JSObject _) implements JSObject {
  external JSPromise<SessionJs> create(JSArray<JSString> argumentsArray);
}

extension type MediaInformationSessionClassJs._(JSObject _)
    implements JSObject {
  external JSPromise<SessionJs> create(JSArray<JSString> argumentsArray);
}

extension type ArchDetectJs._(JSObject _) implements JSObject {
  external JSPromise<JSString> getArch();
}

extension type PackagesJs._(JSObject _) implements JSObject {
  external JSPromise<JSString> getPackageName();
  external JSPromise<JSArray<JSString>> getExternalLibraries();
}

extension type MediaInformationJsonParserJs._(JSObject _) implements JSObject {
  external JSPromise<MediaInformationJs?> from(JSString ffprobeJsonOutput);
  external JSPromise<MediaInformationJs> fromWithError(
      JSString ffprobeJsonOutput);
}

/// Any JS session: FFmpeg, FFprobe or media information.
extension type SessionJs._(JSObject _) implements JSObject {
  external JSNumber? getSessionId();
  external JSString getCommand();
  external JSArray<JSString> getArguments();
  external JSNumber getState();
  external ReturnCodeJs? getReturnCode();
  external JSNumber getDuration();
  external DateJs? getCreateTime();
  external DateJs? getStartTime();
  external DateJs? getEndTime();
  external JSPromise<JSString> getAllLogsAsString(JSNumber? waitTimeout);
  external JSArray<LogJs> getLogs();
  external JSPromise<JSArray<LogJs>> getAllLogs(JSNumber? waitTimeout);
  external JSString? getFailStackTrace();
  external JSPromise<JSBoolean> thereAreAsynchronousMessagesInTransmit();
  external JSBoolean isFFmpeg();
  external JSBoolean isFFprobe();
  external JSBoolean isMediaInformation();

  // FFmpeg sessions only.
  external JSArray<StatisticsJs> getStatistics();
  external JSPromise<JSArray<StatisticsJs>> getAllStatistics(
      JSNumber? waitTimeout);

  // Media information sessions only.
  external MediaInformationJs? getMediaInformation();
}

extension type DateJs._(JSObject _) implements JSObject {
  external JSNumber getTime();
}

extension type ReturnCodeJs._(JSObject _) implements JSObject {
  external JSNumber getValue();
}

extension type LogJs._(JSObject _) implements JSObject {
  external JSNumber getSessionId();
  external JSNumber getLevel();
  external JSString getMessage();
}

extension type StatisticsJs._(JSObject _) implements JSObject {
  external JSNumber getSessionId();
  external JSNumber getVideoFrameNumber();
  external JSNumber getVideoFps();
  external JSNumber getVideoQuality();
  external JSNumber getSize();
  external JSNumber getTime();
  external JSNumber getBitrate();
  external JSNumber getSpeed();
}

extension type MediaInformationJs._(JSObject _) implements JSObject {
  external JSObject? getAllProperties();
}

extension type InputBufferClassJs._(JSObject _) implements JSObject {
  external JSPromise<IoJs> fromByteArray(JSUint8Array data, JSString extension);
}

extension type OutputBufferClassJs._(JSObject _) implements JSObject {
  external JSPromise<IoJs> create(
      JSString extension, JSNumber? initialCapacity, JSNumber? maxCapacity);
}

extension type StreamInputClassJs._(JSObject _) implements JSObject {
  external JSPromise<IoJs> create(JSString extension, JSNumber? capacity);
}

extension type StreamOutputClassJs._(JSObject _) implements JSObject {
  external JSPromise<IoJs> create(JSString extension, JSNumber? capacity);
}

/// An ffkitmem:/ffkitstream: buffer or stream object.
extension type IoJs._(JSObject _) implements JSObject {
  external JSString getUrl();
  external JSPromise<JSNumber> getSize();
  external JSPromise<JSUint8Array> toByteArray();
  external JSPromise<JSNumber> write(JSUint8Array data);
  external JSPromise<JSUint8Array?> read(JSNumber maxBytes);
  external JSPromise<JSAny?> closeInput();
  external JSPromise<JSAny?> close();
}

extension type VirtualFileClassJs._(JSObject _) implements JSObject {
  external VirtualFileJs from(JSString path);
  external JSPromise<VirtualFileJs> createTemp(JSObject options);
}

extension type VirtualFileJs._(JSObject _) implements JSObject {
  external JSString getPath();
  external JSPromise<JSBoolean> exists();
  external JSPromise<JSNumber> length();
  external JSPromise<JSUint8Array> readAsBytes();
  external JSPromise<JSAny?> writeAsBytes(JSUint8Array bytes);
  external JSPromise<JSObject> readAsBlob(JSObject options);
  external JSPromise<JSAny?> rename(JSString newPath);
  external JSPromise<JSAny?> delete();
}

extension type VirtualDirectoryClassJs._(JSObject _) implements JSObject {
  external VirtualDirectoryJs from(JSString path);
}

extension type VirtualDirectoryJs._(JSObject _) implements JSObject {
  external JSString getPath();
  external JSPromise<JSBoolean> exists();
  external JSPromise<JSAny?> create(JSObject options);
  external JSPromise<JSArray<JSString>> listFiles();
  external JSPromise<JSArray<JSString>> listDirectories();
  external JSPromise<JSAny?> rename(JSString newPath);
  external JSPromise<JSAny?> delete();
}

extension type LocalFileClassJs._(JSObject _) implements JSObject {
  external JSPromise<LocalFileJs> fromFile(JSObject file);
}

extension type LocalFileJs._(JSObject _) implements JSObject {
  external JSString getPath();
  external JSPromise<JSAny?> close();
}
