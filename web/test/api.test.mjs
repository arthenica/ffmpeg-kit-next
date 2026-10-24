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

// Load-time and public-surface checks for the binding layer.
//
// Nothing here starts the wasm worker, so these run under plain Node with no
// browser and no build artefacts. They cover the parts that break when the module
// layout changes: the entry point evaluating at all, the exported symbol set, and
// the SessionRegistry indirection that keeps FFmpegKitFactory free of session
// imports.

import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import path from 'node:path';
import test from 'node:test';
import {fileURLToPath} from 'node:url';

import * as api from '../js/index.js';

const WEB_ROOT = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

const EXPECTED_EXPORTS = [
    'AbstractSession',
    'ArchDetect',
    'Chapter',
    'FFmpegKit',
    'FFmpegKitConfig',
    'FFmpegKitInputBuffer',
    'FFmpegKitLocalFile',
    'FFmpegKitOutputBuffer',
    'FFmpegKitStreamInput',
    'FFmpegKitStreamOutput',
    'FFmpegKitVirtualDirectory',
    'FFmpegKitVirtualFile',
    'FFmpegSession',
    'FFprobeKit',
    'FFprobeSession',
    'Level',
    'Log',
    'LogRedirectionStrategy',
    'MediaInformation',
    'MediaInformationJsonParser',
    'MediaInformationSession',
    'Packages',
    'ReturnCode',
    'Session',
    'SessionState',
    'Statistics',
    'StreamInformation',
    'mount',
    'readFile',
    'writeFile',
];

test('the package entry point exports exactly the published surface', () => {
    assert.deepEqual(Object.keys(api).sort(), EXPECTED_EXPORTS);
});

test('SessionRegistry wires every session type to its class', () => {
    const {AbstractSession, FFmpegSession, FFprobeSession, MediaInformationSession} = api;

    // These statics go through SessionRegistry rather than importing the subclasses.
    const ffmpeg = AbstractSession.createFFmpegSessionFromMap({sessionId: 1, command: '-i a b'});
    const ffprobe = AbstractSession.createFFprobeSessionFromMap({sessionId: 2, command: '-i a'});
    const media = AbstractSession.createMediaInformationSessionFromMap({
        sessionId: 3,
        command: '-i a',
    });

    assert.ok(ffmpeg instanceof FFmpegSession);
    assert.ok(ffprobe instanceof FFprobeSession);
    assert.ok(media instanceof MediaInformationSession);

    assert.equal(ffmpeg.isFFmpeg(), true);
    assert.equal(ffprobe.isFFprobe(), true);
    assert.equal(media.isMediaInformation(), true);

    // Populated through _applySessionMap, including the parseArguments fallback.
    assert.equal(ffmpeg.getSessionId(), 1);
    assert.deepEqual(ffmpeg.getArguments(), ['-i', 'a', 'b']);

    // Media information sessions never print logs, matching the native platforms.
    assert.equal(
        media.getLogRedirectionStrategy(),
        api.LogRedirectionStrategy.NEVER_PRINT_LOGS
    );
});

test('sessions keep the Session inheritance chain', () => {
    const {AbstractSession, FFmpegSession, FFprobeSession, MediaInformationSession, Session} = api;
    for (const cls of [FFmpegSession, FFprobeSession, MediaInformationSession]) {
        assert.ok(Object.getPrototypeOf(cls) === AbstractSession, `${cls.name} extends AbstractSession`);
        assert.ok(cls.prototype instanceof Session, `${cls.name} is a Session`);
    }
});

test('parseArguments honours quoting and round-trips through FFmpegKitConfig', () => {
    const {FFmpegKitConfig} = api;
    assert.deepEqual(FFmpegKitConfig.parseArguments('-i file.mp4 -c:v libx264 out.mp4'), [
        '-i',
        'file.mp4',
        '-c:v',
        'libx264',
        'out.mp4',
    ]);
    assert.deepEqual(FFmpegKitConfig.parseArguments("-i 'my file.mp4' out.mp4"), [
        '-i',
        'my file.mp4',
        'out.mp4',
    ]);
    assert.deepEqual(FFmpegKitConfig.parseArguments('-i "my file.mp4"'), ['-i', 'my file.mp4']);
    assert.equal(FFmpegKitConfig.argumentsToString(['-i', 'a.mp4']), '-i a.mp4');
    assert.equal(FFmpegKitConfig.argumentsToString(null), 'null');
});

test('sessionStateToString covers every SessionState', () => {
    const {FFmpegKitConfig, SessionState} = api;
    assert.equal(FFmpegKitConfig.sessionStateToString(SessionState.CREATED), 'CREATED');
    assert.equal(FFmpegKitConfig.sessionStateToString(SessionState.RUNNING), 'RUNNING');
    assert.equal(FFmpegKitConfig.sessionStateToString(SessionState.FAILED), 'FAILED');
    assert.equal(FFmpegKitConfig.sessionStateToString(SessionState.COMPLETED), 'COMPLETED');
    assert.equal(FFmpegKitConfig.sessionStateToString(99), '');
});

test('getVersion reports the package version without starting the worker', async () => {
    // No Worker global exists under Node, so this would throw if it initialized.
    const packageVersion = JSON.parse(
        readFileSync(path.join(WEB_ROOT, 'package.json'), 'utf8')
    ).version;
    const distVersion = JSON.parse(
        readFileSync(path.join(WEB_ROOT, 'package.dist.json'), 'utf8')
    ).version;

    assert.equal(await api.FFmpegKitConfig.getVersion(), packageVersion);
    assert.equal(distVersion, packageVersion);
});

