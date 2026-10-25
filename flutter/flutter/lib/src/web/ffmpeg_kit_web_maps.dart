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

// Converts JS sessions, logs, statistics and media information into the maps
// the native plugins send over the method channel, with the same keys and Dart
// types, so FFmpegKitFactory builds Dart objects without web branches.

import 'dart:js_interop';

import 'ffmpeg_kit_js.dart';

int _millis(DateJs? date) => date == null ? 0 : date.getTime().toDartInt;

/// The Android plugin's toMap(Session).
Map<String, Object?> sessionToMap(SessionJs session) {
  final map = <String, Object?>{
    'sessionId': session.getSessionId()?.toDartInt,
    'createTime': _millis(session.getCreateTime()),
    'startTime': _millis(session.getStartTime()),
    'command': session.getCommand().toDart,
    'arguments': [for (final a in session.getArguments().toDart) a.toDart],
  };
  if (session.isFFprobe().toDart) {
    map['type'] = 2;
  } else if (session.isMediaInformation().toDart) {
    map['type'] = 3;
    final information = mediaInformationToMap(session.getMediaInformation());
    if (information != null) map['mediaInformation'] = information;
  } else {
    map['type'] = 1;
  }
  return map;
}

Map<String, Object?> logToMap(LogJs log) => {
      'sessionId': log.getSessionId().toDartInt,
      'level': log.getLevel().toDartInt,
      'message': log.getMessage().toDart,
    };

Map<String, Object?> statisticsToMap(StatisticsJs statistics) => {
      'sessionId': statistics.getSessionId().toDartInt,
      'videoFrameNumber': statistics.getVideoFrameNumber().toDartInt,
      'videoFps': statistics.getVideoFps().toDartDouble,
      'videoQuality': statistics.getVideoQuality().toDartDouble,
      'size': statistics.getSize().toDartInt,
      'time': statistics.getTime().toDartDouble,
      'bitrate': statistics.getBitrate().toDartDouble,
      'speed': statistics.getSpeed().toDartDouble,
    };

/// The ffprobe properties of a media information object, or null.
Map<dynamic, dynamic>? mediaInformationToMap(MediaInformationJs? information) {
  final properties = information?.getAllProperties();
  return properties == null ? null : properties.dartify() as Map<dynamic, dynamic>;
}
