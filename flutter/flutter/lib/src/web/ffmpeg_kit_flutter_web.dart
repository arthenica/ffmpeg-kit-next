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

// The web implementation of FFmpegKitPlatform. It calls the ffmpeg-kit-next-web
// JS package directly through dart:js_interop (no method channel), answers
// with the maps the native plugins return, and turns the JS package's global
// callbacks into the events the native plugins send.

import 'dart:async';
import 'dart:js_interop';

import 'package:ffmpeg_kit_next_flutter_platform_interface/ffmpeg_kit_flutter_platform_interface.dart';
import 'package:flutter/services.dart';
import 'package:flutter_web_plugins/flutter_web_plugins.dart';
import 'package:web/web.dart' as web;

import 'ffmpeg_kit_js.dart';
import 'ffmpeg_kit_web_errors.dart';
import 'ffmpeg_kit_web_loader.dart';
import 'ffmpeg_kit_web_maps.dart';

/// The JS package's LogRedirectionStrategy.NEVER_PRINT_LOGS. The Dart side
/// prints logs itself, as on the native platforms.
const int _neverPrintLogs = 4;

/// Poll step while a stream write or read waits for room or data.
const Duration _streamPollInterval = Duration(milliseconds: 10);

class FFmpegKitFlutterWeb extends FFmpegKitPlatform {
  FFmpegKitFlutterWeb();

  static void registerWith(Registrar registrar) {
    FFmpegKitPlatform.instance = FFmpegKitFlutterWeb();
  }

  final StreamController<Map<dynamic, dynamic>> _events =
      StreamController<Map<dynamic, dynamic>>.broadcast();
  final Map<int, SessionJs> _sessions = <int, SessionJs>{};
  final Map<String, IoJs> _io = <String, IoJs>{};
  FFmpegKitJsModule? _registeredWith;
  bool _loadConfirmed = false;

  @override
  Stream<Map<dynamic, dynamic>> get events => _events.stream;

  // ---- Loading and events ---------------------------------------------------

  Future<FFmpegKitJsModule> _module() async {
    final module = await FFmpegKitWebLoader.module();
    if (!identical(_registeredWith, module)) {
      _registeredWith = module;
      _registerCallbacks(module.config);
    }
    return module;
  }

  void _registerCallbacks(FFmpegKitConfigJs config) {
    config.setLogRedirectionStrategy(_neverPrintLogs.toJS);
    config.enableLogCallback(((LogJs log) {
      _events.add({'FFmpegKitLogCallbackEvent': logToMap(log)});
    }).toJS);
    config.enableStatisticsCallback(((StatisticsJs statistics) {
      _events.add({
        'FFmpegKitStatisticsCallbackEvent': statisticsToMap(statistics),
      });
    }).toJS);
    final complete = ((SessionJs session) {
      _remember(session);
      _events.add({'FFmpegKitCompleteCallbackEvent': sessionToMap(session)});
    }).toJS;
    config.enableFFmpegSessionCompleteCallback(complete);
    config.enableFFprobeSessionCompleteCallback(complete);
    config.enableMediaInformationSessionCompleteCallback(complete);
    config.addSessionDeleteListener(((JSNumber sessionId) {
      final id = sessionId.toDartInt;
      _sessions.remove(id);
      _events.add({
        'FFmpegKitSessionDeletedCallbackEvent': {'sessionId': id},
      });
    }).toJS);
  }

  /// Runs a call against the loaded JS package, turning JS rejections into
  /// PlatformExceptions.
  Future<T> _run<T>(Future<T> Function(FFmpegKitJsModule module) call) =>
      guardJs(() async => call(await _module()));

  // ---- Sessions -------------------------------------------------------------

  Map<String, Object?> _remember(SessionJs session) {
    final map = sessionToMap(session);
    final id = map['sessionId'];
    if (id is int) _sessions[id] = session;
    return map;
  }

