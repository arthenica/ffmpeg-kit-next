// Public FFprobeKit entry point — same name and shape as the native platforms.
// Both calls return a Promise resolving with the populated session.

import { getFactory } from './FFmpegKitFactory.js';

export class FFprobeKit {
  static execute(command) {
    return getFactory().executeFFprobe(command);
  }

  static getMediaInformation(path) {
    return getFactory().getMediaInformation(path);
  }

  static listFFprobeSessions() {
    return getFactory().listFFprobeSessions();
  }
}
