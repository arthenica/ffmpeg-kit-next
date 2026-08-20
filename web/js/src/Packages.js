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

import {getFactory} from './FFmpegKitFactory.js';

/**
 * Helper class to extract binary package information.
 */
export class Packages {
    /**
     * Returns the FFmpegKit binary package name.
     *
     * @returns {Promise<string>} FFmpegKit binary package name
     */
    static async getPackageName() {
        return getFactory().getPackageName();
    }

    /**
     * Returns enabled external libraries by FFmpeg.
     *
     * @returns {Promise<string[]>} enabled external libraries
     */
    static async getExternalLibraries() {
        return getFactory().getExternalLibraries();
    }
}
