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

// A file on the user's computer, for example one chosen with <input type="file">, made readable
// by FFmpeg without copying it. The worker mounts it read-only (Emscripten WORKERFS) in a folder of its own under
// /ffmpegkit/local/, and FFmpeg reads the parts it needs from the browser's File on demand.

import {getFactory} from './FFmpegKitFactory.js';

/**
 * A file on the user's computer, for example one chosen with <input type="file">. It gets a
 * read-only path in FFmpegKit's virtual file storage; the file is not copied.
 */
export class FFmpegKitLocalFile {
    constructor(id, path, generation) {
        this._id = id;
        this._path = path;
        this._generation = generation;
        this._closed = false;
    }

    /**
     * Gives a file on the user's computer, for example one chosen with <input type="file">, a
     * read-only path FFmpeg can read, without copying it. The file appears at a
     * path chosen by the library, e.g. "/ffmpegkit/local/3/video.mp4". The path keeps the file's
     * own name, which may contain spaces or quotes, so pass it to FFmpeg as an argument
     * (FFmpegKit.executeWithArguments) rather than inside a command string.
     *
     * @param {File|{name: string, data: Blob}} file a File, or a Blob with a name
     * @returns {Promise<FFmpegKitLocalFile>}
     */
    static async fromFile(file) {
        const isFile = typeof File !== 'undefined' && file instanceof File;
        const isNamedBlob = !isFile && file != null && typeof Blob !== 'undefined' && file.data instanceof Blob;
        if (!isFile && !isNamedBlob) throw new TypeError('file must be a File or a {name, data: Blob} object');
        // The name becomes the last part of the mounted path.
        if (typeof file.name !== 'string' || file.name.length === 0 || file.name.includes('/')) {
            throw new TypeError('the file name must be a non-empty string without "/"');
        }
        const factory = getFactory();
        const {id, path} = await factory.localMount(file);
        return new FFmpegKitLocalFile(id, path, factory.runtimeGeneration);
    }

    /** @returns {string} the path FFmpeg commands use for this file */
    getPath() {
        return this._path;
    }

    /**
     * Releases the file. Safe to call more than once. Call it only after every FFmpeg session
     * that reads the file has finished; the library does not track that. After uninit() the
     * mount is already gone with the runtime, so this resolves without starting a new worker.
     *
     * @returns {Promise<void>}
     */
    async close() {
        if (this._closed) return;
        this._closed = true;
        const factory = getFactory();
        if (factory.runtimeGeneration !== this._generation) return;
        try {
            await factory.localUnmount(this._id);
        } catch (err) {
            // uninit() ended the runtime while the request was in flight; the mount is gone with it.
            if (factory.runtimeGeneration !== this._generation) return;
            throw err;
        }
    }
}
