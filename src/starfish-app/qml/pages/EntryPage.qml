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

        Column {
            id: continueWatching
            width: parent.width

//            function updateVisible() {
//                var count = recentlyUsedVideos.rowCount()
//                visible = count > 0
//                recentlyWatchedVideoView.height = (Global.itemHeight + Theme.fontSizeMedium) * count
//            }

            Label {

                x: Theme.horizontalPageMargin
                width: page.width - 2*x
                text: qsTr("Continue watching")
                color: Theme.highlightColor
                font.pixelSize: Global.labelFontSize
            }

            RecentlyWatchedVideoView {
                id: recentlyWatchedVideoView
                height: 0
                width: parent.width

                RecentlyWatchedVideoUpdater {
                    id: updater
                }

                onClicked: function (obj, url, offset) {
                    Global.playVideoHandler(updater, obj, url, offset)
                }

                onCountChanged: {
                    continueWatching.visible = count > 0
                    height = (Global.itemHeight + Theme.fontSizeMedium) * count
                }
            }

//            Component.onCompleted: updateVisible()
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

                VodDataManagerBusyIndicator {}
            }
        }
    }

    onStatusChanged: {
        if (PageStatus.Activating === status) {
            continueWatching.updateVisible()
        }
    }
}

