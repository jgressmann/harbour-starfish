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
import ".."

BasePage {
    id: page

    VisualItemModel {
        id: visualModel

        ListItem {
            contentHeight: Global.itemHeight

            Label {
                x: Theme.horizontalPageMargin
                width: page.width - 2*x

                text: qsTr("New VODs")

                font.pixelSize: Global.labelFontSize
                anchors.verticalCenter: parent.verticalCenter
            }

            onClicked: {
                pageStack.push(Qt.resolvedUrl("NewPage.qml"))
            }
        }

        ListItem {
            contentHeight: Global.itemHeight

            Label {
                x: Theme.horizontalPageMargin
                width: page.width - 2*x
                text: qsTr("Browse VODs")

                font.pixelSize: Global.labelFontSize
                anchors.verticalCenter: parent.verticalCenter
            }

            onClicked: {
                pageStack.push(
                            Qt.resolvedUrl("FilterPage.qml"),
                            {
                                title: qsTr("Game"),
                                filters: {},
                                key: "game",
                                grid: true,
                            })
            }
        }

        ListItem {
            contentHeight: Global.itemHeight

            Label {
                id: videoPlayerLabel
                x: Theme.horizontalPageMargin
                width: page.width - 2*x
                text: qsTr("Video player")

                font.pixelSize: Global.labelFontSize
                anchors.verticalCenter: parent.verticalCenter
            }

            onClicked: {
                pageStack.push(Qt.resolvedUrl("VideoPlayerPage.qml"))
            }
        }


        Column {
            id: continueVodFromRowId
            width: parent.width

            function updateVisible() {
                visible = /*settingPlaybackOffset.value >= 0 && */ settingPlaybackRowId.value >= 0 && sqlModel.rowCount() > 0
            }

            Label {
                x: Theme.horizontalPageMargin
                width: page.width - 2*x
                height: continueVodFromRowId.visible ? videoPlayerLabel.height : 0
                text: qsTr("Continue watching")
                color: Theme.highlightColor
                font.pixelSize: Global.labelFontSize
            }

            SilicaListView {
                height: continueVodFromRowId.visible ? Global.itemHeight + Theme.fontSizeMedium : 0
                width: parent.width
                quickScrollEnabled: false
                model: sqlModel

                delegate: Component {
                    MatchItem {
                        contentHeight: ListView.view.height

                        eventFullName: event_full_name
                        stageName: stage_name
                        side1: side1_name
                        side2: side2_name
                        race1: side1_race
                        race2: side2_race
                        matchName: match_name
                        matchDate: match_date
                        rowId: rowid
                        startOffsetMs: settingPlaybackOffset.value * 1000
                        menuEnabled: false
                    }
                }
            }

            Component.onCompleted: updateVisible()
        }

        ListItem {
            visible: !continueVodFromRowId.visible && /*settingPlaybackOffset.value >= 0 && */settingPlaybackUrl.value
            contentHeight: Global.itemHeight

            Label {
                x: Theme.horizontalPageMargin
                width: page.width - 2*x
                text: qsTr("Continue watching...")

                font.pixelSize: Global.labelFontSize
                anchors.verticalCenter: parent.verticalCenter
            }

            onClicked: {
                if (settingExternalMediaPlayer.value) {
                    Qt.openUrlExternally(settingPlaybackUrl.value)
                } else {
                    pageStack.push(
                                Qt.resolvedUrl("VideoPlayerPage.qml"),
                                {
                                    source: settingPlaybackUrl.value,
                                    startOffsetMs: settingPlaybackOffset.value * 1000,
                                })
                }
            }
        }
    }

    SilicaFlickable {
        anchors.fill: parent

        // Why is this necessary?
        contentWidth: parent.width

        VerticalScrollDecorator {}
        TopMenu {}

        SilicaListView {
            anchors.fill: parent
            model: visualModel
            header: PageHeader {
                title: "Go go go"
            }
        }
    }

    SqlVodModel {
        id: sqlModel
        dataManager: VodDataManager
        columns: ["event_full_name", "stage_name", "side1_name", "side2_name", "side1_race", "side2_race", "match_date", "match_name", "rowid", "offset"]
    }

    onStatusChanged: {
        if (PageStatus.Activating === status) {
            sqlModel.select = "select " + sqlModel.columns.join(",") + " from vods where id=" + settingPlaybackRowId.value
            continueVodFromRowId.updateVisible()
//            console.debug("entry page rows=" + sqlModel.rowCount())
//            console.debug("entry page offset=" + settingPlaybackOffset.value + " url=" + settingPlaybackUrl.value)
//            console.debug("entry page match visible=" + continueVodFromRowId.visible)
        }
    }

//    Component.onCompleted: {
//        console.debug("entry page rowid=" + settingPlaybackRowId.value + " offset=" + settingPlaybackOffset.value + " url=" + settingPlaybackUrl.value)
//    }
}