test('worker-backed helper return shapes match native bridges', async () => {
    const originalWorker = globalThis.Worker;

    class MockWorker {
        constructor() {
            setTimeout(() => {
                this.onmessage?.({
                    data: {
                        type: 0,
                        version: 'test',
                        ffmpegVersion: 'test',
                        buildDate: 'test',
                    },
                });
            }, 0);
        }

        postMessage({id, op, args}) {
            // Native setEnvironmentVariable() is setenv(): 0 on success, non-zero on
            // failure. The mock fails only for the variable named FAILING_VAR.
            const result =
                op === 'setEnvironmentVariable'
                    ? {returnCode: args.variableName === 'FAILING_VAR' ? -1 : 0}
                    : {data: null};
            setTimeout(() => {
                this.onmessage?.({
                    data: {
                        id,
                        type: 2,
                        result,
                    },
                });
            }, 0);
        }

        terminate() {
            this.onmessage = null;
        }
    }

    globalThis.Worker = MockWorker;
    try {
        assert.equal(
            await api.FFmpegKitConfig.setEnvironmentVariable('PARITY_TEST', '1'),
            undefined
        );

        // A failed setenv() must surface, not resolve as if it had succeeded.
        await assert.rejects(
            () => api.FFmpegKitConfig.setEnvironmentVariable('FAILING_VAR', '1'),
            /failed with return code -1/
        );

        // The execute primitives reject rather than throwing synchronously, so a
        // caller's .catch() sees a session that was never created.
        await assert.rejects(
            () => api.FFmpegKitConfig.ffmpegExecute(new api.FFmpegSession()),
            /Session must be created before execution/
        );

        // Same contract for the command-string entry points: a bad command must reject,
        // not throw out of the call, as on the native platforms.
        for (const call of [
            () => api.FFmpegKit.execute(null),
            () => api.FFmpegKit.executeAsync(null),
            () => api.FFprobeKit.execute(null),
            () => api.FFprobeKit.executeAsync(null),
            () => api.FFprobeKit.getMediaInformationFromCommand(null),
            () => api.FFprobeKit.getMediaInformationFromCommandAsync(null),
        ]) {
            await assert.rejects(call, TypeError);
        }

        const outputBuffer = new api.FFmpegKitOutputBuffer('missing', 'ffkitmem:missing');
        const bytes = await outputBuffer.toByteArray();

        assert.ok(bytes instanceof Uint8Array);
        assert.equal(bytes.length, 0);
    } finally {
        await api.FFmpegKitConfig.uninit().catch(() => {});
        if (originalWorker === undefined) delete globalThis.Worker;
        else globalThis.Worker = originalWorker;
    }
});

test('session log and statistics getters return defensive array copies', async () => {
    const session = api.AbstractSession.createFFmpegSessionFromMap({
        sessionId: 17,
        command: '-i input.mp4 output.mp4',
    });
    session._apply({
        logs: 'first log\n',
        logEntries: [
            {
                sessionId: 17,
                level: api.Level.AV_LOG_INFO,
                message: 'first log\n',
            },
        ],
        statistics: [
            {
                sessionId: 17,
                videoFrameNumber: 3,
                videoFps: 30,
                videoQuality: 0,
                size: 10,
                time: 20,
                bitrate: 30,
                speed: 1,
            },
        ],
    });

    // The getAll* family refreshes from the worker first, but only when one is running.
    // There is none here, so the getters report the record the session already holds -
    // a getter must never reject, lose data, or start a runtime of its own.
    session.getLogs().length = 0;
    (await session.getAllLogs()).push(new api.Log(17, api.Level.AV_LOG_ERROR, 'mutated\n'));
    session.getStatistics().length = 0;
    (await session.getAllStatistics()).push(
        new api.Statistics({sessionId: 17, videoFrameNumber: 99})
    );

    assert.equal(session.getLogs().length, 1);
    assert.equal((await session.getAllLogs()).length, 1);
    assert.equal(session.getStatistics().length, 1);
    assert.equal((await session.getAllStatistics()).length, 1);
    assert.equal(await session.getAllLogsAsString(), 'first log\n');
    assert.equal(await session.getOutput(), 'first log\n');
});

