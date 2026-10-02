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

import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import test from 'node:test';
import {createContext, runInContext} from 'node:vm';

// Run the real worker handler with its native module supplied by the test. Only
// module loading/startup is omitted, so error replies and handle cleanup execute
// the same code as a browser worker without needing the full FFmpeg build.
const workerSource = readFileSync(new URL('../js/ffmpegkit.worker.js', import.meta.url), 'utf8')
    .replace(/^import createFFmpegKitModule from .*;$/m, '')
    .replaceAll('import.meta.url', 'workerUrl')
    .replace(/\nworkerInit\(\);\s*$/, '\n');

function workerWithParser(parser) {
    const messages = [];
    const self = {location: {href: 'https://example.test/ffmpegkit.worker.js'}};
    const context = createContext({
        URL,
        workerUrl: new URL('../js/ffmpegkit.worker.js', import.meta.url).href,
        self,
        nativeModule: {MediaInformationJsonParser: parser},
        postMessage: (message) => messages.push(structuredClone(message)),
    });
    runInContext(workerSource + '\nModule = nativeModule;', context);
    return {
        messages,
        send: (id, op, ffprobeJsonOutput) =>
            self.onmessage({data: {id, op, args: {ffprobeJsonOutput}}}),
    };
}

function mediaHandle(properties = {}) {
    let deletions = 0;
    const handle = {
        getAllProperties: () => structuredClone(properties),
        getStreams: () => [],
        getChapters: () => [],
        delete: () => { deletions++; },
    };
    for (const property of [
        'Filename', 'Format', 'LongFormat', 'Duration', 'StartTime', 'Size', 'Bitrate', 'Tags',
    ]) {
        handle['get' + property] = () => null;
    }
    return {handle, deletions: () => deletions};
}

test('parser promise rejections report the native message and allow the next request', async () => {
    const media = mediaHandle({format: {filename: 'video.mp4'}});
    const worker = workerWithParser({
        fromWithError: async (input) => {
            if (input === '{invalid') throw new Error('Missing a name for object member.');
            return media.handle;
        },
    });

    for (let id = 1; id <= 100; id++) {
        await worker.send(id, 'mediaInformationJsonParserFromWithError', '{invalid');
        assert.deepEqual(worker.messages.at(-1), {
            id, type: 3, message: 'Missing a name for object member.',
        });
    }
    assert.equal(media.deletions(), 0);

    await worker.send(101, 'mediaInformationJsonParserFromWithError', '{}');
    assert.equal(worker.messages.at(-1).type, 2);
    assert.equal(worker.messages.at(-1).result.media.format.filename, 'video.mp4');
    assert.equal(media.deletions(), 1);
});

test('an empty parse still resolves differently for from and fromWithError', async () => {
    const from = mediaHandle();
    const withError = mediaHandle();
    const worker = workerWithParser({
        from: () => from.handle,
        fromWithError: async () => withError.handle,
    });

    await worker.send(1, 'mediaInformationJsonParserFrom', '{}');
    await worker.send(2, 'mediaInformationJsonParserFromWithError', '{}');
    assert.deepEqual(worker.messages, [
        {id: 1, type: 2, result: {media: null}},
        {id: 2, type: 2, result: {media: {streams: [], chapters: [], format: {}}}},
    ]);
    assert.equal(from.deletions(), 1);
    assert.equal(withError.deletions(), 1);
});

test('a serialization failure releases the parsed media handle exactly once', async () => {
    const media = mediaHandle();
    media.handle.getStreams = () => { throw new Error('Serialization failed'); };
    const worker = workerWithParser({fromWithError: async () => media.handle});

    await worker.send(1, 'mediaInformationJsonParserFromWithError', '{}');
    assert.deepEqual(worker.messages, [{id: 1, type: 3, message: 'Serialization failed'}]);
    assert.equal(media.deletions(), 1);
});
