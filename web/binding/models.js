// Public value types, mirroring the native FFmpegKitNext API. These are plain
// main-thread objects reconstructed by FFmpegKitFactory from the serialized data
// the worker posts back — never embind handles (those never leave the worker).

export class ReturnCode {
  static SUCCESS = 0;
  static CANCEL = 255;

  constructor(value) {
    this._value = value;
  }
  getValue() {
    return this._value;
  }
  isValueSuccess() {
    return this._value === ReturnCode.SUCCESS;
  }
  isValueError() {
    return this._value !== ReturnCode.SUCCESS && this._value !== ReturnCode.CANCEL;
  }
  isValueCancel() {
    return this._value === ReturnCode.CANCEL;
  }
}

export class Log {
  constructor(sessionId, level, message) {
    this._sessionId = sessionId;
    this._level = level;
    this._message = message;
  }
  getSessionId() {
    return this._sessionId;
  }
  getLevel() {
    return this._level;
  }
  getMessage() {
    return this._message;
  }
}

export class Statistics {
  constructor(data) {
    this._sessionId = data.sessionId;
    this._videoFrameNumber = data.frame;
    this._videoFps = data.fps;
    this._videoQuality = data.quality;
    this._size = data.size;
    this._time = data.time;
    this._bitrate = data.bitrate;
    this._speed = data.speed;
  }
  getSessionId() {
    return this._sessionId;
  }
  getVideoFrameNumber() {
    return this._videoFrameNumber;
  }
  getVideoFps() {
    return this._videoFps;
  }
  getVideoQuality() {
    return this._videoQuality;
  }
  getSize() {
    return this._size;
  }
  getTime() {
    return this._time;
  }
  getBitrate() {
    return this._bitrate;
  }
  getSpeed() {
    return this._speed;
  }
}

export class StreamInformation {
  constructor(data) {
    this._data = data || {};
  }
  getIndex() {
    return this._data.index ?? null;
  }
  getType() {
    return this._data.type ?? null;
  }
  getCodec() {
    return this._data.codec ?? null;
  }
  getCodecLong() {
    return this._data.codecLong ?? null;
  }
  getFormat() {
    return this._data.format ?? null;
  }
  getWidth() {
    return this._data.width ?? null;
  }
  getHeight() {
    return this._data.height ?? null;
  }
  getBitrate() {
    return this._data.bitrate ?? null;
  }
  getSampleRate() {
    return this._data.sampleRate ?? null;
  }
  getChannelLayout() {
    return this._data.channelLayout ?? null;
  }
}

export class MediaInformation {
  constructor(data) {
    this._data = data || {};
    this._streams = (this._data.streams || []).map((s) => new StreamInformation(s));
  }
  getFilename() {
    return this._data.filename ?? null;
  }
  getFormat() {
    return this._data.format ?? null;
  }
  getLongFormat() {
    return this._data.longFormat ?? null;
  }
  getDuration() {
    return this._data.duration ?? null;
  }
  getStartTime() {
    return this._data.startTime ?? null;
  }
  getSize() {
    return this._data.size ?? null;
  }
  getBitrate() {
    return this._data.bitrate ?? null;
  }
  getStreams() {
    return this._streams;
  }
}