test('disabled log and statistics dispatch still updates live session getters', async () => {
    const originalWorker = globalThis.Worker;
    let liveEventsDelivered;
    const liveEventsPromise = new Promise((resolve) => {
        liveEventsDelivered = resolve;
    });
    let logCallbackCalls = 0;
    let statisticsCallbackCalls = 0;

    class MockWorker {
        constructor() {
            setTimeout(() => {
                this.onmessage?.({
                    data: {
                        type: 0,
                        version: 'test',
                        ffmpegVersion: 'test',
                        buildDate: 'test',
                        logLevel: api.Level.AV_LOG_TRACE,
                    },
                });
            }, 0);
        }

        postMessage({id, op, args = {}}) {
            if (op === 'createSession') {
                setTimeout(() => {
                    this.onmessage?.({
                        data: {
                            id,
                            type: 2,
                            result: {
                                sessionId: 31,
                                type: 1,
                                state: api.SessionState.CREATED,
                                command: args.arguments.join(' '),
                                arguments: args.arguments,
                                logEntries: [],
                                statistics: [],
                            },
                        },
                    });
                }, 0);
                return;
            }

            if (op === 'executeAsync') {
                setTimeout(() => {
                    this.onmessage?.({
                        data: {
                            id,
                            type: 4,
                            log: {
                                sessionId: args.sessionId,
                                level: api.Level.AV_LOG_INFO,
                                message: 'suppressed callback log\n',
                            },
                        },
                    });
                    this.onmessage?.({
                        data: {
                            id,
                            type: 5,
                            statistics: {
                                sessionId: args.sessionId,
                                videoFrameNumber: 7,
                                videoFps: 24,
                                videoQuality: 0,
                                size: 100,
                                time: 200,
                                bitrate: 300,
                                speed: 1,
                            },
                        },
                    });
                    liveEventsDelivered();
                    setTimeout(() => {
                        this.onmessage?.({
                            data: {
                                id,
                                type: 2,
                                result: {
                                    sessionId: args.sessionId,
                                    type: 1,
                                    state: api.SessionState.COMPLETED,
                                    returnCode: 0,
                                    logEntries: [
                                        {
                                            sessionId: args.sessionId,
                                            level: api.Level.AV_LOG_INFO,
                                            message: 'suppressed callback log\n',
                                        },
                                    ],
                                    statistics: [
                                        {
                                            sessionId: args.sessionId,
                                            videoFrameNumber: 7,
                                            videoFps: 24,
                                            videoQuality: 0,
                                            size: 100,
                                            time: 200,
                                            bitrate: 300,
                                            speed: 1,
                                        },
                                    ],
                                },
                            },
                        });
                    }, 20);
                }, 0);
                return;
            }

            setTimeout(() => {
                this.onmessage?.({
                    data: {
                        id,
                        type: 2,
                        result: {ok: true},
                    },
                });
            }, 0);
        }

        terminate() {
            this.onmessage = null;
        }
    }

    globalThis.Worker = MockWorker;
    try {
        const session = await api.FFmpegSession.create(
            ['-version'],
            null,
            () => {
                logCallbackCalls += 1;
            },
            () => {
                statisticsCallbackCalls += 1;
            }
        );

        await api.FFmpegKitConfig.disableLogs();
        await api.FFmpegKitConfig.disableStatistics();
        await api.FFmpegKitConfig.asyncFFmpegExecute(session);
        await liveEventsPromise;

        assert.equal(logCallbackCalls, 0);
        assert.equal(statisticsCallbackCalls, 0);
        assert.equal(session.getLogs().length, 1);
        assert.equal(session.getLogsAsString(), 'suppressed callback log\n');
        assert.equal(session.getStatistics().length, 1);
        assert.equal(session.getLastReceivedStatistics().getVideoFrameNumber(), 7);
    } finally {
        await api.FFmpegKitConfig.enableLogs().catch(() => {});
        await api.FFmpegKitConfig.enableStatistics().catch(() => {});
        await api.FFmpegKitConfig.uninit().catch(() => {});
        if (originalWorker === undefined) delete globalThis.Worker;
        else globalThis.Worker = originalWorker;
    }
});

test('worker fatal after async execution fails session and dispatches completion callback', async () => {
    const originalWorker = globalThis.Worker;
    const failureMessage = 'fatal after async start';
    let completeCallbackCalls = 0;
    let completeCallbackResolve;
    const completeCallbackPromise = new Promise((resolve, reject) => {
        const timeout = setTimeout(() => {
            reject(new Error('Timed out waiting for async completion callback'));
        }, 1000);
        completeCallbackResolve = (session) => {
            clearTimeout(timeout);
            resolve(session);
        };
    });

    class MockWorker {
        constructor() {
            setTimeout(() => {
                this.onmessage?.({
                    data: {
                        type: 0,
                        version: 'test',
                        ffmpegVersion: 'test',
                        buildDate: 'test',
                    },
                });
            }, 0);
        }

        postMessage({id, op, args = {}}) {
            if (op === 'createSession') {
                setTimeout(() => {
                    this.onmessage?.({
                        data: {
                            id,
                            type: 2,
                            result: {
                                sessionId: 41,
                                type: 1,
                                state: api.SessionState.CREATED,
                                command: args.arguments.join(' '),
                                arguments: args.arguments,
                                logEntries: [],
                                statistics: [],
                            },
                        },
                    });
                }, 0);
                return;
            }

            if (op === 'executeAsync') {
                setTimeout(() => {
                    this.onmessage?.({
                        data: {
                            type: 1,
                            message: failureMessage,
                        },
                    });
                }, 0);
                return;
            }

            setTimeout(() => {
                this.onmessage?.({
                    data: {
                        id,
                        type: 2,
                        result: {ok: true},
                    },
                });
            }, 0);
        }

        terminate() {
            this.onmessage = null;
        }
    }

    globalThis.Worker = MockWorker;
    try {
        const session = await api.FFmpegSession.create(['-version'], (completedSession) => {
            completeCallbackCalls += 1;
            completeCallbackResolve(completedSession);
        });

        await api.FFmpegKitConfig.asyncFFmpegExecute(session);

        const completedSession = await completeCallbackPromise;
        assert.equal(completedSession, session);
        assert.equal(completeCallbackCalls, 1);
        assert.equal(session.getState(), api.SessionState.FAILED);
        assert.equal(session.getFailStackTrace(), failureMessage);

        // Native AbstractSession::fail() stamps the end time and leaves the return
        // code unset, so a failed session must not report a stale/absent end time.
        //
        // The start time was stamped when the request was posted, as native does in
        // startRunning(). That is what makes the duration meaningful here: native's
        // getDuration() returns 0 unless BOTH stamps are set, so without the start
        // time a failed session would report a real end time next to a zero duration.
        assert.ok(session.getStartTime() instanceof Date);
        assert.ok(session.getEndTime() instanceof Date);
        assert.equal(session.getReturnCode(), null);
        assert.equal(
            session.getDuration(),
            session.getEndTime().getTime() - session.getStartTime().getTime()
        );
        assert.ok(session.getDuration() >= 0);
    } finally {
        await api.FFmpegKitConfig.uninit().catch(() => {});
        if (originalWorker === undefined) delete globalThis.Worker;
        else globalThis.Worker = originalWorker;
    }
});

