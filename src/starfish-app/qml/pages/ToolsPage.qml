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

import ".."


Page {
    id: root

    VisualItemModel {
        id: model

        SectionHeader {
            text: "Fetch"
        }

        TextField {
            width: parent.width
            label: "VOD fetch marker"
            text: VodDataManager.downloadMarker
            readOnly: true
        }


        ButtonLayout {
            width: parent.width

            Button {
                text: "Reset VOD fetch marker"
                anchors.horizontalCenter: parent.horizontalCenter
                onClicked: {
                    VodDataManager.resetDownloadMarker()
                }
            }

            Button {
                visible: debugApp.value
                text: "Reset last fetch timestamp"
                onClicked: {
                    console.debug("reset last fetch timestamp")
                    settingLastUpdateTimestamp.value = 0
                }
            }
        }



        SectionHeader {
            text: "Data"
        }

        ButtonLayout {
            width: parent.width

            Button {
                text: "Clear VOD data"
//                anchors.horizontalCenter: parent.horizontalCenter
                onClicked: {
                    var dialog = pageStack.push(
                                Qt.resolvedUrl("ConfirmClearDialog.qml"),
                                {
                                    acceptDestination: Qt.resolvedUrl("StartPage.qml"),
                                    acceptDestinationAction: PageStackAction.Replace
                                })
                    dialog.accepted.connect(function() {
                        console.debug("clear")
                        VodDataManager.clear()
                        VodDataManager.fetchIcons()
                    })
                }
            }

            Button {
                text: "Delete seen VOD files"
                onClicked: {
                    console.debug("delete seen vod files")
                    var count = VodDataManager.deleteSeenVodFiles()
                    if (count) {
                        deleteVodNotification.itemCount = count
                        deleteVodNotification.body = deleteVodNotification.previewBody = count + " seen VOD file" + (count > 1 ? "s" : "") + " deleted"
                        deleteVodNotification.publish()
                    }
                }
            }

            Button {
                text: "Reset recent videos"
                onClicked: {
                    recentlyUsedVideos.recreateTable()
                }
            }

            Button {
                visible: debugApp.value
                text: "Send new VODs notification"
                onClicked: {
                    console.debug("sending new vods notification")
                    newVodNotification.itemCount = 42
                    newVodNotification.body = newVodNotification.previewBody = "debug notification"
                    newVodNotification.publish()
                }
            }



            Button {
                visible: debugApp.value
                text: "Reset SQL patch level"
                onClicked: {
                    console.debug("reset sql patch level")
                    VodDataManager.resetSqlPatchLevel()
                }
            }
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentWidth: parent.width

        VerticalScrollDecorator {}

        SilicaListView {
            id: listView
            anchors.fill: parent
            model: model
            header: PageHeader {
                title: "Tools"
            }
        }
    }
}

