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
import Sailfish.Pickers 1.0
import Vodman 2.0
import org.duckdns.jgressmann 1.0
import ".."


BasePage {
    id: root

    readonly property bool _toolEnabled: !VodDataManager.busy

    RemorsePopup { id: remorse }

    VisualItemModel {
        id: model

        Column {
            width: parent.width
            spacing: Theme.paddingLarge

            SectionHeader {
                //% "Data fetching"
                text: qsTrId("tools-page-fetch-section-header")
            }

            TextField {
                width: parent.width
                //% "VOD fetch marker"
                label: qsTrId("tools-page-vod-fetch-marker")
                text: VodDataManager.downloadMarker
                readOnly: true
                placeholderText: label
            }


            ButtonLayout {
                width: parent.width

                Button {
                    //% "Reset VOD fetch marker"
                    text: qsTrId("tools-page-reset-vod-fetch-marker")
                    anchors.horizontalCenter: parent.horizontalCenter
                    enabled: _toolEnabled
                    onClicked: remorse.execute(text, function() {
                        VodDataManager.resetDownloadMarker()
                    })
                }

                Button {
                    visible: debugApp.value
                    //% "Reset last fetch timestamp"
                    text: qsTrId("tools-page-reset-last-fetch-timestamp")
                    enabled: _toolEnabled
                    onClicked: remorse.execute(text, function() {
                        console.debug(text)
                        settingLastUpdateTimestamp.value = 0
                    })
                }

                Button {
                    //% "Delete sc2links.com state"
                    text: qsTrId("tools-page-delete-sc2links.com-state")
                    enabled: _toolEnabled
                    onClicked: remorse.execute(text, function() {
                        console.debug(text)
                        App.unlink(sc2LinksDotComScraper.stateFilePath)
                    })
                }

                Button {
                    //% "Delete sc2casts.com state"
                    text: qsTrId("tools-page-delete-sc2casts.com-state")
                    enabled: _toolEnabled
                    onClicked: remorse.execute(text, function() {
                        console.debug(text)
                        App.unlink(sc2CastsDotComScraper.stateFilePath)
                    })
                }
            }


            SectionHeader {
                //% "Data"
                text: qsTrId("tools-page-data-section-header")
            }

            ButtonLayout {
                width: parent.width
                Button {
                    //% "Clear"
                    text: qsTrId("tools-page-data-clear")
                    enabled: _toolEnabled
                    onClicked: {
                        var dialog = pageStack.push(
                                    Qt.resolvedUrl("ConfirmClearDialog.qml"),
                                    {
                                        acceptDestination: Qt.resolvedUrl("StartPage.qml"),
                                        acceptDestinationAction: PageStackAction.Replace,
                                        acceptDestinationReplaceTarget: null,
                                    })
                        dialog.accepted.connect(function() {
                            console.debug("clear")
                            VodDataManager.clear()
                            VodDataManager.fetchIcons()
                        })
                    }
                }
            }

            SectionHeader {
                //% "Seen"
                text: qsTrId("tools-page-seen-section-header")
            }

            ButtonLayout {
                width: parent.width

                Button {
                    text: Strings.deleteSeenVodFiles
                    enabled: _toolEnabled
                    onClicked: remorse.execute(text, function() {
                        Global.deleteSeenVodFiles()
                    })
                }

                Button {
                    //% "Reset recent videos"
                    text: qsTrId("tools-page-seen-recently-watched-videos")
                    enabled: _toolEnabled
                    onClicked: VodDataManager.recentlyWatched.clear()
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
            }

            SectionHeader {
                text: "SQL"
                visible: debugApp.value
            }

            TextField {
                visible: debugApp.value
                width: parent.width
                label: "SQL patch level"
                text: VodDataManager.sqlPatchLevel
                readOnly: true
            }

            ButtonLayout {
                width: parent.width

                Button {
                    visible: debugApp.value
                    text: "Reset SQL patch level"
                    onClicked: {
                        console.debug("reset sql patch level")
                        VodDataManager.resetSqlPatchLevel()
                    }
                }
            }

            SectionHeader {
                text: "youtube-dl"
            }

            Column {
                width: parent.width
                spacing: Theme.paddingMedium

                Item {
                    height: Theme.paddingSmall
                    width: parent.width
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2*x
                    color: Theme.highlightColor
                    text: {
                        switch (YTDLDownloader.downloadStatus) {
                        case YTDLDownloader.StatusDownloading:
                            //% "youtube-dl is being downloaded"
                            return qsTrId("tools-ytdl-downloading")
                        case YTDLDownloader.StatusError:
                        case YTDLDownloader.StatusUnavailable:
                            //% "youtube-dl is not available"
                            return qsTrId("tools-ytdl-unavailable")
                        default:
                            //% "youtube-dl version %1"
                            return qsTrId("tools-ytdl-version").arg(YTDLDownloader.ytdlVersion)
                        }
                    }
                }

                ButtonLayout {
                    width: parent.width
                    Button {
                        enabled: YTDLDownloader.isOnline &&
                                 YTDLDownloader.downloadStatus !== YTDLDownloader.StatusDownloading &&
                                 !VodDataManager.busy
                        //% "Update youtube-dl"
                        text: qsTrId("tools-ytdl-update")
                        onClicked: YTDLDownloader.download()
                    }

                    Button {
                        visible: debugApp.value
                        text: "delete youtube-dl"
                        onClicked: YTDLDownloader.remove()
                    }

                    Button {
                        visible: debugApp.value
                        text: "clear cache"
                        onClicked: VodDataManager.clearYtdlCache()
                    }
                }

                Item {
                    height: Theme.paddingSmall
                    width: parent.width
                }
            }

            ExpandingSectionGroup {
                currentIndex: -1
                width: parent.width

                ExpandingSection {
                    //% "Cache"
                    title: qsTrId("tools-page-cache-section-title")
                    id: cacheSection
                    width: parent.width
                    content.sourceComponent: Column {
                        width: cacheSection.width
                        ButtonLayout {
                            width: parent.width


                            Button {
                                //% "Clear meta data"
                                text: qsTrId("tools-page-cache-section-clear-meta-data")
                                enabled: _toolEnabled
                                onClicked: {
                                    console.debug("clear meta data")
                                    remorse.execute(text, function() {
                                        VodDataManager.clearCache(VodDataManager.CF_MetaData)
                                    })

                                }
                            }

                            Button {
                                //% "Clear thumbnails"
                                text: qsTrId("tools-page-cache-section-clear-thumbnails")
                                enabled: _toolEnabled
                                onClicked: {
                                    console.debug("clear thumbnails")
                                    remorse.execute(text, function() {
                                        VodDataManager.clearCache(VodDataManager.CF_Thumbnails)
                                    })
                                }
                            }

                            Button {
                                //% "Clear VOD files"
                                text: qsTrId("tools-page-cache-section-clear-vod-files")
                                enabled: _toolEnabled
                                onClicked: {
                                    console.debug("clear vods")
                                    remorse.execute(text, function() {
                                        VodDataManager.clearCache(VodDataManager.CF_Vods)
                                    })

                                }
                            }

                            Button {
                                //% "Clear icons"
                                text: qsTrId("tools-page-cache-section-clear-icons")
                                enabled: _toolEnabled
                                onClicked: {
                                    console.debug("clear icons")
                                    remorse.execute(text, function() {
                                        VodDataManager.clearCache(VodDataManager.CF_Icons)
                                        VodDataManager.fetchIcons()
                                    })
                                }
                            }
                        }

                        Item {
                            width: parent.width
                            height: Theme.paddingLarge
                        }
                    }
                }
            }
        } // column
    }





    contentItem: SilicaFlickable {
        anchors.fill: parent
        contentWidth: parent.width

        VerticalScrollDecorator {}

        SilicaListView {
            id: listView
            anchors.fill: parent
            model: model
            header: PageHeader {
                //% "Tools"
                title: qsTrId("tools-page-title")
                VodDataManagerBusyIndicator {}
            }
        }
    }
}