// A worker that dies takes its session id space with it: the replacement starts a fresh
// wasm module numbering from the beginning. Everything this binding recorded against an
// id from the dead runtime - the per-session callbacks, the log redirection strategy -
// must therefore be dropped, or it is silently applied to an unrelated session that
// happens to be handed the same id. uninit() has always done this; losing the runtime
// involuntarily (MSG_FATAL, worker onerror) has to do it too, which is why every path
// goes through _failRuntime().
test('losing the runtime drops per-session state so a reused id starts clean', async () => {
    const originalWorker = globalThis.Worker;
    let nextSessionId = 1;
    let worker = null;

    class MockWorker {
        constructor() {
            worker = this;
            setTimeout(() => {
                this.onmessage?.({
                    data: {
                        type: 0,
                        version: 'test',
                        ffmpegVersion: 'test',
                        buildDate: 'test',
                    },
                });
            }, 0);
        }

        postMessage({id, op, args = {}}) {
            const result =
                op === 'createSession'
                    ? {
                          sessionId: nextSessionId++,
                          type: 1,
                          state: api.SessionState.CREATED,
                          command: args.arguments.join(' '),
                          arguments: args.arguments,
                          logEntries: [],
                          statistics: [],
                      }
                    : {ok: true};
            setTimeout(() => this.onmessage?.({data: {id, type: 2, result}}), 0);
        }

        die() {
            this.onmessage?.({data: {type: 1, message: 'worker died'}});
        }

        terminate() {
            this.onmessage = null;
        }
    }

    globalThis.Worker = MockWorker;
    api.FFmpegKitConfig.enableLogCallback(null);
    const staleSessionLogs = [];
    try {
        const doomed = await api.FFmpegSession.create(
            ['-version'],
            null,
            (log) => staleSessionLogs.push(log.getMessage()),
            null,
            api.LogRedirectionStrategy.NEVER_PRINT_LOGS
        );
        assert.equal(doomed.getSessionId(), 1);

        worker.die();

        // The replacement module numbers its sessions from the beginning again. This one
        // registers no callback and no strategy of its own, through the low-level create
        // that does not clear the registry either.
        nextSessionId = 1;
        const fresh = await api.AbstractSession.createFFmpegSession(['-version']);
        assert.equal(fresh.getSessionId(), 1);
        assert.equal(fresh.getLogCallback(), null);
        assert.equal(
            fresh.getLogRedirectionStrategy(),
            api.FFmpegKitConfig.getLogRedirectionStrategy()
        );

        // A log for it must reach neither the dead session's callback nor be muted by the
        // dead session's NEVER_PRINT_LOGS.
        const printed = [];
        const originalConsoleLog = console.log;
        console.log = (...consoleArguments) => printed.push(consoleArguments.join(' '));
        try {
            worker.onmessage?.({
                data: {
                    type: 4,
                    log: {sessionId: 1, level: api.Level.AV_LOG_INFO, message: 'fresh'},
                },
            });
        } finally {
            console.log = originalConsoleLog;
        }

        assert.deepEqual(staleSessionLogs, []);
        assert.deepEqual(printed, ['fresh']);
    } finally {
        await api.FFmpegKitConfig.uninit().catch(() => {});
        if (originalWorker === undefined) delete globalThis.Worker;
        else globalThis.Worker = originalWorker;
    }
});

// Same rule as the getAll* family: a getter must never boot a wasm module. Booting one to
// answer this question is also self-defeating - a module that did not exist a moment ago
// cannot be holding any of this session's messages. Flutter's AbstractSession equivalent
// likewise skips the init() its FFmpegKitConfig counterpart performs.
test('thereAreAsynchronousMessagesInTransmit never starts a runtime', async () => {
    const originalWorker = globalThis.Worker;
    let workersConstructed = 0;

    class MockWorker {
        constructor() {
            workersConstructed += 1;
            setTimeout(() => {
                this.onmessage?.({
                    data: {
                        type: 0,
                        version: 'test',
                        ffmpegVersion: 'test',
                        buildDate: 'test',
                    },
                });
            }, 0);
        }

        postMessage({id}) {
            setTimeout(() => {
                this.onmessage?.({data: {id, type: 2, result: {count: 0}}});
            }, 0);
        }

        terminate() {
            this.onmessage = null;
        }
    }

    await api.FFmpegKitConfig.uninit().catch(() => {});
    globalThis.Worker = MockWorker;
    try {
        const session = api.AbstractSession.createFFmpegSessionFromMap({
            sessionId: 91,
            command: '-version',
        });

        assert.equal(await session.thereAreAsynchronousMessagesInTransmit(), false);
        assert.equal(workersConstructed, 0, 'a session getter must not boot a wasm module');
    } finally {
        await api.FFmpegKitConfig.uninit().catch(() => {});
        if (originalWorker === undefined) delete globalThis.Worker;
        else globalThis.Worker = originalWorker;
    }
});

