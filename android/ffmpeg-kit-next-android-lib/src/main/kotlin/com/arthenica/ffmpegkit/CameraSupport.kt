/*
 * Copyright (c) 2019-2021, 2026 Taner Sener
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

package com.arthenica.ffmpegkit

import android.content.Context
import android.hardware.camera2.CameraAccessException
import android.hardware.camera2.CameraCharacteristics
import android.hardware.camera2.CameraManager
import android.hardware.camera2.CameraMetadata
import android.util.Log
import com.arthenica.ffmpegkit.FFmpegKitConfig.Companion.TAG

/**
 * <p>Helper class to detect camera devices that can be used in
 * <code>FFmpeg</code>/<code>FFprobe</code> commands.
 */
internal object CameraSupport {

    /**
     * <p>Lists camera ids that can be used in <code>FFmpeg</code>/<code>FFprobe</code> commands.
     *
     * @param context application context
     * @return the list of supported camera ids on Android API Level 24+, an empty list on older
     * API levels
     */
    fun extractSupportedCameraIds(context: Context): List<String> {
        val detectedCameraIdList = ArrayList<String>()

        try {
            val manager = context.getSystemService(Context.CAMERA_SERVICE) as CameraManager?
            if (manager != null) {
                val cameraIdList = manager.cameraIdList

                for (cameraId in cameraIdList) {
                    val chars = manager.getCameraCharacteristics(cameraId)
                    val cameraSupport = chars.get(CameraCharacteristics.INFO_SUPPORTED_HARDWARE_LEVEL)

                    if (cameraSupport != null && cameraSupport == CameraMetadata.INFO_SUPPORTED_HARDWARE_LEVEL_LEGACY) {
                        Log.d(TAG,
                            "Detected camera with id $cameraId has LEGACY hardware level which is not supported by Android Camera2 NDK API."
                        )
                    } else if (cameraSupport != null) {
                        detectedCameraIdList.add(cameraId)
                    }
                }
            }
        } catch (e: CameraAccessException) {
            Log.w(TAG, "Detecting camera ids failed.", e)
        }

        return detectedCameraIdList
    }

}
