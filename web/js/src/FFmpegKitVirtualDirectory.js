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

// A folder in FFmpegKit's virtual file storage. The storage lives in the worker's memory:
// it is private to FFmpegKit and temporary (gone on page reload or uninit()). Like dart:io's
// Directory, an object is only a path; every method that touches the storage is a worker
// request and rejects with an Error whose `code` is the POSIX name (ENOENT, ENOTDIR, ...).

import {getFactory} from './FFmpegKitFactory.js';

/** A folder in FFmpegKit's virtual file storage, referenced by path. */
export class FFmpegKitVirtualDirectory {
    constructor(path) {
        if (typeof path !== 'string' || path.length === 0) {
            throw new TypeError('path must be a non-empty string');
        }
        this._path = path;
    }

    /**
     * A reference to `path`. Does not create or check anything. Relative paths resolve from "/".
     *
     * @param {string} path folder path in the virtual storage
     * @returns {FFmpegKitVirtualDirectory}
     */
    static from(path) {
        return new FFmpegKitVirtualDirectory(path);
    }

    /** @returns {string} the path this object refers to */
    getPath() {
        return this._path;
    }

    /** @returns {Promise<boolean>} true only when a folder exists at the path */
    async exists() {
        return (await getFactory().fsStat(this._path)).kind === 'directory';
    }

    /**
     * Creates the folder. Succeeds if it already exists. Without `recursive`, rejects with
     * ENOENT when the parent folder is missing; with `recursive: true`, creates missing parents.
     *
     * @param {{recursive?: boolean}} [options]
     * @returns {Promise<FFmpegKitVirtualDirectory>} this object
     */
    async create({recursive = false} = {}) {
        await getFactory().fsMkdir(this._path, Boolean(recursive));
        return this;
    }

    /** @returns {Promise<string[]>} names of the files directly inside, sorted */
    listFiles() {
        return getFactory().fsReaddir(this._path, 'file');
    }

    /** @returns {Promise<string[]>} names of the folders directly inside, sorted */
    listDirectories() {
        return getFactory().fsReaddir(this._path, 'directory');
    }

    /**
     * Moves the folder. Rejects with ENOENT when the parent of `newPath` is missing.
     *
     * @param {string} newPath
     * @returns {Promise<FFmpegKitVirtualDirectory>} a new object for `newPath`; this one keeps the old path
     */
    async rename(newPath) {
        const target = FFmpegKitVirtualDirectory.from(newPath);
        await getFactory().fsRename(this._path, newPath, 'directory');
        return target;
    }

    /** Deletes the folder and everything in it. @returns {Promise<void>} */
    delete() {
        return getFactory().fsDelete(this._path, 'directory');
    }
}