  List<Map<String, Object?>> _rememberAll(JSArray<SessionJs> sessions) =>
      [for (final session in sessions.toDart) _remember(session)];

  Future<SessionJs> _session(FFmpegKitJsModule module, int? sessionId) async {
    if (sessionId == null) {
      throw PlatformException(
          code: 'INVALID_SESSION', message: 'Invalid session id.');
    }
    final known = _sessions[sessionId];
    if (known != null) return known;
    final found = await module.config.getSession(sessionId.toJS).toDart;
    if (found == null) {
      throw PlatformException(
          code: 'SESSION_NOT_FOUND', message: 'Session not found.');
    }
    _sessions[sessionId] = found;
    return found;
  }

  static JSArray<JSString> _strings(List<String> values) =>
      [for (final v in values) v.toJS].toJS;

  static List<Map<String, Object?>> _logs(JSArray<LogJs> logs) =>
      [for (final log in logs.toDart) logToMap(log)];

  static List<Map<String, Object?>> _statistics(JSArray<StatisticsJs> all) =>
      [for (final s in all.toDart) statisticsToMap(s)];

  @override
  Future<Map<dynamic, dynamic>?> abstractSessionCreateFFmpegSession(
          List<String> argumentsArray) =>
      _run((m) async => _remember(
          await m.ffmpegSession.create(_strings(argumentsArray)).toDart));

  @override
  Future<Map<dynamic, dynamic>?> abstractSessionCreateFFprobeSession(
          List<String> argumentsArray) =>
      _run((m) async => _remember(
          await m.ffprobeSession.create(_strings(argumentsArray)).toDart));

  @override
  Future<Map<dynamic, dynamic>?> abstractSessionCreateMediaInformationSession(
          List<String> argumentsArray) =>
      _run((m) async => _remember(await m.mediaInformationSession
          .create(_strings(argumentsArray))
          .toDart));

  @override
  Future<int?> abstractSessionGetEndTime(int? sessionId) => _run((m) async =>
      (await _session(m, sessionId)).getEndTime()?.getTime().toDartInt);

  @override
  Future<int?> abstractSessionGetDuration(int? sessionId) => _run(
      (m) async => (await _session(m, sessionId)).getDuration().toDartInt);

  @override
  Future<List<dynamic>?> abstractSessionGetAllLogs(
          int? sessionId, int? waitTimeout) =>
      _run((m) async => _logs(await (await _session(m, sessionId))
          .getAllLogs(waitTimeout?.toJS)
          .toDart));

  @override
  Future<List<dynamic>?> abstractSessionGetLogs(int? sessionId) =>
      _run((m) async => _logs((await _session(m, sessionId)).getLogs()));

  @override
  Future<String?> abstractSessionGetAllLogsAsString(
          int? sessionId, int? waitTimeout) =>
      _run((m) async => (await (await _session(m, sessionId))
              .getAllLogsAsString(waitTimeout?.toJS)
              .toDart)
          .toDart);

  @override
  Future<int?> abstractSessionGetState(int? sessionId) => _run(
      (m) async => (await _session(m, sessionId)).getState().toDartInt);

  @override
  Future<int?> abstractSessionGetReturnCode(int? sessionId) => _run((m) async =>
      (await _session(m, sessionId)).getReturnCode()?.getValue().toDartInt);

  @override
  Future<String?> abstractSessionGetFailStackTrace(int? sessionId) => _run(
      (m) async => (await _session(m, sessionId)).getFailStackTrace()?.toDart);

  @override
  Future<bool> abstractSessionThereAreAsynchronousMessagesInTransmit(
          int? sessionId) =>
      _run((m) async => (await (await _session(m, sessionId))
              .thereAreAsynchronousMessagesInTransmit()
              .toDart)
          .toDart);

  // ---- ArchDetect, FFmpegKit, FFprobeKit ------------------------------------

  @override
  Future<String> archDetectGetArch() =>
      _run((m) async => (await m.archDetect.getArch().toDart).toDart);

