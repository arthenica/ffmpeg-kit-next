## 0.4.0
- Adds `FFmpegKitPlatform.events`, the stream of log, statistics, completion and session-deleted events the plugin
  listens to; the method channel implementation reads it from the existing event channel
- Breaking for custom `FFmpegKitPlatform` implementations and mocks: the default `events` throws
  `UnimplementedError`, and `FFmpegKitConfig.init()` now listens to it, so they must override `events`
- Requires Dart 3.11 or later

## 0.3.1
- `ffmpegKitFlutterInitializerPrintLoadConfirmation` takes the plugin version to print
- Fixed `abstractSessionThereAreAsynchronousMessagesInTransmit` method mapping

## 0.3.0
- ffkit protocol data types and methods added

## 0.2.1
- Fixes the signature of getMediaInformation method 

## 0.2.0
- Implements execute methods
- Merges existing getSafParameter methods into a single method with a new openMode parameter
- Adds list media information sessions methods to FFprobeKit
- Adds getMediaInformation method

## 0.1.0
- Initial release
