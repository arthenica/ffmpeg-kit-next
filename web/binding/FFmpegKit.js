// Public FFmpegKit entry point — same name and shape as the native platforms.
// On web, execute/executeAsync return a Promise that resolves with the populated
// FFmpegSession once the command completes; the optional logCallback /
// statisticsCallback fire live as it runs (executeAsync) or in one batch on
// completion (execute — the module thread is blocked during a synchronous run).

import { getFactory } from './FFmpegKitFactory.js';

export class FFmpegKit {
  static executeAsync(command, logCallback = null, statisticsCallback = null) {
    return getFactory().executeFFmpegAsync(command, logCallback, statisticsCallback);
  }

  static execute(command, logCallback = null, statisticsCallback = null) {
    return getFactory().executeFFmpeg(command, logCallback, statisticsCallback);
  }

  static cancel(sessionId = null) {
    return getFactory().cancel(sessionId);
  }

  static listSessions() {
    return getFactory().listFFmpegSessions();
  }
}
