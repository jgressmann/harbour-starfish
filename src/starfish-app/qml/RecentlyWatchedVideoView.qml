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

import "."
import "pages"

SilicaListView {
    id: listView
    readonly property string _table: Global.defaultTable

    model: VodDataManager.recentlyWatched

    signal clicked(var key, string playbackUrl, int offset, var matchItem)

    MatchItemMemory {
        id: matchItemConnections
    }

    delegate: Component {
        Loader {
            onLoaded: {
                console.debug("loaded video_id="+video_id+" url="+url)
            }

            sourceComponent: url ? fileComponent : vodComponent

            Component {
                id: fileComponent


                ListItem {
                    id: listItem

                    width: listView.width
                    contentHeight: Global.itemHeight + Theme.fontSizeMedium

                    menu: Component {
                        ContextMenu {
                            MenuItem {
                                //% "Remove from list"
                                text: qsTrId("recently-watched-video-view-remove-from-list")
                                onClicked: {
                                    // not sure why I need locals here
                                    var removeArgs = VodDataManager.recentlyWatched.urlKey(url)
                                    var model = VodDataManager.recentlyWatched
                                    listItem.remorseAction(
                                        //% "Removing %1"
                                        qsTrId("recently-watched-video-view-removing").arg(labelLabel.text),
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
                        listView.clicked(VodDataManager.recentlyWatched.urlKey(url),
                                         url,
                                         offset,
                                         null)
                    }


//                    property date modififedChangedListener: modified
//                    onModififedChangedListenerChanged: {
//                        console.debug("index="+ index+" modified=" + modified)
//                        thumbnail.source = ""
//                        thumbnail.source = thumbnail_path
//                    }

                    Component.onCompleted: {
                        console.debug("index=" + index + " offset=" + offset + " url=" + url)
                    }
                }
            }


            Component {
                id: vodComponent

                MatchItem {
                    id: matchItem
                    width: listView.width

                    rowId: video_id
                    memory: matchItemConnections

                    onPlayRequest: function (self) {
                        listView.clicked(VodDataManager.recentlyWatched.vodKey(self.rowId), self.vodUrl, self.startOffset, self)
                    }

                    menu: Component {
                        ContextMenu {
                            MenuItem {
                                //% "Remove from list"
                                text: qsTrId("recently-watched-video-view-remove-from-list")
                                onClicked: {
                                    // not sure why I need locals here
                                    var removeArgs = VodDataManager.recentlyWatched.vodKey(matchItem.rowId)
                                    var model = VodDataManager.recentlyWatched //
                                    matchItem.remorseAction(
                                        //% "Removing %1"
                                        qsTrId("recently-watched-video-view-removing").arg(matchItem.title),
                                        function() {
                                            model.remove(removeArgs)
                                        })
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    ViewPlaceholder {
        //% "No recent videos."
        text: qsTrId("recently-watched-video-view-no-content")
        //% "This list will fill with the videos you have watched."
        hintText: qsTrId("recently-watched-video-view-no-content-hint")
    }

    function clear() {
        VodDataManager.recentlyWatched.clear()
    }
}
