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
import Sailfish.Pickers 1.0
import org.duckdns.jgressmann 1.0
import "."

PullDownMenu {
    MenuItem {
        text: "About " + App.displayName
        onClicked: pageStack.push(Qt.resolvedUrl("pages/AboutPage.qml"))
    }

    MenuItem {
        text: qsTr("Settings")
        onClicked: pageStack.push(Qt.resolvedUrl("pages/SettingsPage.qml"))
    }

    MenuItem {
        text: "Tools"
        onClicked: pageStack.push(Qt.resolvedUrl("pages/ToolsPage.qml"))
    }



//    MenuItem {
//        text: qsTr("Clear VOD data")
//        onClicked: {
//            var dialog = pageStack.push(
//                        Qt.resolvedUrl("pages/ConfirmClearDialog.qml"),
//                        {
//                            acceptDestination: Qt.resolvedUrl("pages/StartPage.qml"),
//                            acceptDestinationAction: PageStackAction.Replace
//                        })
//            dialog.accepted.connect(function() {
//                console.debug("clear")
//                VodDataManager.clear()
//                VodDataManager.fetchIcons()
//            })
//        }
//    }


    MenuItem {
        id: openVideo
        text: qsTr("Open video")

        property string videoUrl
        property string title
        property int playbackOffset: 0
        property int videoId: -1
        property int storeThreshold: -1
//        property var playerPage

        onClicked: {
            _openVideoPage(function (url, offset) {
                var playerPage = pageStack.replace(
                            Qt.resolvedUrl("pages/VideoPlayerPage.qml"),
                            {
                                source: url,
                                startOffset: offset
                            },
                            PageStackAction.Immediate)

                var callback = function () {
                    _openVideoPage(function(a, b) {
                        playerPage.play(a, b)
                        pageStack.pop(playerPage)
                    })
                }
                playerPage.openHandler = callback

            })
        }


        Connections {
            id: videoPlayerPageConnections
//            target: pageStack.currentPage
            ignoreUnknownSignals: true
//            enabled: false
            onTitleChanged: openVideo.title = target.title
            onPlaybackOffsetChanged: openVideo.playbackOffset = target.playbackOffset

            onSourceChanged: {
                if (target) {
                    console.debug("player source changed")
                    target = null
                    var thumbnailPath = VodDataManager.makeThumbnailFile(Global.videoFramePath)
                    openVideo._storeRecentVideo(thumbnailPath)
                }
            }
        }

        Connections {
            target: pageStack
            onDepthChanged: {
                if (openVideo.storeThreshold >= 0 && pageStack.depth < openVideo.storeThreshold) {
                    console.debug("player page popped")
                    openVideo.storeThreshold = -1
                    var thumbnailPath = VodDataManager.makeThumbnailFile(Global.videoFramePath)
                    openVideo._storeRecentVideo(thumbnailPath)
                }
            }
        }

        function _storeRecentVideo(thumbnailPath) {
            console.debug("video_id="+ openVideo.videoId + " offset=" + openVideo.playbackOffset + " title=" + openVideo.title + " source=" + openVideo.videoUrl + " thumbnail=" + thumbnailPath)
//            recentlyUsedVideos.add([openVideo.videoId, openVideo.videoUrl, openVideo.playbackOffset, thumbnailPath])
        }

        function _openVideoPage(callback, openHandler) {
            var threshold = pageStack.depth
            var openPage = pageStack.push(Qt.resolvedUrl("pages/OpenVideoPage.qml"))
            openPage.videoSelected.connect(function (obj, playbackUrl, offset) {
                openVideo.storeThreshold = threshold
//                openVideo.videoId = videoRowId
//                openVideo.videoUrl = Global.completeFileUrl(videoUrl)
//                openVideo.playbackOffset = offset
                recentlyUsedVideos.add(obj)
                callback(playbackUrl, offset)
            })
        }
    }

    MenuItem {
        text: qsTr("Fetch new VODs")
        enabled: vodDatabaseDownloader.status !== VodDatabaseDownloader.Status_Downloading
        onClicked: vodDatabaseDownloader.downloadNew()
    }



}