  @override
  Future<void> ffmpegKitCancel() =>
      _run((m) => m.ffmpegKit.cancel(null).toDart);

  @override
  Future<void> ffmpegKitCancelSession(int sessionId) =>
      _run((m) => m.ffmpegKit.cancel(sessionId.toJS).toDart);

  @override
  Future<List<dynamic>?> ffmpegKitListSessions() => _run(
      (m) async => _rememberAll(await m.ffmpegKit.listSessions().toDart));

  @override
  Future<List<dynamic>?> ffprobeKitListFFprobeSessions() => _run((m) async =>
      _rememberAll(await m.ffprobeKit.listFFprobeSessions().toDart));

  @override
  Future<List<dynamic>?> ffprobeKitListMediaInformationSessions() =>
      _run((m) async => _rememberAll(
          await m.ffprobeKit.listMediaInformationSessions().toDart));

  // ---- FFmpegKitConfig ------------------------------------------------------

  @override
  Future<bool> ffmpegKitConfigShutdown(int waitTimeout) {
    RangeError.checkValueInInterval(
        waitTimeout, -2147483648, 2147483647, 'waitTimeout');
    return _run((m) async =>
        (await m.config.shutdown(waitTimeout.toJS).toDart).toDart);
  }

  @override
  Future<bool> ffmpegKitConfigIsShutdown() =>
      _run((m) async => m.config.isShutdown().toDart);

  @override
  Future<void> ffmpegKitConfigEnableRedirection() =>
      _run((m) => m.config.enableRedirection().toDart);

  @override
  Future<void> ffmpegKitConfigDisableRedirection() =>
      _run((m) => m.config.disableRedirection().toDart);

  @override
  Future<void> ffmpegKitConfigSetFontconfigConfigurationPath(String path) =>
      _run((m) => m.config.setFontconfigConfigurationPath(path.toJS).toDart);

  @override
  Future<void> ffmpegKitConfigSetFontDirectory(
          String path, Map<String, String>? mapping) =>
      _run((m) => m.config
          .setFontDirectory(path.toJS, mapping?.jsify() as JSObject?)
          .toDart);

  @override
  Future<void> ffmpegKitConfigSetFontDirectoryList(
          List<String> fontDirectoryList, Map<String, String>? mapping) =>
      _run((m) => m.config
          .setFontDirectoryList(
              _strings(fontDirectoryList), mapping?.jsify() as JSObject?)
          .toDart);

  @override
  Future<String?> ffmpegKitConfigRegisterNewFFmpegPipe() async =>
      throw notSupportedOnWeb();

  @override
  Future<void> ffmpegKitConfigCloseFFmpegPipe(String ffmpegPipePath) async =>
      throw notSupportedOnWeb();

  @override
  Future<int?> ffmpegKitConfigWriteToPipe(
          String inputPath, String pipePath) async =>
      throw notSupportedOnWeb();

  @override
  Future<String?> ffmpegKitConfigGetFFmpegVersion() =>
      _run((m) async => (await m.config.getFFmpegVersion().toDart).toDart);

  // The web build has no LTS variant, and the JS package has no counterpart.
  @override
  // ignore: deprecated_member_use
  Future<bool?> ffmpegKitConfigIsLTSBuild() async => false;

  @override
  Future<String?> ffmpegKitConfigGetBuildDate() =>
      _run((m) async => (await m.config.getBuildDate().toDart).toDart);

  @override
  Future<void> ffmpegKitConfigSetEnvironmentVariable(
          String name, String value) =>
      _run((m) =>
          m.config.setEnvironmentVariable(name.toJS, value.toJS).toDart);

  @override
  Future<void> ffmpegKitConfigIgnoreSignal(int signal) async =>
      throw notSupportedOnWeb();

  @override
  Future<void> ffmpegKitConfigFFmpegExecute(int? sessionId) => _run((m) async {
        await m.config.ffmpegExecute(await _session(m, sessionId)).toDart;
      });

