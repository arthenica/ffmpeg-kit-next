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

#import "FFmpegKitStreamOutput.h"
#import "FFmpegKitConfig.h"
#import "FFmpegKitInputBuffer.h"

static const long DefaultStreamCapacity = 1024 * 1024;
static const int StreamTypeOutput = 2;

@interface FFmpegKitConfig (FFmpegKitStreamOutputSupport)
+ (long)registerFFmpegKitStream:(long)capacity type:(int)type;
+ (NSData *)readFFmpegKitStream:(long)streamId
                       maxBytes:(int)maxBytes
                        timeout:(int)timeoutMs;
+ (void)unregisterFFmpegKitStream:(long)streamId;
@end

@interface FFmpegKitInputBuffer (FFmpegKitUrlSupport)
+ (NSString *)urlWithProtocol:(NSString *)protocol
                   resourceId:(long)resourceId
                    extension:(NSString *)extension;
@end

@interface FFmpegKitStreamOutput ()
- (instancetype)initWithResourceId:(long)resourceId
                         extension:(NSString *)extension;
- (void)ensureOpen;
@end

@implementation FFmpegKitStreamOutput {
    long _resourceId;
    NSString *_url;
    BOOL _closed;
}

+ (instancetype)create:(NSString *)extension {
    return [FFmpegKitStreamOutput create:extension
                                capacity:DefaultStreamCapacity];
}

+ (instancetype)create:(NSString *)extension capacity:(long)capacity {
    if (capacity <= 0) {
        [NSException raise:NSInvalidArgumentException
                    format:@"capacity must be positive"];
    }

    long resourceId =
        [FFmpegKitConfig registerFFmpegKitStream:capacity type:StreamTypeOutput];
    if (resourceId == 0) {
        [NSException raise:NSInternalInconsistencyException
                    format:@"Failed to register FFmpegKit output stream."];
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
    }

    return self;
}

- (NSString *)getUrl {
    [self ensureOpen];
    return _url;
}

- (NSData *)read:(int)maxBytes {
    return [self read:maxBytes timeout:-1];
}

- (NSData *)read:(int)maxBytes timeout:(int)timeoutMs {
    [self ensureOpen];
    if (maxBytes < 0) {
        [NSException raise:NSInvalidArgumentException
                    format:@"maxBytes must not be negative"];
    }

    return [FFmpegKitConfig readFFmpegKitStream:_resourceId
                                       maxBytes:maxBytes
                                        timeout:timeoutMs];
}

- (void)close {
    if (!_closed) {
        [FFmpegKitConfig unregisterFFmpegKitStream:_resourceId];
        _closed = YES;
    }
}

- (void)ensureOpen {
    if (_closed) {
        [NSException raise:NSInternalInconsistencyException
                    format:@"FFmpegKit output stream is closed."];
    }
}

@end