// The filter under test is the one shared by the Flutter and React Native plugins
// (_processLogCallbackEvent / processLogCallbackEvent): drop a log whose level is
// above the active level, and always forward AV_LOG_STDERR - even at AV_LOG_QUIET.
// It only works if levels arrive as plain numbers, which is why the level boundary
// carries ints on both sides (see the log-level note in FFmpegKitBindings.cpp).
test('log level filtering matches the Flutter and React Native plugins', async () => {
    const originalWorker = globalThis.Worker;
    const deliveredLevels = [];
    let batchDelivered;

    function nextBatch() {
        let settle;
        const promise = new Promise((resolve, reject) => {
            const timeout = setTimeout(() => {
                reject(new Error('Timed out waiting for the log batch'));
            }, 1000);
            settle = () => {
                clearTimeout(timeout);
                resolve();
            };
        });
        batchDelivered = settle;
        return promise;
    }

    class MockWorker {
        constructor() {
            setTimeout(() => {
                this.onmessage?.({
                    data: {
                        type: 0,
                        version: 'test',
                        ffmpegVersion: 'test',
                        buildDate: 'test',
                        logLevel: api.Level.AV_LOG_TRACE,
                    },
                });
            }, 0);
        }

        postMessage({id, op, args = {}}) {
            if (op === 'createSession') {
                setTimeout(() => {
                    this.onmessage?.({
                        data: {
                            id,
                            type: 2,
                            result: {
                                sessionId: 51,
                                type: 1,
                                state: api.SessionState.CREATED,
                                command: args.arguments.join(' '),
                                arguments: args.arguments,
                                logEntries: [],
                                statistics: [],
                            },
                        },
                    });
                }, 0);
                return;
            }

            if (op === 'executeAsync') {
                setTimeout(() => {
                    for (const level of [
                        api.Level.AV_LOG_INFO,
                        api.Level.AV_LOG_DEBUG,
                        api.Level.AV_LOG_STDERR,
                    ]) {
                        this.onmessage?.({
                            data: {
                                id,
                                type: 4,
                                log: {
                                    sessionId: args.sessionId,
                                    level,
                                    message: `level ${level}\n`,
                                },
                            },
                        });
                    }
                    batchDelivered();
                }, 0);
                return;
            }

            setTimeout(() => {
                this.onmessage?.({data: {id, type: 2, result: {ok: true}}});
            }, 0);
        }

        terminate() {
            this.onmessage = null;
        }
    }

    globalThis.Worker = MockWorker;
    try {
        api.FFmpegKitConfig.enableLogCallback((log) => deliveredLevels.push(log.getLevel()));

        await api.FFmpegKitConfig.setLogLevel(api.Level.AV_LOG_INFO);
        let batch = nextBatch();
        const session = await api.FFmpegSession.create(['-version']);
        await api.FFmpegKitConfig.asyncFFmpegExecute(session);
        await batch;

        assert.deepEqual(deliveredLevels, [api.Level.AV_LOG_INFO, api.Level.AV_LOG_STDERR]);

        // The filter decides who is TOLD about a log, not whether the session has it.
        // Native drops what its own level excludes before it ever reaches addLog()
        // (process_log() in FFmpegKitConfig.cpp returns ahead of session->addLog()), so
        // every event that gets this far is one the native session already holds - and
        // the Flutter/React Native level re-check cannot take an entry back out of
        // getAllLogs() either. The live mirror has to agree, or getLogs() would
        // contradict getAllLogs() mid-run and then contradict itself once the terminal
        // result replaces the buffer with the full native record. AV_LOG_DEBUG is here
        // even though no callback was told about it.
        assert.deepEqual(
            session.getLogs().map((log) => log.getLevel()),
            [api.Level.AV_LOG_INFO, api.Level.AV_LOG_DEBUG, api.Level.AV_LOG_STDERR]
        );

        // At AV_LOG_QUIET nothing but AV_LOG_STDERR is forwarded.
        deliveredLevels.length = 0;
        await api.FFmpegKitConfig.setLogLevel(api.Level.AV_LOG_QUIET);
        batch = nextBatch();
        await api.FFmpegKitConfig.asyncFFmpegExecute(await api.FFmpegSession.create(['-version']));
        await batch;

        assert.deepEqual(deliveredLevels, [api.Level.AV_LOG_STDERR]);
    } finally {
        api.FFmpegKitConfig.enableLogCallback(null);
        await api.FFmpegKitConfig.setLogLevel(api.Level.AV_LOG_TRACE).catch(() => {});
        await api.FFmpegKitConfig.uninit().catch(() => {});
        if (originalWorker === undefined) delete globalThis.Worker;
        else globalThis.Worker = originalWorker;
    }
});

test('worker uses output-buffer capacity overload only when both bounds are provided', () => {
    const workerSource = readFileSync(path.join(WEB_ROOT, 'js/ffmpegkit.worker.js'), 'utf8');

    assert.match(
        workerSource,
        /args\.initialCapacity != null && args\.maxCapacity != null/
    );
    assert.doesNotMatch(
        workerSource,
        /args\.initialCapacity != null \|\| args\.maxCapacity != null/
    );
});

