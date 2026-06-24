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

#import "FFmpegKitStreamInput.h"
#import "FFmpegKitConfig.h"
#import "FFmpegKitInputBuffer.h"

static const long DefaultStreamCapacity = 1024 * 1024;
static const int StreamTypeInput = 1;

@interface FFmpegKitConfig (FFmpegKitStreamInputSupport)
+ (long)registerFFmpegKitStream:(long)capacity type:(int)type;
+ (int)writeFFmpegKitStream:(long)streamId
                       data:(NSData *)data
                     offset:(NSUInteger)offset
                     length:(NSUInteger)length
                    timeout:(int)timeoutMs;
+ (void)closeFFmpegKitStreamInput:(long)streamId;
+ (void)unregisterFFmpegKitStream:(long)streamId;
@end

@interface FFmpegKitInputBuffer (FFmpegKitUrlSupport)
+ (NSString *)urlWithProtocol:(NSString *)protocol
                   resourceId:(long)resourceId
                    extension:(NSString *)extension;
@end

@interface FFmpegKitStreamInput ()
- (instancetype)initWithResourceId:(long)resourceId
                         extension:(NSString *)extension;
- (void)ensureOpen;
@end

@implementation FFmpegKitStreamInput {
    long _resourceId;
    NSString *_url;
    BOOL _closed;
    BOOL _inputClosed;
}

+ (instancetype)create:(NSString *)extension {
    return [FFmpegKitStreamInput create:extension
                               capacity:DefaultStreamCapacity];
}

+ (instancetype)create:(NSString *)extension capacity:(long)capacity {
    if (capacity <= 0) {
        [NSException raise:NSInvalidArgumentException
                    format:@"capacity must be positive"];
    }

    long resourceId = [FFmpegKitConfig registerFFmpegKitStream:capacity
                                                          type:StreamTypeInput];
    if (resourceId == 0) {
        [NSException raise:NSInternalInconsistencyException
                    format:@"Failed to register FFmpegKit input stream."];
    }

    return [[self alloc] initWithResourceId:resourceId extension:extension];
}

- (instancetype)initWithResourceId:(long)resourceId
                         extension:(NSString *)extension {
    self = [super init];
    if (self) {
        _resourceId = resourceId;
        _url = [FFmpegKitInputBuffer urlWithProtocol:@"ffkitstream"
                                          resourceId:resourceId
                                           extension:extension];
        _closed = NO;
        _inputClosed = NO;
    }

    return self;
}

- (NSString *)getUrl {
    [self ensureOpen];
    return _url;
}

- (int)write:(NSData *)data {
    return [self write:data timeout:-1];
}

- (int)write:(NSData *)data timeout:(int)timeoutMs {
    return [self write:data
                offset:0
                length:data == nil ? 0 : [data length]
               timeout:timeoutMs];
}

- (int)write:(NSData *)data
      offset:(NSUInteger)offset
      length:(NSUInteger)length
     timeout:(int)timeoutMs {
    [self ensureOpen];
    if (_inputClosed) {
        [NSException raise:NSInternalInconsistencyException
                    format:@"FFmpegKit input stream is closed for writing."];
    }
    if (data == nil) {
        [NSException raise:NSInvalidArgumentException
                    format:@"data must not be nil"];
    }
    if (offset > [data length] || length > ([data length] - offset)) {
        [NSException raise:NSInvalidArgumentException
                    format:@"offset and length must fit inside data"];
    }

    int written = [FFmpegKitConfig writeFFmpegKitStream:_resourceId
                                                   data:data
                                                 offset:offset
                                                 length:length
                                                timeout:timeoutMs];
    if (written < 0) {
        [NSException raise:NSInternalInconsistencyException
                    format:@"Failed to write FFmpegKit input stream: %d.",
                           written];
    }

    return written;
}

- (void)closeInput {
    if (!_closed && !_inputClosed) {
        [FFmpegKitConfig closeFFmpegKitStreamInput:_resourceId];
        _inputClosed = YES;
    }
}

- (void)close {
    if (!_closed) {
        [self closeInput];
        [FFmpegKitConfig unregisterFFmpegKitStream:_resourceId];
        _closed = YES;
    }
}

- (void)ensureOpen {
    if (_closed) {
        [NSException raise:NSInternalInconsistencyException
                    format:@"FFmpegKit input stream is closed."];
    }
}

@end
