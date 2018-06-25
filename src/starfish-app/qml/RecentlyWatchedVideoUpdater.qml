/* The MIT License (MIT)
 *
 * Copyright (c) 2018 Jean Gressmann <jean@0x42.de>
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
//    readonly property string thumbnailFilePath: _thumbnailFilePath
//    property string _thumbnailFilePath
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

//        if (!!key) {
//            _thumbnailFilePath = recentlyUsedVideos.select(["thumbnail_path"], key)[0].thumbnail_path
//            if (!_thumbnailFilePath) {
//                _thumbnailFilePath = VodDataManager.makeThumbnailFile(Global.videoThumbnailPath)
//                recentlyUsedVideos.update({ thumbnail_path: _thumbnailFilePath}, key)
//            }

//            console.debug("thumbnail path=" + _thumbnailFilePath)
//        }
    }

    function _saveVideoData() {
        if (!!key) {
            var k = _key
            _key = null
            var _thumbnailFilePath = recentlyUsedVideos.select(["thumbnail_path"], k)[0].thumbnail_path
            if (!_thumbnailFilePath) {
                _thumbnailFilePath = VodDataManager.makeThumbnailFile(Global.videoThumbnailPath)
            }

            console.debug("unlink " + _thumbnailFilePath)
            var result = App.unlink(_thumbnailFilePath)
            if (!result) {
                console.debug("unlink failed!!!!!!!!!!!!!!")
            }

            console.debug("copy " + Global.videoThumbnailPath + " -> " + _thumbnailFilePath)
            result = App.copy(Global.videoThumbnailPath, _thumbnailFilePath)
            if (!result) {
                console.debug("copy failed!!!!!!!!!!!!!!")
            }

            console.debug("unlink " + Global.videoThumbnailPath)
            result = App.unlink(Global.videoThumbnailPath)
            if (!result) {
                console.debug("unlink failed!!!!!!!!!!!!!!")
            }

            recentlyUsedVideos.update({ offset: _playbackOffset, thumbnail_path: _thumbnailFilePath}, k)
            dataSaved(k, _playbackOffset, _thumbnailFilePath)

//            _thumbnailFilePath = ""
            _playbackOffset = 0
        }
    }
}
