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

#import "FFmpegKitOutputBuffer.h"
#import "FFmpegKitConfig.h"
#import "FFmpegKitInputBuffer.h"

static const long DefaultInitialCapacity = 4096;

@interface FFmpegKitConfig (FFmpegKitOutputBufferSupport)
+ (long)registerFFmpegKitOutputBuffer:(long)initialCapacity
                          maxCapacity:(long)maxCapacity;
+ (long)getFFmpegKitBufferSize:(long)bufferId;
+ (NSData *)getFFmpegKitOutputBuffer:(long)bufferId;
+ (NSData *)getFFmpegKitOutputBufferNoCopy:(long)bufferId;
+ (void)unregisterFFmpegKitBuffer:(long)bufferId;
@end

@interface FFmpegKitInputBuffer (FFmpegKitUrlSupport)
+ (NSString *)urlWithProtocol:(NSString *)protocol
                   resourceId:(long)resourceId
                    extension:(NSString *)extension;
@end

@interface FFmpegKitOutputBuffer ()
- (instancetype)initWithResourceId:(long)resourceId
                         extension:(NSString *)extension;
- (void)ensureOpen;
@end

@implementation FFmpegKitOutputBuffer {
    long _resourceId;
    NSString *_url;
    BOOL _closed;
}

+ (instancetype)create:(NSString *)extension {
    return [FFmpegKitOutputBuffer create:extension
                         initialCapacity:DefaultInitialCapacity
                             maxCapacity:0];
}

+ (instancetype)create:(NSString *)extension
       initialCapacity:(long)initialCapacity
           maxCapacity:(long)maxCapacity {
    if (initialCapacity < 0 || maxCapacity < 0) {
        [NSException raise:NSInvalidArgumentException
                    format:@"capacities must not be negative"];
    }

    long resourceId =
        [FFmpegKitConfig registerFFmpegKitOutputBuffer:initialCapacity
                                           maxCapacity:maxCapacity];
    if (resourceId == 0) {
        [NSException raise:NSInternalInconsistencyException
                    format:@"Failed to register FFmpegKit output buffer."];
    }

    return [[self alloc] initWithResourceId:resourceId extension:extension];
}

- (instancetype)initWithResourceId:(long)resourceId
                         extension:(NSString *)extension {
    self = [super init];
    if (self) {
        _resourceId = resourceId;
        _url = [FFmpegKitInputBuffer urlWithProtocol:@"ffkitmem"
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

- (long)getSize {
    [self ensureOpen];
    return [FFmpegKitConfig getFFmpegKitBufferSize:_resourceId];
}

- (NSData *)toData {
    [self ensureOpen];
    NSData *output = [FFmpegKitConfig getFFmpegKitOutputBuffer:_resourceId];
    if (output == nil) {
        [NSException raise:NSInternalInconsistencyException
                    format:@"Failed to read FFmpegKit output buffer."];
    }

    return output;
}

- (NSData *)asDataNoCopy {
    [self ensureOpen];
    NSData *output =
        [FFmpegKitConfig getFFmpegKitOutputBufferNoCopy:_resourceId];
    return output == nil ? [NSData data] : output;
}

- (void)close {
    if (!_closed) {
        [FFmpegKitConfig unregisterFFmpegKitBuffer:_resourceId];
        _closed = YES;
    }
}

- (void)ensureOpen {
    if (_closed) {
        [NSException raise:NSInternalInconsistencyException
                    format:@"FFmpegKit output buffer is closed."];
    }
}

@end
