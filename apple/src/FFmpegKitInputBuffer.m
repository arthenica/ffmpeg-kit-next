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

#import "FFmpegKitInputBuffer.h"
#import "FFmpegKitConfig.h"

@interface FFmpegKitConfig (FFmpegKitBufferSupport)
+ (long)registerFFmpegKitInputBuffer:(NSData *)data;
+ (long)registerFFmpegKitInputBufferWithBytes:(const void *)bytes
                                       length:(NSUInteger)length;
+ (long)getFFmpegKitBufferSize:(long)bufferId;
+ (void)unregisterFFmpegKitBuffer:(long)bufferId;
@end

@interface FFmpegKitInputBuffer ()
+ (NSString *)urlWithProtocol:(NSString *)protocol
                   resourceId:(long)resourceId
                    extension:(NSString *)extension;
- (instancetype)initWithResourceId:(long)resourceId
                         extension:(NSString *)extension;
- (void)ensureOpen;
@end

@implementation FFmpegKitInputBuffer {
    long _resourceId;
    NSString *_url;
    BOOL _closed;
}

+ (instancetype)fromData:(NSData *)data extension:(NSString *)extension {
    if (data == nil) {
        [NSException raise:NSInvalidArgumentException
                    format:@"data must not be nil"];
    }

    long resourceId = [FFmpegKitConfig registerFFmpegKitInputBuffer:data];
    if (resourceId == 0) {
        [NSException raise:NSInternalInconsistencyException
                    format:@"Failed to register FFmpegKit input buffer."];
    }

    return [[self alloc] initWithResourceId:resourceId extension:extension];
}

+ (instancetype)fromBytes:(const void *)bytes
                   length:(NSUInteger)length
                extension:(NSString *)extension {
    if (bytes == NULL && length > 0) {
        [NSException raise:NSInvalidArgumentException
                    format:@"bytes must not be NULL when length is positive"];
    }

    long resourceId =
        [FFmpegKitConfig registerFFmpegKitInputBufferWithBytes:bytes
                                                        length:length];
    if (resourceId == 0) {
        [NSException raise:NSInternalInconsistencyException
                    format:@"Failed to register FFmpegKit input buffer."];
    }

    return [[self alloc] initWithResourceId:resourceId extension:extension];
}

+ (NSString *)urlWithProtocol:(NSString *)protocol
                   resourceId:(long)resourceId
                    extension:(NSString *)extension {
    NSString *normalizedExtension = @"bin";

    if (extension != nil) {
        NSString *candidate = [[extension
            stringByTrimmingCharactersInSet:
                [NSCharacterSet whitespaceAndNewlineCharacterSet]]
            lowercaseStringWithLocale:[NSLocale localeWithLocaleIdentifier:
                                                   @"en_US_POSIX"]];

        while ([candidate hasPrefix:@"."]) {
            candidate = [candidate substringFromIndex:1];
        }

        NSMutableString *safeExtension = [[NSMutableString alloc] init];
        for (NSUInteger i = 0; i < [candidate length]; i++) {
            unichar c = [candidate characterAtIndex:i];
            if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
                [safeExtension appendFormat:@"%C", c];
            }
        }

        if ([safeExtension length] > 0) {
            normalizedExtension = safeExtension;
        }
    }

    return [NSString stringWithFormat:@"%@:%ld.%@", protocol, resourceId,
                                      normalizedExtension];
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

- (void)close {
    if (!_closed) {
        [FFmpegKitConfig unregisterFFmpegKitBuffer:_resourceId];
        _closed = YES;
    }
}

- (void)ensureOpen {
    if (_closed) {
        [NSException raise:NSInternalInconsistencyException
                    format:@"FFmpegKit input buffer is closed."];
    }
}

@end
