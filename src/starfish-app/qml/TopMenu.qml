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

    MenuItem {
        id: openVideo
        text: qsTr("Open video")

        RecentlyWatchedVideoUpdater {
            id: updater
        }

        onClicked: {
            var topPage = pageStack.currentPage
            _openVideoPage(function (url, offset, saveScreenShot) {
                pageStack.pop(topPage, PageStackAction.Immediate)

                if (settingExternalMediaPlayer.value && url.indexOf("http") !== 0) {
                    Qt.openUrlExternally(url)
                } else {
                    var playerPage = pageStack.push(
                                Qt.resolvedUrl("pages/VideoPlayerPage.qml"),
                                null,
                                PageStackAction.Immediate)

                    var callback = function () {
                        _openVideoPage(function(a, b) {
                            playerPage.play(a, b)
                            pageStack.pop(playerPage)
                        })
                    }
                    playerPage.openHandler = callback

                    playerPage.play(url, offset, saveScreenShot)

                    updater.playerPage = playerPage

                }
            })
        }


        function _openVideoPage(callback) {
            var openPage = pageStack.push(Qt.resolvedUrl("pages/OpenVideoPage.qml"))
            openPage.videoSelected.connect(function (obj, url, offset, saveScreenShot) {
                recentlyUsedVideos.add(obj)
                offset = Math.max(offset, recentlyUsedVideos.select(["offset"], obj)[0].offset)
                updater.setKey(obj)
                callback(url, offset, saveScreenShot)
            })
        }
    }

    MenuItem {
        text: qsTr("Fetch new VODs")
        enabled: vodDatabaseDownloader.status !== VodDatabaseDownloader.Status_Downloading
        onClicked: {
            settingLastUpdateTimestamp.value = Global.secondsSinceTheEpoch() // in case debugging ends
            vodDatabaseDownloader.downloadNew()
        }
    }
}