  @override
  Future<void> ffmpegKitConfigFFprobeExecute(int? sessionId) =>
      _run((m) async {
        await m.config.ffprobeExecute(await _session(m, sessionId)).toDart;
      });

  @override
  Future<void> ffmpegKitConfigGetMediaInformationExecute(
          int? sessionId, int? waitTimeout) =>
      _run((m) async {
        await m.config
            .getMediaInformationExecute(
                await _session(m, sessionId), waitTimeout?.toJS)
            .toDart;
      });

  @override
  Future<void> ffmpegKitConfigAsyncFFmpegExecute(int? sessionId) =>
      _run((m) async {
        await m.config
            .asyncFFmpegExecute(await _session(m, sessionId))
            .toDart;
      });

  @override
  Future<void> ffmpegKitConfigAsyncFFprobeExecute(int? sessionId) =>
      _run((m) async {
        await m.config
            .asyncFFprobeExecute(await _session(m, sessionId))
            .toDart;
      });

  @override
  Future<void> ffmpegKitConfigAsyncGetMediaInformationExecute(
          int? sessionId, int? waitTimeout) =>
      _run((m) async {
        await m.config
            .asyncGetMediaInformationExecute(
                await _session(m, sessionId), waitTimeout?.toJS)
            .toDart;
      });

  @override
  Future<void> ffmpegKitConfigSetLogLevel(int logLevel) =>
      _run((m) => m.config.setLogLevel(logLevel.toJS).toDart);

  @override
  Future<int?> ffmpegKitConfigGetSessionHistorySize() => _run(
      (m) async => (await m.config.getSessionHistorySize().toDart).toDartInt);

  @override
  Future<void> ffmpegKitConfigSetSessionHistorySize(int sessionHistorySize) =>
      _run((m) =>
          m.config.setSessionHistorySize(sessionHistorySize.toJS).toDart);

  @override
  Future<Map<dynamic, dynamic>?> ffmpegKitConfigGetSession(int sessionId) =>
      _run((m) async {
        final session = await m.config.getSession(sessionId.toJS).toDart;
        return session == null ? null : _remember(session);
      });

  @override
  Future<Map<dynamic, dynamic>?> ffmpegKitConfigGetLastSession() =>
      _run((m) async {
        final session = await m.config.getLastSession().toDart;
        return session == null ? null : _remember(session);
      });

  @override
  Future<Map<dynamic, dynamic>?> ffmpegKitConfigGetLastCompletedSession() =>
      _run((m) async {
        final session = await m.config.getLastCompletedSession().toDart;
        return session == null ? null : _remember(session);
      });

  @override
  Future<List<dynamic>?> ffmpegKitConfigGetSessions() =>
      _run((m) async => _rememberAll(await m.config.getSessions().toDart));

  @override
  Future<void> clearSessions() => _run((m) async {
        await m.config.clearSessions().toDart;
        _sessions.clear();
      });

  @override
  Future<void> deleteSession(int sessionId) => _run((m) async {
        await m.config.deleteSession(sessionId.toJS).toDart;
        _sessions.remove(sessionId);
      });

  @override
  Future<List<dynamic>?> ffmpegKitConfigGetSessionsByState(int sessionState) =>
      _run((m) async => _rememberAll(
          await m.config.getSessionsByState(sessionState.toJS).toDart));

  @override
  Future<int?> ffmpegKitConfigMessagesInTransmit(int sessionId) =>
      _run((m) async =>
          (await m.config.messagesInTransmit(sessionId.toJS).toDart)
              .toDartInt);

  @override
  Future<void> ffmpegKitConfigEnableLogs() =>
      _run((m) => m.config.enableLogs().toDart);

  @override
  Future<void> ffmpegKitConfigDisableLogs() =>
      _run((m) => m.config.disableLogs().toDart);