test('Level and ReturnCode value semantics are unchanged', () => {
    const {Level, ReturnCode} = api;
    assert.equal(Level.levelToString(Level.AV_LOG_ERROR), 'ERROR');
    assert.equal(Level.levelToString(Level.AV_LOG_QUIET), '');
    assert.equal(Level.levelToString(Level.AV_LOG_STDERR), 'STDERR');

    // Levels are plain numbers, as on the native platforms - there is no Level instance.
    assert.equal(Level.AV_LOG_INFO, 32);
    assert.equal(typeof Level.AV_LOG_INFO, 'number');

    assert.equal(ReturnCode.isSuccess(new ReturnCode(0)), true);
    assert.equal(ReturnCode.isCancel(new ReturnCode(255)), true);
    assert.equal(new ReturnCode(1).isValueError(), true);
    assert.equal(ReturnCode.isSuccess(null), false);
});

test('MediaInformation projects streams and chapters', () => {
    const media = new api.MediaInformation({
        format: {filename: 'a.mp4', format_name: 'mov', duration: '12.5'},
        streams: [{index: 0, codec_type: 'video', width: 1920, height: 1080}],
        chapters: [{id: 1, start_time: '0.000'}],
    });

    assert.equal(media.getFilename(), 'a.mp4');
    assert.equal(media.getFormat(), 'mov');
    assert.equal(media.getDuration(), '12.5');

    const [stream] = media.getStreams();
    assert.ok(stream instanceof api.StreamInformation);
    assert.equal(stream.getType(), 'video');
    assert.equal(stream.getWidth(), 1920);

    const [chapter] = media.getChapters();
    assert.ok(chapter instanceof api.Chapter);
    assert.equal(chapter.getId(), 1);

    assert.deepEqual(new api.MediaInformation(null).getStreams(), []);
});

test('a history snapshot never double-records live logs of a running session', async () => {
    const {getFactory} = await import('../js/src/FFmpegKitFactory.js');
    const factory = getFactory();
    const line = 'frame=1\n';

    const session = api.AbstractSession.createFFmpegSessionFromMap({
        sessionId: 91,
        command: '-i a b',
    });
    factory._indexSession(session);

    // A log event has been drained by the worker and delivered.
    session._addLog(new api.Log(91, api.Level.AV_LOG_INFO, line));

    // A getSessions() snapshot taken while the run is in flight already contains it.
    const snapshot = {
        sessionId: 91,
        type: 1,
        command: '-i a b',
        state: 1,
        logs: line,
        logEntries: [{sessionId: 91, level: api.Level.AV_LOG_INFO, message: line}],
        statistics: [{sessionId: 91, frame: 1}],
    };

    try {
        // While the execution is pending, the snapshot must not touch the live buffers.
        factory._pending.set(-1, {resolve() {}, reject() {}, session});
        assert.equal(factory._mapToSession(snapshot), session);
        assert.equal(session.getLogsAsString(), line);
        assert.equal(session.getLogs().length, 1);
        assert.equal(session.getStatistics().length, 0);

        // Once nothing is in flight the snapshot is authoritative again.
        factory._pending.delete(-1);
        factory._mapToSession(snapshot);
        assert.equal(session.getLogs().length, 1);
        assert.equal(session.getStatistics().length, 1);
    } finally {
        factory._pending.delete(-1);
        factory._sessionsById.delete(91);
    }
});

// A worker that boots, records every op it receives, and answers each one from `replies`
// (an op -> result map). Unknown ops get `{ok: true}`, matching the real worker's habit
// of acknowledging side-effect ops.
function mockWorkerClass(replies = {}, received = []) {
    return class MockWorker {
        constructor() {
            setTimeout(() => {
                this.onmessage?.({
                    data: {
                        type: 0,
                        version: 'test',
                        ffmpegVersion: 'test',
                        buildDate: 'test',
                        logLevel: api.Level.AV_LOG_TRACE,
                    },
                });
            }, 0);
        }

        postMessage({id, op, args = {}}) {
            received.push({op, args});
            const reply = replies[op];
            setTimeout(() => {
                this.onmessage?.({
                    data: {
                        id,
                        type: 2,
                        result: typeof reply === 'function' ? reply(args) : reply ?? {ok: true},
                    },
                });
            }, 0);
        }

        terminate() {
            this.onmessage = null;
        }
    };
}

async function withMockWorker(replies, body) {
    const received = [];
    const originalWorker = globalThis.Worker;
    globalThis.Worker = mockWorkerClass(replies, received);
    try {
        return await body(received);
    } finally {
        await api.FFmpegKitConfig.uninit().catch(() => {});
        if (originalWorker === undefined) delete globalThis.Worker;
        else globalThis.Worker = originalWorker;
    }
}

