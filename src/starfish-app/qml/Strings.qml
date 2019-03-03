/* The MIT License (MIT)
 *
 * Copyright (c) 2018, 2019 Jean Gressmann <jean@0x42.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

pragma Singleton
import QtQuick 2.0

Item { // Components can't declare functions
    //% "There seems to be nothing here."
    readonly property string noContent: qsTrId("strings-no-content")
    //% "Delete seen VOD files"
    readonly property string deleteSeenVodFiles: qsTrId("strings-delete-seen-vod-files")
    //% "Delete VODs"
    readonly property string deleteVods: qsTrId("strings-delete-vods")

    function deleteSeenVodFileRemorse(name) {
        //% "Deleting seen VOD files for %1"
        return qsTrId("strings-deleting-seen-vods").arg(name)
    }

    function deleteVodsRemorse(name) {
        //% "Deleting VODs for %1"
        return qsTrId("strings-deleting-vods").arg(name)
    }
}
