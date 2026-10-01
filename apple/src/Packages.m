/*
 * Copyright (c) 2021, 2026 Taner Sener
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

#import "Packages.h"
#import "FFmpegKitConfig.h"
#import "config.h"
#import "libavutil/ffversion.h"

static NSMutableArray *supportedExternalLibraries;

@implementation Packages

+ (void)initialize {
    supportedExternalLibraries = [[NSMutableArray alloc] init];
    [supportedExternalLibraries addObject:@"dav1d"];
    [supportedExternalLibraries addObject:@"fontconfig"];
    [supportedExternalLibraries addObject:@"freetype"];
    [supportedExternalLibraries addObject:@"fribidi"];
    [supportedExternalLibraries addObject:@"gmp"];
    [supportedExternalLibraries addObject:@"gnutls"];
    [supportedExternalLibraries addObject:@"harfbuzz"];
    [supportedExternalLibraries addObject:@"kvazaar"];
    [supportedExternalLibraries addObject:@"mp3lame"];
    [supportedExternalLibraries addObject:@"libaom"];
    [supportedExternalLibraries addObject:@"libass"];
    [supportedExternalLibraries addObject:@"libjxl"];
    [supportedExternalLibraries addObject:@"liblc3"];
    [supportedExternalLibraries addObject:@"libsvtav1"];
    [supportedExternalLibraries addObject:@"iconv"];
    [supportedExternalLibraries addObject:@"libilbc"];
    [supportedExternalLibraries addObject:@"libtheora"];
    [supportedExternalLibraries addObject:@"libvidstab"];
    [supportedExternalLibraries addObject:@"libvorbis"];
    [supportedExternalLibraries addObject:@"libvpx"];
    [supportedExternalLibraries addObject:@"libwebp"];
    [supportedExternalLibraries addObject:@"libxml2"];
    [supportedExternalLibraries addObject:@"opencore-amr"];
    [supportedExternalLibraries addObject:@"openh264"];
    [supportedExternalLibraries addObject:@"openssl"];
    [supportedExternalLibraries addObject:@"opus"];
    [supportedExternalLibraries addObject:@"rubberband"];
    [supportedExternalLibraries addObject:@"sdl2"];
    [supportedExternalLibraries addObject:@"shine"];
    [supportedExternalLibraries addObject:@"snappy"];
    [supportedExternalLibraries addObject:@"soxr"];
    [supportedExternalLibraries addObject:@"speex"];
    [supportedExternalLibraries addObject:@"tesseract"];
    [supportedExternalLibraries addObject:@"twolame"];
    [supportedExternalLibraries addObject:@"vvenc"];
    [supportedExternalLibraries addObject:@"x264"];
    [supportedExternalLibraries addObject:@"x265"];
    [supportedExternalLibraries addObject:@"xvid"];
    [supportedExternalLibraries addObject:@"zimg"];
}

+ (NSString *)getBuildConf {
    return [NSString stringWithUTF8String:FFMPEG_CONFIGURATION];
}

+ (NSString *)getPackageName {
#ifndef FFMPEG_KIT_PACKAGE_NAME
#define FFMPEG_KIT_PACKAGE_NAME
#endif
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define FFMPEG_KIT_PACKAGE_NAME_STR TOSTRING(FFMPEG_KIT_PACKAGE_NAME)

    return [NSString stringWithUTF8String:FFMPEG_KIT_PACKAGE_NAME_STR];
}

+ (NSArray *)getExternalLibraries {
    NSString *buildConfiguration = [Packages getBuildConf];
    NSMutableArray *enabledLibraryArray = [[NSMutableArray alloc] init];

    for (int i = 0; i < [supportedExternalLibraries count]; i++) {
        NSString *supportedExternalLibrary =
            [supportedExternalLibraries objectAtIndex:i];

        NSString *libraryName1 =
            [NSString stringWithFormat:@"enable-%@", supportedExternalLibrary];
        NSString *libraryName2 = [NSString
            stringWithFormat:@"enable-lib%@", supportedExternalLibrary];

        if ([buildConfiguration rangeOfString:libraryName1].location !=
                NSNotFound ||
            [buildConfiguration rangeOfString:libraryName2].location !=
                NSNotFound) {
            [enabledLibraryArray addObject:supportedExternalLibrary];
        }
    }

    [enabledLibraryArray sortUsingSelector:@selector(compare:)];

    return enabledLibraryArray;
}

@end
