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

import QtQuick 2.0
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0
import "."

Item {
    id: root
    readonly property var key: _key
    property string title
    readonly property int playbackOffset: _playbackOffset
    property int _playbackOffset: 0
    property alias playerPage: videoPlayerPageConnections.target
    property var _key

    signal dataSaved(var key, int offset, string thumbnailPath)

    Connections {
        id: videoPlayerPageConnections

        ignoreUnknownSignals: true
        onTitleChanged: root.title = target.title
        onPlaybackOffsetChanged: root._playbackOffset = target.playbackOffset

        onClosedChanged: {
            console.debug("closed")
            root._saveVideoData()
        }

        onVideoThumbnailCaptured: function (path) {
            console.debug("got frame")
        }
    }

    function setKey(k) {
        _saveVideoData()

        _key = k
    }

    function _saveVideoData() {
        if (!!key) {
            var k = _key
            _key = null

            var thumbnailFilePath = VodDataManager.makeThumbnailFile(Global.videoThumbnailPath)
            console.debug("unlink " + Global.videoThumbnailPath)
            var unlinkResult = App.unlink(Global.videoThumbnailPath)
            if (!unlinkResult) {
                console.debug("unlink failed!!!!!!!!!!!!!!")
            }

            if (thumbnailFilePath) {
                VodDataManager.recentlyWatched.setThumbnailPath(k, thumbnailFilePath)
            }

//            VodDataManager.recentlyWatched.setOffset(k, _playbackOffset)

            dataSaved(k, _playbackOffset, thumbnailFilePath)

            _playbackOffset = 0
        }
    }
}
