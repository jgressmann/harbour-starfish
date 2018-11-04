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
    readonly property string _table: Global.defaultTable

    model: recentlyUsedVideos

    signal clicked(var obj, string playbackUrl, int offset, var matchItem)

//    property var matchItems: []
    MatchItemConnections {
        id: matchItemConnections
    }

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
                                    // not sure why I need locals here
                                    var removeArgs = {url: url}
                                    var model = recentlyUsedVideos
                                    listItem.remorseAction(
                                        "Removing " + labelLabel.text,
                                        function() {
                                            model.remove(removeArgs)
                                        })
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
                                    visible: status === Image.Ready
                                    source: thumbnail_path
                                }

                                Image {
                                    anchors.fill: parent
                                    fillMode: Image.PreserveAspectFit
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
                                id: labelLabel
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
                                         offset,
                                         null)
                    }


                    property date modififedChangedListener: modified
                    onModififedChangedListenerChanged: {
                        console.debug("index="+ index+" modified=" + modified)
                        thumbnail.source = ""
                        thumbnail.source = thumbnail_path
                    }

                    Component.onCompleted: {
                        console.debug("index=" + index + " offset=" + offset + " url=" + url)
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

                    readonly property int xindex: index
                    readonly property int xoffset: offset
                    readonly property int xvideo_id: video_id

                    delegate: Component {
                        MatchItem {
                            id: matchItem
                            width: ListView.view.width
//                            contentHeight: Global.itemHeight + Theme.fontSizeMedium
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
                            baseOffset: vod_offset
                            length: vod_length
                            table: _table
                            connectionHandler: matchItemConnections

                            onPlayRequest: function (self) {
                                console.debug("xvideo_id=" + xvideo_id)
                                console.debug("video_id=" + video_id)
                                console.debug("rowid=" + rowId)
                                if (video_id === -1) {
                                    console.debug("rowid=" + rowId)
                                    console.debug("ARRRRRRRRRRRRRRRRRRRRRRRRRRRRGGGGGGGGGGGGGGGGGGG")
                                }

                                listView.clicked({video_id: rowId}, self.vodUrl, xoffset, self)
                            }



                            menu: Component {
                                ContextMenu {
                                    MenuItem {
                                        text: "Remove from list"
                                        onClicked: {
                                            // not sure why I need locals here
                                            var removeArgs = { video_id: matchItem.rowId }
                                            var model = recentlyUsedVideos //
                                            matchItem.remorseAction(
                                                "Removing " + matchItem.title,
                                                function() {
                                                    model.remove(removeArgs)
                                                })
                                        }
                                    }
                                }
                            }

//                            Component.onCompleted: {
//                                console.debug("pre completed item index " + xindex + " matchItems: " + matchItems)
//                                matchItems.push(matchItem)
//                                console.debug("post completed item index " + xindex + " matchItems: " + matchItems)
//                            }

//                            Component.onDestruction: {

//                                if (listView.matchItems) { // aparently happens on destruction
//                                    console.debug("pre destruction index " + xindex + " matchItems: " + listView.matchItems)
//                                    var i = listView.matchItems.indexOf(matchItem)
//                                    if (i >= 0) {
//                                        listView.matchItems[i] = listView.matchItems[listView.matchItems.length-1]
//                                        listView.matchItems.pop()
//                                    }
//                                    console.debug("post destruction index " + xindex + " matchItems: " + listView.matchItems)
//                                }

//                            }
                        }
                    }

                    SqlVodModel {
                        id: sqlModel
                        dataManager: VodDataManager
                        columns: ["event_full_name", "stage_name", "side1_name", "side2_name", "side1_race", "side2_race", "match_date", "match_name", "offset", "length"]
                        columnAliases: {
                            var x = {}
                            x["length"] = "vod_length"
                            x["offset"] = "vod_offset"
                            return x
                        }
                        select: "select " + columns.join(",") + " from " + _table + " where id=" + video_id
                    }

                    Component.onCompleted: {
                        console.debug("index=" + index + " offset=" + offset + " video_id=" + video_id)
                    }
                }
            }
        }
    }

    ViewPlaceholder {
        text: "No recent videos."
        hintText: "This list will fill with the videos you have watched."
    }

    function getMatchItemById(_id) {
//        if (matchItems) {
//            for (var i = 0; i < matchItems.length; ++i) {
//                var item = matchItems[i]
//                if (item.rowId === _id) {
//                    return item
//                }
//            }
//        }
    }

    function clear() {
        recentlyUsedVideos.clear()
    }

//    Component.onDestruction: {
//        console.debug("RecentlyWatchedVideoView destruction, matchItems: " + matchItems)
//    }
}
