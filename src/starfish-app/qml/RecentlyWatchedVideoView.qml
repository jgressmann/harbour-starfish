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
import "pages"

SilicaListView {
    id: listView

    model: recentlyUsedVideos

    signal clicked(var obj, string playbackUrl, int offset)



    delegate: Component {
        Loader {
            onLoaded: {
                console.debug("loaded video_id="+video_id+" url="+url)
            }

            sourceComponent: video_id === -1 ? fileComponent : vodComponent

            Component {
                id: fileComponent


                ListItem {
                    id: listItem

                    width: listView.width
                    contentHeight: Global.itemHeight + Theme.fontSizeMedium


                    menu: Component {
                        ContextMenu {
                            MenuItem {
                                text: "Remove from list"
                                onClicked: {
                                    recentlyUsedVideos.remove({url: url})
                                }
                            }
                        }
                    }

                    Column {
                        id: heading
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2*x

                        Label {
                            id: directoryLabel
                            width: parent.width
                            visible: !!text
                            truncationMode: TruncationMode.Fade
                            font.pixelSize: Theme.fontSizeSmall
                            text: {
                                // only show file name for files
                                if (url && (url.indexOf("file:///") === 0 || url.indexOf("/") === 0)) {
                                    var str = url
                                    if (str.indexOf("file:///") === 0) {
                                        str = url.substr(7)
                                    }

                                    var lastSlashIndex = str.lastIndexOf("/")
                                    return str.substr(0, lastSlashIndex)
                                }

                                return ""
                            }
                        }

                        Item {
        //                            color: "transparent"
                            width: parent.width
                            height: listItem.contentHeight - directoryLabel.height

                            Item {
                                id: thumbnailGroup
                                width: Global.itemHeight
                                height: Global.itemHeight
                                anchors.verticalCenter: parent.verticalCenter

                                Image {
                                    id: thumbnail
                                    anchors.fill: parent
                                    sourceSize.width: width
                                    sourceSize.height: height
                                    fillMode: Image.PreserveAspectFit
                                    cache: false
                                    visible: status === Image.Ready
                                    source: thumbnail_path
                                }

                                Image {
                                    anchors.fill: parent
                                    fillMode: Image.PreserveAspectFit
//                                    cache: false
                                    visible: !thumbnail.visible
                                    source: "image://theme/icon-m-sailfish"
                                }
                            }

                            Item {
                                id: imageSpacer
                                width: Theme.paddingMedium
                                height: parent.height

                                anchors.left: thumbnailGroup.right
                            }

                            Label {
                                anchors.left: imageSpacer.right
                                anchors.right: parent.right
                                anchors.top: parent.top
        //                                anchors.bottom: dirLabel.top
                                anchors.bottom: parent.bottom
                                verticalAlignment: Text.AlignVCenter
                                text: {
                                    // only show file name for files
                                    if (url && (url.indexOf("file:///") === 0 || url.indexOf("/") === 0)) {
                                        var lastSlashIndex = url.lastIndexOf("/")
                                        return url.substr(lastSlashIndex + 1)
                                    }

                                    return url
                                }

        //                            font.pixelSize: Global.labelFontSize
//                                truncationMode: TruncationMode.Fade
                                wrapMode: Text.WrapAnywhere
                                elide: Text.ElideRight
                            }
                        }
                    }


                    onClicked: {
                        console.debug("url=" + url)
                        listView.clicked({
                                             url: url
                                         },
                                         url,
                                         offset)
                    }


                    property date modififedChangedListener: modified
                    onModififedChangedListenerChanged: {
                        console.debug("index="+ index+" modified=" + modified)
                        thumbnail.source = ""
                        thumbnail.source = thumbnail_path
                    }

                    Component.onCompleted: {
//                        console.debug("index=" + index + " modified=" + modified + " rowid=" + rowid + " video id=" + video_id + " url=" + url + " offset=" + offset + " thumbnail=" + thumbnail_path)
//                        console.debug("index=" + index + " thumbnail=" + thumbnail_path)
                    }
                }

            }


            Component {
                id: vodComponent


                SilicaListView {
                    id: wrapperView
                    width: listView.width


                    quickScrollEnabled: false
//                                clip: true
                    model: sqlModel


                    delegate: Component {
                        MatchItem {
                            id: matchItem
                            width: ListView.view.width
                            contentHeight: Global.itemHeight + Theme.fontSizeMedium
                            onHeightChanged: wrapperView.height = height

                            eventFullName: event_full_name
                            stageName: stage_name
                            side1: side1_name
                            side2: side2_name
                            race1: side1_race
                            race2: side2_race
                            matchName: match_name
                            matchDate: match_date
                            rowId: video_id
                            startOffset: offset

                            onPlayRequest: function (self) {
                                console.debug("video_id=" + video_id)
                                console.debug("rowid=" + rowId)
                                if (video_id === -1) {
                                    console.debug("rowid=" + rowId)
                                    console.debug("ARRRRRRRRRRRRRRRRRRRRRRRRRRRRGGGGGGGGGGGGGGGGGGG")
                                }

                                listView.clicked({
                                                    video_id: rowId
                                                 },
                                                 self.vodUrl,
                                                 self.startOffset)
                            }

                            menu: Component {
                                ContextMenu {
                                    MenuItem {
                                        text: "Remove from list"
                                        onClicked: {
                                            recentlyUsedVideos.remove({ video_id: matchItem.rowId })
                                        }
                                    }
                                }
                            }
                        }
                    }

                    SqlVodModel {
                        id: sqlModel
                        dataManager: VodDataManager
                        columns: ["event_full_name", "stage_name", "side1_name", "side2_name", "side1_race", "side2_race", "match_date", "match_name", "offset"]
                        select: "select " + columns.join(",") + " from vods where id=" + video_id
                    }

                    Component.onCompleted: {
//                        sqlModel.select = "select " + sqlModel.columns.join(",") + " from vods where id=" + video_id
                    }
                }
            }
        }
    }

    ViewPlaceholder {
        text: "No recent videos."
        hintText: "This list will fill with the videos you have watched."
    }
}
