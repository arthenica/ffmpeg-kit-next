// Public wrappers for the ffkitmem:/ffkitstream: in-memory I/O protocols. These let
// FFmpeg read inputs and write outputs without staging files in MEMFS (and its heap
// copy). Each wrapper is a thin handle to a C++ object living in the worker; the
// worker keeps the real embind object in a registry keyed by an opaque handle, and
// these methods post ops to operate on it. Use getUrl() as an -i input or output
// target in a command.
//
// Streaming note: the worker services stream write()/read() while executeAsync runs
// on its own pthread, but it does so NON-BLOCKING (it must not block its own event
// loop). write() may accept fewer bytes than offered and read() may return null
// (nothing ready yet) — loop and retry, interleaved with the running command.

import { getFactory } from './FFmpegKitFactory.js';

/** Seekable in-memory input built from a byte array. Addressable via ffkitmem:. */
export class FFmpegKitInputBuffer {
  constructor(handle, url) {
    this._handle = handle;
    this._url = url;
  }
  /** @param {Uint8Array} data  @param {string} extension  hints the container, e.g. "mp4" */
  static async fromByteArray(data, extension = '') {
    const { handle, url } = await getFactory().ioCreate('inputBuffer', { extension, data });
    return new FFmpegKitInputBuffer(handle, url);
  }
  getUrl() {
    return this._url;
  }
  close() {
    return getFactory().ioClose(this._handle);
  }
}

/** Seekable in-memory output; read the produced bytes with toByteArray() afterward. */
export class FFmpegKitOutputBuffer {
  constructor(handle, url) {
    this._handle = handle;
    this._url = url;
  }
  static async create(extension = '', { initialCapacity, maxCapacity } = {}) {
    const { handle, url } = await getFactory().ioCreate('outputBuffer', {
      extension,
      initialCapacity,
      maxCapacity,
    });
    return new FFmpegKitOutputBuffer(handle, url);
  }
  getUrl() {
    return this._url;
  }
  /** @returns {Promise<Uint8Array>} the bytes written so far. */
  toByteArray() {
    return getFactory().ioOutputBytes(this._handle);
  }
  close() {
    return getFactory().ioClose(this._handle);
  }
}

/** Non-seekable streaming input; pump write() while a command consumes it. ffkitstream:. */
export class FFmpegKitStreamInput {
  constructor(handle, url) {
    this._handle = handle;
    this._url = url;
  }
  static async create(extension = '', { capacity } = {}) {
    const { handle, url } = await getFactory().ioCreate('streamInput', { extension, capacity });
    return new FFmpegKitStreamInput(handle, url);
  }
  getUrl() {
    return this._url;
  }
  /** Non-blocking. @returns {Promise<number>} bytes accepted into the ring (may be short). */
  write(data) {
    return getFactory().ioStreamWrite(this._handle, data);
  }
  /** Signals end-of-input so the reader sees EOF once the ring drains. */
  closeInput() {
    return getFactory().ioStreamCloseInput(this._handle);
  }
  close() {
    return getFactory().ioClose(this._handle);
  }
}

/** Non-seekable streaming output; pump read() while a command produces it. ffkitstream:. */
export class FFmpegKitStreamOutput {
  constructor(handle, url) {
    this._handle = handle;
    this._url = url;
  }
  static async create(extension = '', { capacity } = {}) {
    const { handle, url } = await getFactory().ioCreate('streamOutput', { extension, capacity });
    return new FFmpegKitStreamOutput(handle, url);
  }
  getUrl() {
    return this._url;
  }
  /**
   * Non-blocking read. @returns {Promise<Uint8Array|null>} bytes, an empty array at
   * EOF/closed, or null when nothing is ready yet (retry).
   */
  read(maxBytes) {
    return getFactory().ioStreamRead(this._handle, maxBytes);
  }
  close() {
    return getFactory().ioClose(this._handle);
  }
}