  @override
  Future<void> ffmpegKitConfigEnableStatistics() =>
      _run((m) => m.config.enableStatistics().toDart);

  @override
  Future<void> ffmpegKitConfigDisableStatistics() =>
      _run((m) => m.config.disableStatistics().toDart);

  @override
  Future<String?> ffmpegKitConfigGetPlatform() async => 'web';

  @override
  Future<String?> ffmpegKitConfigSelectDocumentForRead(
          String? type, List<String>? extraTypes) async =>
      throw notSupportedOnWeb();

  @override
  Future<String?> ffmpegKitConfigSelectDocumentForWrite(
          String? title, String? type, List<String>? extraTypes) async =>
      throw notSupportedOnWeb();

  @override
  Future<String?> ffmpegKitConfigGetSafParameter(
          String uriString, String openMode,
          [bool? reusable]) async =>
      throw notSupportedOnWeb();

  @override
  Future<void> ffmpegKitConfigUnregisterSafProtocolUrl(String safUrl) async =>
      throw notSupportedOnWeb();

  // ---- FFmpegKitFlutterInitializer ------------------------------------------

  @override
  Future<int?> ffmpegKitFlutterInitializerGetLogLevel() =>
      _run((m) async => m.config.getLogLevel().toDartInt);

  @override
  Future<void> ffmpegKitFlutterInitializerPrintLoadConfirmation(
          String version) =>
      _run((m) async {
        if (_loadConfirmed) return;
        _loadConfirmed = true;
        final packageName = (await m.packages.getPackageName().toDart).toDart;
        final arch = (await m.archDetect.getArch().toDart).toDart;
        final buildDate = (await m.config.getBuildDate().toDart).toDart;
        final prefix = packageName.isEmpty ? '' : '$packageName-';
        final versionPart = version.isEmpty ? 'unknown' : version;
        web.console.log(
            'Loaded ffmpeg-kit-next-flutter-${prefix}web-$arch-$versionPart-$buildDate.'
                .toJS);
      });

  // ---- FFmpegSession, MediaInformation, Packages ----------------------------

  @override
  Future<List<dynamic>?> ffmpegSessionGetAllStatistics(
          int? sessionId, int? waitTimeout) =>
      _run((m) async => _statistics(await (await _session(m, sessionId))
          .getAllStatistics(waitTimeout?.toJS)
          .toDart));

  @override
  Future<List<dynamic>?> ffmpegSessionGetStatistics(int? sessionId) =>
      _run((m) async =>
          _statistics((await _session(m, sessionId)).getStatistics()));

  @override
  Future<Map<dynamic, dynamic>?> mediaInformationJsonParserFrom(
          String ffprobeJsonOutput) =>
      _run((m) async => mediaInformationToMap(await m
          .mediaInformationJsonParser
          .from(ffprobeJsonOutput.toJS)
          .toDart));

  @override
  Future<Map<dynamic, dynamic>?> mediaInformationJsonParserFromWithError(
          String ffprobeJsonOutput) =>
      _run((m) async => mediaInformationToMap(await m
          .mediaInformationJsonParser
          .fromWithError(ffprobeJsonOutput.toJS)
          .toDart));

  @override
  Future<Map<dynamic, dynamic>?> mediaInformationSessionGetMediaInformation(
          int? sessionId) =>
      _run((m) async => mediaInformationToMap(
          (await _session(m, sessionId)).getMediaInformation()));

  @override
  Future<String?> getPackageName() =>
      _run((m) async => (await m.packages.getPackageName().toDart).toDart);

  @override
  Future<List<dynamic>?> getExternalLibraries() => _run((m) async => [
        for (final name
            in (await m.packages.getExternalLibraries().toDart).toDart)
          name.toDart
      ]);

  @override
  Future<List<dynamic>?> getSupportedCameraIds() async =>
      throw notSupportedOnWeb();

  // ---- ffkitmem: buffers and ffkitstream: streams ---------------------------

