/*
 * Copyright (c) 2024 ARTHENICA LTD
 *
 * This file is part of FFmpegKit.
 *
 * FFmpegKit is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FFmpegKit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with FFmpegKit.  If not, see <http://www.gnu.org/licenses/>.
 */

package com.arthenica.ffmpegkit

import android.content.ContentProvider
import android.content.ContentValues
import android.content.Context
import android.database.Cursor
import android.database.MatrixCursor
import android.net.Uri
import android.os.ParcelFileDescriptor
import android.provider.DocumentsContract
import org.junit.Assert
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.robolectric.Robolectric
import org.robolectric.RobolectricTestRunner
import org.robolectric.RuntimeEnvironment
import org.robolectric.annotation.Config
import java.io.File

/**
 * Lifecycle tests for SAF protocol urls, exercising [FFmpegKitConfig.getSafParameter] together with
 * the native <code>safOpen</code>/<code>safClose</code> callbacks against a real (Robolectric)
 * Android framework.
 *
 * <p>These verify the per-url reuse behaviour: a single-use url is unregistered when its file is
 * closed and can no longer be opened, a reusable url survives the close, and the reuse decision is
 * captured at creation time so that later changes to the global setting do not affect urls that
 * already exist.
 */
@RunWith(RobolectricTestRunner::class)
@Config(sdk = [34])
class SAFProtocolUrlLifecycleTest {

    /**
     * Minimal content provider that satisfies the two content resolver calls made while a SAF url is
     * resolved and opened: the display-name query in [FFmpegKitConfig.getSafParameter] and the
     * <code>openFileDescriptor</code> call in <code>safOpen</code>.
     */
    class FakeSafProvider : ContentProvider() {
        override fun onCreate(): Boolean = true

        override fun query(
            uri: Uri,
            projection: Array<out String>?,
            selection: String?,
            selectionArgs: Array<out String>?,
            sortOrder: String?
        ): Cursor {
            val cursor = MatrixCursor(arrayOf(DocumentsContract.Document.COLUMN_DISPLAY_NAME))
            cursor.addRow(arrayOf("video.mp4"))
            return cursor
        }

        override fun openFile(uri: Uri, mode: String): ParcelFileDescriptor {
            val file = File.createTempFile("ffkitsaf-test", ".mp4")
            file.deleteOnExit()
            return ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_WRITE)
        }

