/*
 * Copyright (c) 2018-2022, 2026 Taner Sener
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

#import "MediaInformationJsonParser.h"

NSString *const MediaInformationJsonParserKeyStreams = @"streams";
NSString *const MediaInformationJsonParserKeyChapters = @"chapters";

static MediaInformation *parseMediaInformation(
        NSString *ffprobeJsonOutput,
        NSException *__autoreleasing *failure) {

    NSData *jsonData =
        [ffprobeJsonOutput dataUsingEncoding:NSUTF8StringEncoding];
    if (jsonData == nil) {
        *failure = [NSException exceptionWithName:@"ParsingException"
                                           reason:@"The text is not valid UTF-8."
                                         userInfo:nil];
        return nil;
    }

    NSError *error = nil;
    id json = [NSJSONSerialization JSONObjectWithData:jsonData
                                              options:kNilOptions
                                                error:&error];
    if (error != nil) {
        *failure = [NSException
            exceptionWithName:@"ParsingException"
                       reason:[NSString
                                  stringWithFormat:@"%ld", (long)[error code]]
                     userInfo:[error userInfo]];
        return nil;
    }
    if (json == nil) {
        return nil;
    }
    if (![json isKindOfClass:[NSDictionary class]]) {
        *failure = [NSException
            exceptionWithName:@"ParsingException"
                       reason:@"The top-level JSON value is not an object."
                     userInfo:nil];
        return nil;
    }
    NSDictionary *jsonDictionary = json;

    /*
     * Streams and chapters are lists of objects. Anything else is skipped, or
     * kept as an entry without properties, instead of sending a message the
     * value does not understand, which would raise.
     */
    NSMutableArray *streamArray = [[NSMutableArray alloc] init];
    id jsonStreamArray =
        [jsonDictionary objectForKey:MediaInformationJsonParserKeyStreams];
    if ([jsonStreamArray isKindOfClass:[NSArray class]]) {
        for (id element in jsonStreamArray) {
            NSDictionary *streamDictionary =
                [element isKindOfClass:[NSDictionary class]] ? element : nil;
            [streamArray
                addObject:[[StreamInformation alloc] init:streamDictionary]];
        }
    }

    NSMutableArray *chapterArray = [[NSMutableArray alloc] init];
    id jsonChapterArray =
        [jsonDictionary objectForKey:MediaInformationJsonParserKeyChapters];
    if ([jsonChapterArray isKindOfClass:[NSArray class]]) {
        for (id element in jsonChapterArray) {
            NSDictionary *chapterDictionary =
                [element isKindOfClass:[NSDictionary class]] ? element : nil;
            [chapterArray addObject:[[Chapter alloc] init:chapterDictionary]];
        }
    }

    return [[MediaInformation alloc] init:jsonDictionary
                              withStreams:streamArray
                             withChapters:chapterArray];
}

@implementation MediaInformationJsonParser

+ (MediaInformation *)from:(NSString *)ffprobeJsonOutput {
    @try {
        return [self fromWithError:ffprobeJsonOutput];
    } @catch (NSException *exception) {
        NSLog(@"MediaInformation parsing failed: %@.\n",
              [NSString stringWithFormat:@"%@\n%@", [exception userInfo],
                                         [exception callStackSymbols]]);
        return nil;
    }
}

/*
 * Without ARC exception cleanup, strong references are not released when an
 * exception unwinds through their scope. parseMediaInformation reports failures
 * as values so its references are released before this method raises. Clear the
 * input parameter too, since ARC retains it, and keep failure __autoreleasing.
 */
+ (MediaInformation *)fromWithError:(NSString *)ffprobeJsonOutput {
    NSException *__autoreleasing failure = nil;
    MediaInformation *mediaInformation =
        parseMediaInformation(ffprobeJsonOutput, &failure);
    if (failure != nil) {
        ffprobeJsonOutput = nil;
        @throw failure;
    }
    return mediaInformation;
}

@end