  String _keep(IoJs io) {
    final url = io.getUrl().toDart;
    _io[url] = io;
    return url;
  }

  IoJs _ioFor(String url, String kind) {
    final io = _io[url];
    if (io == null) {
      throw PlatformException(code: 'NOT_FOUND', message: '$kind not found.');
    }
    return io;
  }

  Future<void> _close(String url, String kind) =>
      _run((m) async {
        await _ioFor(url, kind).close().toDart;
        _io.remove(url);
      });

  @override
  Future<String?> inputBufferFromByteArray(
          Uint8List data, String? extension) =>
      _run((m) async => _keep(await m.inputBuffer
          .fromByteArray(data.toJS, (extension ?? '').toJS)
          .toDart));

  @override
  Future<void> inputBufferClose(String url) => _close(url, 'Input buffer');

  @override
  Future<String?> outputBufferCreate(
          String? extension, int? initialCapacity, int? maxCapacity) =>
      _run((m) async => _keep(await m.outputBuffer
          .create((extension ?? '').toJS, initialCapacity?.toJS,
              maxCapacity?.toJS)
          .toDart));

  @override
  Future<int?> outputBufferGetSize(String url) => _run((m) async =>
      (await _ioFor(url, 'Output buffer').getSize().toDart).toDartInt);

  @override
  Future<Uint8List?> outputBufferToByteArray(String url) => _run((m) async =>
      (await _ioFor(url, 'Output buffer').toByteArray().toDart).toDart);

  @override
  Future<void> outputBufferClose(String url) => _close(url, 'Output buffer');

  @override
  Future<String?> streamInputCreate(String? extension, int? capacity) =>
      _run((m) async => _keep(await m.streamInput
          .create((extension ?? '').toJS, capacity?.toJS)
          .toDart));

  // The JS write never blocks; like the native plugins, this waits up to
  // timeoutMs (without a timeout: until room) for the stream to take bytes.
  @override
  Future<int?> streamInputWrite(String url, Uint8List data, int? timeoutMs) =>
      _run((m) async {
        final input = _ioFor(url, 'Stream input');
        final deadline = timeoutMs == null
            ? null
            : DateTime.now().add(Duration(milliseconds: timeoutMs));
        final bytes = data.toJS;
        while (true) {
          final accepted = (await input.write(bytes).toDart).toDartInt;
          if (accepted > 0 || data.isEmpty) return accepted;
          if (deadline != null && !DateTime.now().isBefore(deadline)) return 0;
          await Future<void>.delayed(_streamPollInterval);
        }
      });

  @override
  Future<void> streamInputCloseInput(String url) => _run(
      (m) => _ioFor(url, 'Stream input').closeInput().toDart);

  @override
  Future<void> streamInputClose(String url) => _close(url, 'Stream input');

  @override
  Future<String?> streamOutputCreate(String? extension, int? capacity) =>
      _run((m) async => _keep(await m.streamOutput
          .create((extension ?? '').toJS, capacity?.toJS)
          .toDart));

  // The JS read answers null while no data is ready and an empty array at the
  // end; this waits up to timeoutMs (without a timeout: until data or the end)
  // and answers null on timeout, like the native plugins.
  @override
  Future<Uint8List?> streamOutputRead(
          String url, int maxBytes, int? timeoutMs) =>
      _run((m) async {
        final output = _ioFor(url, 'Stream output');
        final deadline = timeoutMs == null
            ? null
            : DateTime.now().add(Duration(milliseconds: timeoutMs));
        while (true) {
          final chunk = await output.read(maxBytes.toJS).toDart;
          if (chunk != null) return chunk.toDart;
          if (deadline != null && !DateTime.now().isBefore(deadline)) {
            return null;
          }
          await Future<void>.delayed(_streamPollInterval);
        }
      });

  @override
  Future<void> streamOutputClose(String url) => _close(url, 'Stream output');
}