test('the getAll* family forwards waitTimeout and reports what native returns', async () => {
    const line = 'native log\n';
    const replies = {
        getAllLogs: {
            logEntries: [{sessionId: 43, level: api.Level.AV_LOG_INFO, message: line}],
        },
        getAllLogsAsString: {logs: line},
        getAllStatistics: {statistics: [{sessionId: 43, frame: 12}]},
    };

    await withMockWorker(replies, async (received) => {
        // The getAll* family reads through the runtime; it never starts one.
        await api.FFmpegKitConfig.init();

        const session = api.AbstractSession.createFFmpegSessionFromMap({
            sessionId: 43,
            command: '-i a b',
        });

        // Nothing was delivered live, so only the native call can produce a record -
        // this is what Flutter/RN get from waitForAsynchronousMessagesInTransmit.
        assert.deepEqual(session.getLogs(), []);
        assert.equal(session.getLogsAsString(), '');
        assert.deepEqual(session.getStatistics(), []);

        const logs = await session.getAllLogs();
        assert.equal(logs.length, 1);
        assert.ok(logs[0] instanceof api.Log);
        assert.equal(logs[0].getMessage(), line);
        assert.equal(await session.getAllLogsAsString(), line);
        assert.equal(await session.getOutput(), line);

        const statistics = await session.getAllStatistics();
        assert.equal(statistics.length, 1);
        assert.ok(statistics[0] instanceof api.Statistics);
        assert.equal(statistics[0].getVideoFrameNumber(), 12);

        // waitTimeout travels to the worker when given, and is omitted otherwise so the
        // native AbstractSession default applies.
        assert.deepEqual(
            received.filter(({op}) => op === 'getAllLogs').pop().args,
            {sessionId: 43}
        );
        await session.getAllLogs(250);
        assert.deepEqual(
            received.filter(({op}) => op === 'getAllLogs').pop().args,
            {sessionId: 43, waitTimeout: 250}
        );
        await session.getAllLogsAsString(10);
        assert.deepEqual(
            received.filter(({op}) => op === 'getAllLogsAsString').pop().args,
            {sessionId: 43, waitTimeout: 10}
        );
        await session.getAllStatistics(10);
        assert.deepEqual(
            received.filter(({op}) => op === 'getAllStatistics').pop().args,
            {sessionId: 43, waitTimeout: 10}
        );
    });
});

test('the getAll* family falls back to the held record with no runtime', async () => {
    const session = api.AbstractSession.createFFmpegSessionFromMap({
        sessionId: 45,
        command: '-i a b',
    });
    session._apply({
        logs: 'held log\n',
        logEntries: [{sessionId: 45, level: api.Level.AV_LOG_INFO, message: 'held log\n'}],
        statistics: [{sessionId: 45, frame: 3}],
    });

    // No worker is running, so the getters must answer from what they hold rather than
    // rejecting or starting a runtime of their own.
    assert.equal(await session.getAllLogsAsString(), 'held log\n');
    assert.equal(await session.getOutput(), 'held log\n');
    assert.equal((await session.getAllLogs()).length, 1);
    assert.equal((await session.getAllStatistics()).length, 1);
});

test('getSessionsByState rejects a state that is not a SessionState', async () => {
    await withMockWorker({getSessionsByState: {sessions: []}}, async (received) => {
        for (const state of [99, -1, 'completed', '3', null, undefined]) {
            await assert.rejects(
                () => api.FFmpegKitConfig.getSessionsByState(state),
                /Unknown session state/
            );
        }
        assert.deepEqual(received.filter(({op}) => op === 'getSessionsByState'), []);

        assert.deepEqual(
            await api.FFmpegKitConfig.getSessionsByState(api.SessionState.COMPLETED),
            []
        );
    });
});

test('sessions rebuilt from a map restore their recorded log redirection strategy', async () => {
    const {getFactory} = await import('../js/src/FFmpegKitFactory.js');
    const factory = getFactory();
    const {ALWAYS_PRINT_LOGS, NEVER_PRINT_LOGS} = api.LogRedirectionStrategy;

    factory._logRedirectionStrategiesById.set(61, ALWAYS_PRINT_LOGS);
    factory._logRedirectionStrategiesById.set(62, ALWAYS_PRINT_LOGS);
    try {
        assert.equal(
            api.AbstractSession.createFFmpegSessionFromMap({
                sessionId: 61,
                command: '-i a b',
            }).getLogRedirectionStrategy(),
            ALWAYS_PRINT_LOGS
        );
        assert.equal(
            api.AbstractSession.createFFprobeSessionFromMap({
                sessionId: 61,
                command: '-i a',
            }).getLogRedirectionStrategy(),
            ALWAYS_PRINT_LOGS
        );

        // A session with no recorded strategy reports none, as on Flutter.
        assert.equal(
            api.AbstractSession.createFFmpegSessionFromMap({
                sessionId: 63,
                command: '-i a b',
            }).getLogRedirectionStrategy(),
            null
        );

        // Media-information sessions are always NEVER_PRINT_LOGS and never inherit.
        assert.equal(
            api.AbstractSession.createMediaInformationSessionFromMap({
                sessionId: 62,
                command: '-i a',
            }).getLogRedirectionStrategy(),
            NEVER_PRINT_LOGS
        );
    } finally {
        factory._logRedirectionStrategiesById.delete(61);
        factory._logRedirectionStrategiesById.delete(62);
    }
});

