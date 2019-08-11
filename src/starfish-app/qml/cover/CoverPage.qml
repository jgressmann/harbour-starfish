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
import QtGraphicalEffects 1.0
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0
import ".."

CoverBackground {
    id: root

    property int _newVodCount: 0
//    property bool _pausedPlayback: false

    SqlVodModel {
        id: sqlModel
        database: VodDataManager.database
        columns: ["count"]
        Component.onCompleted: {
            VodDataManager.vodsChanged.connect(reload)
        }

        Component.onDestruction: {
            VodDataManager.vodsChanged.disconnect(reload)
        }
    }

    Image {
        id: videoFrameImage
        anchors.fill: parent
        fillMode: Image.PreserveAspectCrop
        visible: status === Image.Ready
        cache: false // can't cache always loading same file, it's the content that changes
//        onStatusChanged: {
//            console.debug("video frame image status: " + status)
//        }
    }

    Image {
        id: logoImage
        visible: !videoFrameImage.visible
        anchors.centerIn: parent
        source: "/usr/share/icons/hicolor/128x128/apps/harbour-starfish.png"

        Desaturate {
            anchors.fill: parent
            source: parent
            desaturation: 1
        }
    }

    Item {
        x: Theme.horizontalPageMargin
        width: parent.width - 2*x
        y: Theme.paddingMedium
//        height: parent.height - 2*y


        Row {
            id: row
            Label {
                font.pixelSize: Theme.fontSizeHuge
                text: _newVodCount
                anchors.verticalCenter: parent.verticalCenter
            }

            Item {
                height: parent.height
                width: Theme.paddingSmall
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter

                Label {
                    font.pixelSize: Theme.fontSizeTiny
                    font.bold: true
                    //% "New"
                    text: qsTrId("sf-cover-label-new-vods-line1", _newVodCount)
                }

                Label {
                    font.pixelSize: Theme.fontSizeTiny
                    font.bold: true
                    //% "VODs"
                    text: qsTrId("sf-cover-label-new-vods-line2", _newVodCount)

                }
            }
        }

        Item {
            id: spacer
            height: Theme.paddingSmall
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: row.bottom
        }

        Label {
            id: updateLabel
            visible: settingLastUpdateTimestamp.value > 0
            anchors.left: parent.left
            anchors.right: parent.right
//            anchors.bottom: parent.bottom
            anchors.top: spacer.bottom
            font.pixelSize: Theme.fontSizeLarge
//            font.bold: true
            color: Theme.highlightColor
            wrapMode: Text.Wrap

            SequentialAnimation on opacity {
                loops: Animation.Infinite
                running: vodDatabaseDownloader.status === VodDatabaseDownloader.Status_Downloading

                NumberAnimation {
                   from: 1
                   to: 0.2
                   duration: 750
                }

                NumberAnimation {
                   from: 0.2
                   to: 1
                   duration: 750
                }

                onRunningChanged: {
//                    console.debug("running="+running)
                    if (!running) {
                        updateLabel.opacity = 1
                    }
                }
            }
        }

        Item {
            id: spacer2
            height: Theme.paddingSmall
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: updateLabel.bottom
        }

        Label {
            id: activeDownloadsLabel
            visible: VodDataManager.vodDownloads > 0
            anchors.left: parent.left
            anchors.right: parent.right
//            anchors.bottom: parent.bottom
            anchors.top: spacer2.bottom
            font.pixelSize: Theme.fontSizeLarge
//            font.bold: true
            color: Theme.highlightColor
            wrapMode: Text.Wrap
            //% "%1 active download"
            text: qsTrId("sf-cover-label-active-vod-downloads", VodDataManager.vodDownloads).arg(VodDataManager.vodDownloads)

            onVisibleChanged: opacity = 1

            SequentialAnimation on opacity {
                loops: Animation.Infinite
                running: activeDownloadsLabel.visible

                NumberAnimation {
                   from: 1
                   to: 0.2
                   duration: 750
                }

                NumberAnimation {
                   from: 0.2
                   to: 1
                   duration: 750
                }
            }
        }
    }

    onStatusChanged: {
//        console.debug("cover page status=" + status)
        switch (status) {
        case PageStatus.Activating:
            // NOTE: the cover page will be active if the app menu is open
            // NOTE: the cover page will go though an activating->active->deactivating->inactive
            //       cycle if the device comes out of lock
            //sqlModel.select = ""
            sqlModel.select = "select count(*) from " + Global.defaultTable + " where " + Global.getNewVodsContstraints()
            _newVodCount = sqlModel.data(sqlModel.index(0, 0), 0)
            videoFrameImage.source = "" // force reload
            videoFrameImage.source = Global.videoCoverPath
            refreshTimer.restart()
            _updateUpdatingLabel()
            if (settingPlaybackPauseInCoverMode.value) {
//                _pausedPlayback = true
//                Global.addPlaybackPause()

                // FIX ME this is actually buggy b/c it doesn't
                // handle the 'returning from device lock' case correctly
                if (window.videoPlayer &&
                    window.videoPlayer.isPlaying) {
                    window.videoPlayer.pause()
                }
            }
            break
        case PageStatus.Deactivating:
            refreshTimer.stop()
//            if (_pausedPlayback) {
//                _pausedPlayback = false
//                Global.removePlaybackPause()
//            }
            break
        }
    }



    Timer {
        id: refreshTimer
        interval: 60000
        repeat: true
        onTriggered: _updateUpdatingLabel()
    }

    Connections {
        target: vodDatabaseDownloader
        onStatusChanged: _updateUpdatingLabel()
    }

    CoverActionList {
        enabled: VodDataManager.ready
        CoverAction {
            iconSource: vodDatabaseDownloader.status === VodDatabaseDownloader.Status_Downloading
                        ? "image://theme/icon-cover-cancel"
                        : "image://theme/icon-cover-refresh"
            onTriggered: {
                if (vodDatabaseDownloader.status === VodDatabaseDownloader.Status_Downloading) {
                    vodDatabaseDownloader.cancel()
                } else {
                    vodDatabaseDownloader.downloadNew()
                }
            }
        }
    }

    function _updateUpdatingLabel() {
        updateLabel.text = _updatingLabelText()
    }

    function _updatingLabelText() {
        if (vodDatabaseDownloader.status === VodDatabaseDownloader.Status_Downloading) {
            //% "Updating"
            return qsTrId("sf-cover-update-status-updating")
        }

        var n = Global.secondsSinceTheEpoch()
        console.debug("now=" + n + " update=" + settingLastUpdateTimestamp.value)
        var diff = n - settingLastUpdateTimestamp.value
        if (diff <= 10) {
            //% "Updated just now"
            return qsTrId("sf-cover-update-status-just-now")
        } else if (diff < 60) {
            //% "Updated seconds ago"
            return  qsTrId("sf-cover-update-status-just-seconds-ago")
        } else if (diff < 60 * 60) {
            //% "Updated minutes ago"
            return  qsTrId("sf-cover-update-status-minutes-ago")
        } else if (diff < 24 * 60 * 60) {
            //% "Updated hours ago"
            return  qsTrId("sf-cover-update-status-hours-ago")
        } else {
            //% "Updated a really long time ago"
            return qsTrId("sf-cover-update-status-more")
        }
    }
}

