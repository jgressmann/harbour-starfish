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
import QtGraphicalEffects 1.0
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0
import ".."

CoverBackground {
    id: root

    property int _newVodCount: 0

    SqlVodModel {
        id: sqlModel
        dataManager: VodDataManager
        columns: ["count"]
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

//            Label {
//                font.pixelSize: Theme.fontSizeTiny
//                font.bold: true
//                text: "New VODs"
//                width: 100
//                height: parent.height
//                wrapMode: Text.Wrap
//                verticalAlignment: Text.AlignVCenter
//            }

            Column {
                anchors.verticalCenter: parent.verticalCenter

                Label {
                    font.pixelSize: Theme.fontSizeTiny
                    font.bold: true
                    text: "New"
                }

                Label {
                    font.pixelSize: Theme.fontSizeTiny
                    font.bold: true
                    text: "VODs"

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
            anchors.bottom: parent.bottom
            anchors.top: spacer.bottom
            font.pixelSize: Theme.fontSizeLarge
//            font.bold: true
            color: Theme.highlightColor
            wrapMode: Text.Wrap

//            onOpacityChanged: {
//                console.debug("opacity=" + opacity)
//            }

            SequentialAnimation on opacity {
                loops: Animation.Infinite
                running: vodDatabaseDownloader.status === VodDatabaseDownloader.Status_Downloading

                NumberAnimation {
                   from: 1
                   to: 0.2
                   duration: 1000
                }

                NumberAnimation {
                   from: 0.2
                   to: 1
                   duration: 1000
                }

                onRunningChanged: {
//                    console.debug("running="+running)
                    if (!running) {
                        updateLabel.opacity = 1
                    }
                }
            }
        }
    }

    onStatusChanged: {
//        console.debug("status=" + status)
        switch (status) {
        case PageStatus.Activating:
            sqlModel.select = ""
            sqlModel.select = "select count(*) from vods where seen=0"
            _newVodCount = sqlModel.data(sqlModel.index(0, 0), 0)
            videoFrameImage.source = "" // force reload
            videoFrameImage.source = Global.videoCoverPath
            refreshTimer.restart()
            _updateUpdatingLabel()
            break
        case PageStatus.Deactivating:
            refreshTimer.stop()
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
            return "Updating"
        }

        var timeStr = ""
        var n = Global.secondsSinceTheEpoch()
        console.debug("now=" + n + " update=" + settingLastUpdateTimestamp.value)
        var diff = n - settingLastUpdateTimestamp.value
        if (diff <= 10) {
            timeStr = "just now"
        } else if (diff < 60) {
            timeStr = "seconds ago"
        } else if (diff < 60 * 60) {
            timeStr = "minutes ago"
        } else if (diff < 24 * 60 * 60) {
            timeStr = "hours ago"
        } else {
            timeStr = "a really long time ago"
        }

        return "Updated " + timeStr
    }
}

