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

// A file in FFmpegKit's virtual file storage, which lives in the worker's memory: private to
// FFmpegKit and temporary (gone on page reload or uninit()). Paths given to FFmpeg commands
// refer to this storage. Like dart:io's File, an object is only a path; every method that
// touches the storage is a worker request and rejects with an Error whose `code` is the POSIX
// name (ENOENT, EISDIR, ...).

import {getFactory} from './FFmpegKitFactory.js';

/** A file in FFmpegKit's virtual file storage, referenced by path. */
export class FFmpegKitVirtualFile {
    constructor(path) {
        if (typeof path !== 'string' || path.length === 0) {
            throw new TypeError('path must be a non-empty string');
        }
        this._path = path;
    }

    /**
     * A reference to `path`. Does not create or check anything. Relative paths resolve from "/".
     *
     * @param {string} path file path in the virtual storage
     * @returns {FFmpegKitVirtualFile}
     */
    static from(path) {
        return new FFmpegKitVirtualFile(path);
    }

    /**
     * Creates an empty file at a unique path chosen by the library under /ffmpegkit/tmp/,
     * keeping `extension` (e.g. "/ffmpegkit/tmp/7.srt"). FFmpeg sometimes uses the extension
     * to pick a format. Temp files are not deleted automatically.
     *
     * @param {{extension?: string}} [options]
     * @returns {Promise<FFmpegKitVirtualFile>}
     */
    static async createTemp({extension = ''} = {}) {
        return FFmpegKitVirtualFile.from(await getFactory().fsCreateTemp(extension));
    }

    /** @returns {string} the path this object refers to */
    getPath() {
        return this._path;
    }

    /** @returns {Promise<boolean>} true only when a regular file exists at the path */
    async exists() {
        return (await getFactory().fsStat(this._path)).kind === 'file';
    }

    /** @returns {Promise<number>} the file's size in bytes */
    length() {
        return getFactory().fsLength(this._path);
    }

    /**
     * Copies the file's contents out of the virtual storage. For small data used in app code;
     * use readAsBlob() for large outputs.
     *
     * @returns {Promise<Uint8Array>}
     */
    readAsBytes() {
        return getFactory().fsRead(this._path);
    }

    /**
     * Creates or replaces the file with a copy of `bytes`. Rejects with ENOENT when the parent
     * folder is missing. The caller keeps its own buffer.
     *
     * @param {Uint8Array} bytes
     * @returns {Promise<FFmpegKitVirtualFile>} this object
     */
    async writeAsBytes(bytes) {
        // Anything else would be coerced by the worker into an empty or truncated file.
        if (!(bytes instanceof Uint8Array)) throw new TypeError('bytes must be a Uint8Array');
        await getFactory().fsWrite(this._path, bytes);
        return this;
    }

    /**
     * Returns the contents as a Blob made inside the worker and passed to the page by
     * reference, so the bytes never pass through app memory. For saving, sharing and uploading
     * large outputs.
     *
     * @param {{type?: string}} [options] MIME type of the Blob
     * @returns {Promise<Blob>}
     */
    readAsBlob({type = ''} = {}) {
        return getFactory().fsReadBlob(this._path, type);
    }

    /**
     * Moves the file; an existing file at `newPath` is replaced. Rejects with ENOENT when the
     * parent of `newPath` is missing.
     *
     * @param {string} newPath
     * @returns {Promise<FFmpegKitVirtualFile>} a new object for `newPath`; this one keeps the old path
     */
    async rename(newPath) {
        const target = FFmpegKitVirtualFile.from(newPath);
        await getFactory().fsRename(this._path, newPath, 'file');
        return target;
    }

    /** Deletes the file. @returns {Promise<void>} */
    delete() {
        return getFactory().fsDelete(this._path, 'file');
    }
}