        override fun getType(uri: Uri): String = "video/mp4"
        override fun insert(uri: Uri, values: ContentValues?): Uri? = null
        override fun delete(uri: Uri, selection: String?, selectionArgs: Array<out String>?): Int = 0
        override fun update(
            uri: Uri,
            values: ContentValues?,
            selection: String?,
            selectionArgs: Array<out String>?
        ): Int = 0
    }

    private lateinit var context: Context
    private lateinit var uri: Uri

    @Before
    fun setUp() {
        context = RuntimeEnvironment.getApplication()
        Robolectric.setupContentProvider(FakeSafProvider::class.java, AUTHORITY)
        uri = Uri.parse("content://$AUTHORITY/document/1")
    }

    @Test
    fun singleUseUrlIsUnregisteredAfterClose() {
        val id = safIdOf(FFmpegKitConfig.getSafParameter(context, uri, "r", false))

        val fd = safOpen(id)
        Assert.assertTrue("expected a valid file descriptor", fd > 0)
        Assert.assertEquals(1, safClose(fd))

        // the url was single-use, so it must have been unregistered on close
        Assert.assertEquals(0, safOpen(id))
    }

    @Test
    fun reusableUrlSurvivesClose() {
        val url = FFmpegKitConfig.getSafParameter(context, uri, "r", true)
        val id = safIdOf(url)

        val firstFd = safOpen(id)
        Assert.assertTrue(firstFd > 0)
        Assert.assertEquals(1, safClose(firstFd))

        // the url is reusable, so it must still be registered and openable again
        val secondFd = safOpen(id)
        Assert.assertTrue(secondFd > 0)
        Assert.assertEquals(1, safClose(secondFd))

        FFmpegKitConfig.unregisterSafProtocolUrl(url)
    }

    @Test
    fun singleUseAndReusableUrlsCoexist() {
        val reusableUrl = FFmpegKitConfig.getSafParameter(context, uri, "r", true)
        val singleUseUrl = FFmpegKitConfig.getSafParameter(context, uri, "r", false)
        val reusableId = safIdOf(reusableUrl)
        val singleUseId = safIdOf(singleUseUrl)

        // closing the single-use url unregisters only that url
        val singleUseFd = safOpen(singleUseId)
        Assert.assertTrue(singleUseFd > 0)
        Assert.assertEquals(1, safClose(singleUseFd))
        Assert.assertEquals(0, safOpen(singleUseId))

        // the reusable url created alongside it is unaffected and still usable
        val reusableFd = safOpen(reusableId)
        Assert.assertTrue(reusableFd > 0)
        Assert.assertEquals(1, safClose(reusableFd))

        FFmpegKitConfig.unregisterSafProtocolUrl(reusableUrl)
    }

    @Test
    fun defaultReuseIsCapturedAtCreationTimeWhenGlobalFlagIsFalse() {
        val original = FFmpegKitConfig.getSafUrlsReusable()
        try {
            FFmpegKitConfig.setSafUrlsReusable(false)
            val id = safIdOf(FFmpegKitConfig.getSafParameter(context, uri, "r"))

            // flipping the global flag after creation must not turn this url reusable
            FFmpegKitConfig.setSafUrlsReusable(true)

            val fd = safOpen(id)
            Assert.assertTrue(fd > 0)
            Assert.assertEquals(1, safClose(fd))
            Assert.assertEquals(0, safOpen(id))
        } finally {
            FFmpegKitConfig.setSafUrlsReusable(original)
        }
    }

    @Test
    fun defaultReuseIsCapturedAtCreationTimeWhenGlobalFlagIsTrue() {
        val original = FFmpegKitConfig.getSafUrlsReusable()
        try {
            FFmpegKitConfig.setSafUrlsReusable(true)
            val url = FFmpegKitConfig.getSafParameter(context, uri, "r")
            val id = safIdOf(url)

            // flipping the global flag after creation must not turn this url single-use
            FFmpegKitConfig.setSafUrlsReusable(false)

            val firstFd = safOpen(id)
            Assert.assertTrue(firstFd > 0)
            Assert.assertEquals(1, safClose(firstFd))

            val secondFd = safOpen(id)
            Assert.assertTrue(secondFd > 0)
            Assert.assertEquals(1, safClose(secondFd))

            FFmpegKitConfig.unregisterSafProtocolUrl(url)
        } finally {
            FFmpegKitConfig.setSafUrlsReusable(original)
        }
    }

    companion object {
        private const val AUTHORITY = "com.arthenica.ffmpegkit.test.saf"

        // safOpen/safClose are private native callbacks; they are reached by reflection so their
        // JNI-visible names stay untouched.
        private val safOpenMethod =
            FFmpegKitConfig::class.java.getDeclaredMethod("safOpen", Integer.TYPE)
                .apply { isAccessible = true }
        private val safCloseMethod =
            FFmpegKitConfig::class.java.getDeclaredMethod("safClose", Integer.TYPE)
                .apply { isAccessible = true }

        private fun safOpen(safId: Int): Int = safOpenMethod.invoke(null, safId) as Int

        private fun safClose(fileDescriptor: Int): Int =
            safCloseMethod.invoke(null, fileDescriptor) as Int

        /** Extracts the numeric SAF id from a url such as <code>ffkitsaf:12.mp4</code>. */
        private fun safIdOf(safUrl: String): Int =
            safUrl.substringAfter("ffkitsaf:").substringBefore(".").toInt()
    }
}
