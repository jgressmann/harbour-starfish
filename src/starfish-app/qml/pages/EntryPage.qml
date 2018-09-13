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

//    property var itemPlaying: null
    property int _videoId: -1

    VisualItemModel {
        id: visualModel

        ListItem {
            id: newVodsItem
            contentHeight: visible ? Global.itemHeight : 0

            property string constraints

            Label {
                x: Theme.horizontalPageMargin
                width: page.width - 2*x

                text: qsTr("New")

                font.pixelSize: Global.labelFontSize
                anchors.verticalCenter: parent.verticalCenter
            }

            onClicked: Global.openNewVodPage(pageStack)

            SqlVodModel {
                id: newModel
                dataManager: VodDataManager
                columns: ["count"]
            }

            function update() {
                constraints = Global.getNewVodsContstraints()
                newModel.select = "select count(*) from " + Global.defaultTable + " where " + constraints
                visible = newModel.data(newModel.index(0, 0)) > 0
            }
        }

        ListItem {
            id: unseenVodsItem
            contentHeight: visible ? Global.itemHeight : 0

            Label {
                x: Theme.horizontalPageMargin
                width: page.width - 2*x

                text: qsTr("Not yet watched")

                font.pixelSize: Global.labelFontSize
                anchors.verticalCenter: parent.verticalCenter
            }

            onClicked: {
                pageStack.push(
                            Qt.resolvedUrl("FilterPage.qml"),
                            {
                                title: VodDataManager.label("game"),
                                table: Global.defaultTable,
                                where: " where seen=0",
                                key: "game",
                                grid: true,
                            })
            }

            SqlVodModel {
                id: unseenModel
                dataManager: VodDataManager
                columns: ["count"]
                select: "select count(*) from vods where seen=0"
            }

            function update() {
                visible = unseenModel.data(unseenModel.index(0, 0)) > 0
            }
        }

        ListItem {
            contentHeight: Global.itemHeight

            Label {
                x: Theme.horizontalPageMargin
                width: page.width - 2*x
                text: qsTr("Browse")

                font.pixelSize: Global.labelFontSize
                anchors.verticalCenter: parent.verticalCenter
            }

            onClicked: {
                pageStack.push(
                            Qt.resolvedUrl("FilterPage.qml"),
                            {
                                title: VodDataManager.label("game"),
                                table: Global.defaultTable,
                                key: "game",
                                grid: true,
                            })
            }
        }

        ListItem {
            id: offlineVodsItem
            contentHeight: visible ? Global.itemHeight : 0

            Label {
                x: Theme.horizontalPageMargin
                width: page.width - 2*x
                text: qsTr("Offline available")

                font.pixelSize: Global.labelFontSize
                anchors.verticalCenter: parent.verticalCenter
            }

            onClicked: {
                pageStack.push(
                            Qt.resolvedUrl("FilterPage.qml"),
                            {
                                title: VodDataManager.label("game"),
                                table: "offline_vods",
                                where: " where progress>0",
                                key: "game",
                                grid: true,
                            })
            }

            SqlVodModel {
                id: offlineModel
                dataManager: VodDataManager
                columns: ["count"]
                select: "select count(*) from offline_vods where progress>0"
            }

            function update() {
                visible = offlineModel.data(offlineModel.index(0, 0)) > 0
            }
        }

        ListItem {
            id: downloadingVodsItem
            contentHeight: visible ? Global.itemHeight : 0
            visible: VodDataManager.vodDownloads > 0

            Label {
                x: Theme.horizontalPageMargin
                width: page.width - 2*x
                text: qsTr("Active downloads")

                font.pixelSize: Global.labelFontSize
                anchors.verticalCenter: parent.verticalCenter
            }

            onClicked: pageStack.push(Qt.resolvedUrl("ActiveDownloadPage.qml"))
        }

        ListItem {
            contentHeight: visible ? continueWatching.height : 0
            visible: recentlyWatchedVideoView.height > 0

            menu: ContextMenu {
                MenuItem {
                    text: "Clear"
                    onClicked: recentlyWatchedVideoView.clear()
                }
            }

            Column {
                id: continueWatching
                width: parent.width

                Label {
                    visible: recentlyWatchedVideoView.height > 0
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

                    onClicked: function (obj, url, offset, matchItem) {
    //                    itemPlaying = matchItem
                        _videoId = obj["video_id"] || -1
                        console.debug("clicked match item " + matchItem + " " + typeof(matchItem))
                        Global.playVideoHandler(updater, obj, url, offset)
                    }

                    onCountChanged: {
                        continueWatching.visible = count > 0
                        /* overestimate, so that view is long enough */
                        height = (Global.itemHeight + Theme.fontSizeHuge)  * count
                    }
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
                title: "go go go"

                VodDataManagerBusyIndicator {}
            }
        }
    }

    onStatusChanged: {
        switch (status) {
        case PageStatus.Activating:
            if (-1 !== _videoId) {
                var matchItem = recentlyWatchedVideoView.getMatchItemById(_videoId)
                if (matchItem) {
                    matchItem.cancelImplicitVodFileFetch()
                }
                _videoId = -1
            }

            offlineVodsItem.update()
            newVodsItem.update()
            unseenVodsItem.update()

//            // THIS doens't work for some reason
//            console.debug("playing match item " + page.itemPlaying + " " + typeof(page.itemPlaying))
//            if (itemPlaying) {
//                if (itemPlaying.gone) {
//                    console.debug("AAAAAAAAAAAA")
//                }

//                itemPlaying.cancelImplicitVodFileFetch()
//                itemPlaying = null
//            }
            break
        }
    }

//    Component.onDestruction: Global.performOwnerGone(recentlyWatchedVideoView.matchItems)
}

