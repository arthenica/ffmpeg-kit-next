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

// Deprecated web-only filesystem helpers, kept for one release. writeFile and readFile are
// thin wrappers over FFmpegKitVirtualFile that keep their earlier behaviour (parent folders are
// created, a missing file reads as null); mount is unchanged and keeps its EBUSY failure when
// the same folder is mounted twice.

import {FFmpegKitVirtualDirectory} from './FFmpegKitVirtualDirectory.js';
import {FFmpegKitVirtualFile} from './FFmpegKitVirtualFile.js';
import {getFactory} from './FFmpegKitFactory.js';

/** @deprecated Use FFmpegKitVirtualFile.from(path).writeAsBytes(data). */
export async function writeFile(path, data) {
    const index = path.lastIndexOf('/');
    if (index > 0) await FFmpegKitVirtualDirectory.from(path.slice(0, index)).create({recursive: true});
    await FFmpegKitVirtualFile.from(path).writeAsBytes(data);
}

/** @deprecated Use FFmpegKitVirtualFile.from(path).readAsBytes(), which rejects for a missing file. */
export async function readFile(path) {
    try {
        return await FFmpegKitVirtualFile.from(path).readAsBytes();
    } catch (err) {
        if (err?.code === 'ENOENT') return null;
        throw err;
    }
}

/**
 * Mounts File/Blob inputs read-only via WORKERFS at `mountPoint`. `files` are File objects;
 * `blobs` are `{ name, data: Blob }`.
 *
 * @deprecated Use FFmpegKitLocalFile.fromFile(file).
 */
export function mount(mountPoint, options = {}) {
    return getFactory().mount(mountPoint, options);
}
