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
import ".."

BasePage {
    id: page

    VisualItemModel {
        id: visualModel

        ListItem {
            id: newVodsItem
            contentHeight: visible ? Global.itemHeight : 0

            property string constraints

            Label {
                x: Theme.horizontalPageMargin
                width: page.width - 2*x
                //% "New"
                text: qsTrId("sf-entry-page-new")

                font.pixelSize: Global.labelFontSize
                anchors.verticalCenter: parent.verticalCenter
            }

            onClicked: Global.openNewVodPage(pageStack)

            SqlVodModel {
                id: newModel
                database: VodDataManager.database
                columns: ["count"]
            }

            function update() {
                newModel.reload()
                if (Global.getNewVodsContstraints) { // gets called really early during setup of application page (before its .onCompleted runs)
                    constraints = Global.getNewVodsContstraints()
                    newModel.select = "select count(*) from " + Global.defaultTable + " where " + constraints
                    visible = newModel.data(newModel.index(0, 0)) > 0
                } else {
                    visible = false
                }
            }

            Component.onCompleted: {
                VodDataManager.vodsChanged.connect(update)
                VodDataManager.seenChanged.connect(update)
                update()
            }

            Component.onDestruction: {
                VodDataManager.vodsChanged.disconnect(update)
                VodDataManager.seenChanged.disconnect(update)
            }

            Connections {
                target: Global
                onGetNewVodsContstraintsChanged: {
                    if (Global.getNewVodsContstraints) {
                        target = null // one shot
                        newVodsItem.update()
                    }
                }
            }
        }

        ListItem {
            id: unseenVodsItem
            contentHeight: visible ? Global.itemHeight : 0

            Label {
                id: unseenVodsTitleLabel
                x: Theme.horizontalPageMargin
                width: page.width - 2*x

                //% "Not yet watched"
                text: qsTrId("sf-entry-page-unwatched")

                font.pixelSize: Global.labelFontSize
                anchors.verticalCenter: parent.verticalCenter
            }

            onClicked: {
                var props = {}
                //% "Unwatched"
                props[Global.propBreadcrumbs] = [qsTrId("sf-entry-page-unwatched-breadcrumb")]
                props[Global.propHiddenFlags] = 0
                props[Global.propKey] = "game"
                props[Global.propTable] = Global.defaultTable
                props[Global.propTitle] = VodDataManager.label("game")
                props[Global.propWhere] = " where hidden=0 and seen=0"


                pageStack.push(
                            Qt.resolvedUrl("FilterPage.qml"),
                            {
                                props: props,
                                grid: true,
                            })
            }

            SqlVodModel {
                id: unseenModel
                database: VodDataManager.database
                columns: ["count"]
                select: "select count(*) from vods where hidden=0 and seen=0"
            }

            function update() {
                unseenModel.reload()
                visible = unseenModel.data(unseenModel.index(0, 0)) > 0
            }

            Component.onCompleted: {
                VodDataManager.vodsChanged.connect(update)
                VodDataManager.seenChanged.connect(update)
                update()
            }

            Component.onDestruction: {
                VodDataManager.vodsChanged.disconnect(update)
                VodDataManager.seenChanged.disconnect(update)
            }
        }

        ListItem {
            contentHeight: Global.itemHeight

            Label {
                id: browseVodsTitleLabel
                x: Theme.horizontalPageMargin
                width: page.width - 2*x
                //% "Browse"
                text: qsTrId("sf-entry-page-browse")

                font.pixelSize: Global.labelFontSize
                anchors.verticalCenter: parent.verticalCenter
            }

            onClicked: {
                var props = {}
                props[Global.propBreadcrumbs] = [browseVodsTitleLabel.text]
                props[Global.propHiddenFlags] = 0
                props[Global.propKey] = "game"
                props[Global.propTable] = Global.defaultTable
                props[Global.propTitle] = VodDataManager.label("game")
                props[Global.propWhere] = " where hidden=0"

                pageStack.push(
                            Qt.resolvedUrl("FilterPage.qml"),
                            {
                                props: props,
                                grid: true,
                            })
            }
        }

        ListItem {
            id: offlineVodsItem
            contentHeight: visible ? Global.itemHeight : 0

            Label {
                id: offlineVodsTitleLabel
                x: Theme.horizontalPageMargin
                width: page.width - 2*x
                //% "Offline available"
                text: qsTrId("sf-entry-page-offline-available")

                font.pixelSize: Global.labelFontSize
                anchors.verticalCenter: parent.verticalCenter
            }

            onClicked: {
                var props = {}
                //% "Offline"
                props[Global.propBreadcrumbs] = [qsTrId("sf-entry-page-offline-breadcrumb")]
                props[Global.propHiddenFlags] = 0
                props[Global.propKey] = "game"
                props[Global.propTable] = "offline_vods"
                props[Global.propTitle] = VodDataManager.label("game")
                props[Global.propWhere] = " where hidden=0 and progress>0"

                pageStack.push(
                            Qt.resolvedUrl("FilterPage.qml"),
                            {
                                props: props,
                                grid: true,
                            })
            }

            SqlVodModel {
                id: offlineModel
                database: VodDataManager.database
                columns: ["count"]
                select: "select count(*) from offline_vods where hidden=0 and progress>0"
            }

            function update() {
                offlineModel.reload()
                visible = offlineModel.data(offlineModel.index(0, 0)) > 0
            }

            Component.onCompleted: {
                VodDataManager.vodsChanged.connect(update)
                update()
            }

            Component.onDestruction: {
                VodDataManager.vodsChanged.disconnect(update)
            }
        }

        ListItem {
            id: hiddenVodsItem
            contentHeight: visible ? Global.itemHeight : 0

            Label {
                id: hiddenVodsTitleLabel
                x: Theme.horizontalPageMargin
                width: page.width - 2*x
                //% "Hidden"
                text: qsTrId("sf-entry-page-hidden")

                font.pixelSize: Global.labelFontSize
                anchors.verticalCenter: parent.verticalCenter
            }

            onClicked: {
                var props = {}
                props[Global.propBreadcrumbs] = [hiddenVodsTitleLabel.text]
                props[Global.propHiddenFlags] = VodDataManager.HT_Deleted
                props[Global.propKey] = "game"
                props[Global.propTable] =Global.defaultTable
                props[Global.propTitle] = VodDataManager.label("game")
                props[Global.propWhere] = " where hidden!=0"
                pageStack.push(
                            Qt.resolvedUrl("FilterPage.qml"),
                            {
                                props: props,
                                grid: true,
                            })
            }

            SqlVodModel {
                id: hiddenModel
                database: VodDataManager.database
                columns: ["count"]
                select: "select count(*) from " + Global.defaultTable + " where hidden!=0"
            }

            function update() {
                hiddenModel.reload()
                visible = hiddenModel.data(hiddenModel.index(0, 0)) > 0
            }

            Component.onCompleted: {
                VodDataManager.vodsChanged.connect(update)
                update()
            }

            Component.onDestruction: {
                VodDataManager.vodsChanged.disconnect(update)
            }
        }

        ListItem {
            id: downloadingVodsItem
            contentHeight: visible ? Global.itemHeight : 0
            visible: VodDataManager.vodDownloads > 0

            Label {
                x: Theme.horizontalPageMargin
                width: page.width - 2*x
                //% "Active downloads"
                text: qsTrId("sf-entry-page-active-downloads")

                font.pixelSize: Global.labelFontSize
                anchors.verticalCenter: parent.verticalCenter
            }

            onClicked: pageStack.push(Qt.resolvedUrl("ActiveDownloadPage.qml"))
        }

        ListItem {
            id: recentlyWatchedVideosItem
            contentHeight: visible ? continueWatching.height : 0
            visible: recentlyWatchedVideoView.height > 0

            menu: ContextMenu {
                MenuItem {
                    //% "Clear"
                    text: qsTrId("sf-entry-page-clear-recently-watched-videos")
                    onClicked: recentlyWatchedVideosItem.remorseAction(
                                   //% "Clearing recently watched videos"
                                   qsTrId("sf-entry-page-clearing-recently-watched-videos"),
                                   function () { recentlyWatchedVideoView.clear() })
                }
            }

            Column {
                id: continueWatching
                width: parent.width

                Label {
                    visible: recentlyWatchedVideoView.height > 0
                    x: Theme.horizontalPageMargin
                    width: page.width - 2*x
                    //% "Continue watching"
                    text: qsTrId("sf-entry-page-continue-watching")
                    color: Theme.highlightColor
                    font.pixelSize: Global.labelFontSize
                }

                RecentlyWatchedVideoView {
                    id: recentlyWatchedVideoView
                    height: 0
                    width: parent.width

                    onClicked: function (playlist, seen) {
                        window.playPlaylist(playlist, seen)
                    }

                    onCountChanged: {
                        continueWatching.visible = count > 0
                        /* overestimate, so that view is long enough */
                        height = (Global.itemHeight + Theme.fontSizeLarge)  * count
                    }
                }
            }
        }


    }

    contentItem: SilicaFlickable {
        anchors.fill: parent

        // Why is this necessary?
        contentWidth: parent.width

        VerticalScrollDecorator {}
        TopMenu {}

        SilicaListView {
            anchors.fill: parent
            model: visualModel
            header: PageHeader {
                //% "go go go"
                title: qsTrId("sf-entry-page-title")

                VodDataManagerBusyIndicator {}
            }
        }
    }


//    Component.onDestruction: Global.performOwnerGone(recentlyWatchedVideoView.matchItems)
}