test('per-session callbacks are registry-backed and dropped with the session', async () => {
    const SESSION_ID = 71;
    const sessionMap = {
        type: 1,
        sessionId: SESSION_ID,
        command: '-i a b',
        arguments: ['-i', 'a', 'b'],
        state: api.SessionState.CREATED,
    };

    await withMockWorker({createSession: sessionMap}, async () => {
        const completeCallback = () => {};
        const logCallback = () => {};
        const statisticsCallback = () => {};

        const created = await api.FFmpegSession.create(
            ['-i', 'a', 'b'],
            completeCallback,
            logCallback,
            statisticsCallback
        );

        assert.equal(created.getCompleteCallback(), completeCallback);
        assert.equal(created.getLogCallback(), logCallback);
        assert.equal(created.getStatisticsCallback(), statisticsCallback);

        // The callbacks are keyed on the session id in FFmpegKitFactory, not stored on the
        // session object - so a fresh wrapper rebuilt from a native history map reports the
        // same callbacks, exactly as Flutter's and React Native's *FromMap statics do.
        const rebuilt = api.AbstractSession.createFFmpegSessionFromMap(sessionMap);
        assert.notEqual(rebuilt, created);
        assert.equal(rebuilt.getCompleteCallback(), completeCallback);
        assert.equal(rebuilt.getLogCallback(), logCallback);
        assert.equal(rebuilt.getStatisticsCallback(), statisticsCallback);

        // deleteSession() drops them the way FFmpegKitFactory.deleteSession() does on the
        // other platforms: every wrapper for that id reports null, whether the app already
        // held it or builds it afterwards.
        await api.FFmpegKitConfig.deleteSession(SESSION_ID);

        for (const dropped of [
            created,
            rebuilt,
            api.AbstractSession.createFFmpegSessionFromMap(sessionMap),
        ]) {
            assert.equal(dropped.getCompleteCallback(), null);
            assert.equal(dropped.getLogCallback(), null);
            assert.equal(dropped.getStatisticsCallback(), null);
        }

        // clearSessions() is the bulk form of the same thing.
        const recreated = await api.FFmpegSession.create(['-i', 'a', 'b'], completeCallback);
        assert.equal(recreated.getCompleteCallback(), completeCallback);

        await api.FFmpegKitConfig.clearSessions();
        assert.equal(recreated.getCompleteCallback(), null);
    });
});

// A wasm module built before the getArguments binding makes the worker omit `arguments`
// from the session map. Both consumers of that key treat an empty array as a real answer -
// _apply() overwrites when it IS an array, _applySessionMap() reparses only when it is NOT
// one - so sending [] would wipe the arguments instead of triggering the fallback the
// version guard exists for. The create paths carry the caller's own array for the same
// reason, exactly as Flutter's AbstractSession.create*Session() does.
test('a session map without arguments falls back instead of reporting none', async () => {
    // History read: no `arguments` key, so the command string is reparsed.
    const rebuilt = api.AbstractSession.createFFmpegSessionFromMap({
        sessionId: 81,
        command: "-i 'my file.mp4' out.mp4",
    });
    assert.deepEqual(rebuilt.getArguments(), ['-i', 'my file.mp4', 'out.mp4']);

    // An explicitly empty array is a real answer and must be honoured, not reparsed.
    const empty = api.AbstractSession.createFFmpegSessionFromMap({
        sessionId: 82,
        command: '-i a b',
        arguments: [],
    });
    assert.deepEqual(empty.getArguments(), []);

    // Create path: it calls _apply(), which has no reparse fallback, so the caller's array
    // is carried through rather than read back out of the map. Reparsing could not recover
    // this one anyway - the argument contains a space.
    const commandArguments = ['-i', 'my file.mp4', 'out.mp4'];
    await withMockWorker(
        {createSession: {type: 1, sessionId: 83, command: "-i 'my file.mp4' out.mp4"}},
        async () => {
            const created = await api.FFmpegSession.create(commandArguments);
            assert.deepEqual(created.getArguments(), commandArguments);
        }
    );
});

test('in-memory I/O factories take positional parameters like the React Native plugin', async () => {
    await withMockWorker({ioCreate: {handle: 1, url: 'ffkitmem:1'}}, async (received) => {
        await api.FFmpegKitOutputBuffer.create('mp4', 1024, 4096);
        await api.FFmpegKitStreamInput.create('mp4', 64);
        await api.FFmpegKitStreamOutput.create('mkv', 128);
        await api.FFmpegKitOutputBuffer.create();

        const creates = received.filter(({op}) => op === 'ioCreate').map(({args}) => args);
        assert.deepEqual(creates[0], {
            kind: 'outputBuffer',
            extension: 'mp4',
            initialCapacity: 1024,
            maxCapacity: 4096,
            data: undefined,
        });
        assert.deepEqual(creates[1], {
            kind: 'streamInput',
            extension: 'mp4',
            capacity: 64,
            data: undefined,
        });
        assert.deepEqual(creates[2], {
            kind: 'streamOutput',
            extension: 'mkv',
            capacity: 128,
            data: undefined,
        });
        assert.deepEqual(creates[3], {
            kind: 'outputBuffer',
            extension: '',
            initialCapacity: null,
            maxCapacity: null,
            data: undefined,
        });
    });
});

test('a worker that cannot be constructed rejects instead of wedging the runtime', async () => {
    const originalWorker = globalThis.Worker;
    globalThis.Worker = class {
        constructor() {
            throw new Error('Worker blocked by policy');
        }
    };

    try {
        // Without this the failed attempt would be cached as a promise that never
        // settles, and every later call would await it forever.
        await assert.rejects(() => api.FFmpegKitConfig.init(), /Worker blocked by policy/);
        await assert.rejects(() => api.FFmpegKitConfig.getSessionHistorySize(), /Worker blocked/);

        // The failed attempt must not be cached: a working environment recovers.
        globalThis.Worker = mockWorkerClass({getSessionHistorySize: {size: 10}});
        assert.equal(await api.FFmpegKitConfig.getSessionHistorySize(), 10);
    } finally {
        await api.FFmpegKitConfig.uninit().catch(() => {});
        if (originalWorker === undefined) delete globalThis.Worker;
        else globalThis.Worker = originalWorker;
    }
});
