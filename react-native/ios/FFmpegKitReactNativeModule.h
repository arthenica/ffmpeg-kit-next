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

#import <Foundation/Foundation.h>
#import <ffmpegkit/FFmpegKitConfig.h>
#import <React/RCTBridgeModule.h>
#import <React/RCTEventEmitter.h>

#ifdef RCT_NEW_ARCH_ENABLED
#import <FFmpegKitReactNativeSpec/FFmpegKitReactNativeSpec.h>
#endif

#ifdef RCT_NEW_ARCH_ENABLED
@interface FFmpegKitReactNativeModule : RCTEventEmitter<NativeFFmpegKitReactNativeModuleSpec>
#else
@interface FFmpegKitReactNativeModule : RCTEventEmitter<RCTBridgeModule>
#endif
@end
