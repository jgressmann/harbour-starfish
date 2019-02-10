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

ContentPage {
    id: page

    objectName: "NewPage"

    property int _videoId: -1
    property string table
    property string constraints: ""

    SqlVodModel {
        id: sqlModel
        database: VodDataManager.database
        columns: ["event_full_name", "stage_name", "side1_name", "side2_name", "side1_race", "side2_race", "match_date", "match_name", "id", "offset", "offset", "length", "url"]
        columnAliases: {
            var x = {}
            x["id"] = "vod_id"
            x["length"] = "vod_length"
            x["url"] = "vod_url"
            return x
        }
        select: "select " + columns.join(",") + " from " + table + " where "
                + (constraints.length > 0 ? constraints : "1=1")
                + " order by match_date desc, event_full_name asc, match_name asc"
    }

    RecentlyWatchedVideoUpdater {
        id: updater

        onDataSaved: function (key, offset, thumbnailFilePath) {
            matchItemConnections.updateStartOffset(_videoId)
        }
    }

    MatchItemMemory {
        id: matchItemConnections
    }

    SilicaFlickable {
        anchors.fill: parent

        // Why is this necessary?
        contentWidth: parent.width

        VerticalScrollDecorator {}
        TopMenu {}



        HighlightingListView {
            id: listView
            anchors.fill: parent
            model: sqlModel
            header: ContentPageHeader {
                title: qsTr("New")
            }



            delegate: Component {
                MatchItem {
                    width: listView.width
//                    contentHeight: Global.itemHeight + Theme.fontSizeMedium // MatchItem is a ListItem
                    eventFullName: event_full_name
                    stageName: stage_name
                    side1: side1_name
                    side2: side2_name
                    race1: side1_race
                    race2: side2_race
                    matchName: match_name
                    matchDate: match_date
                    rowId: vod_id
                    startOffset: offset
                    baseOffset: offset
                    table: page.table
                    length: vod_length
                    url: vod_url
                    memory: matchItemConnections

                    onClicked: {
                        ListView.view.currentIndex = index
                    }

                    onPlayRequest: function (self, callback) {
                        _videoId = vod_id
                        Global.playVideoHandler(updater, VodDataManager.recentlyWatched.vodKey(self.rowId), self.vodUrl, self.startOffset, self.seen)
                    }
                }
            }

            ViewPlaceholder {
                enabled: listView.count === 0
                text: "There are no new VODs available."
                hintText: "Pull down to fetch new VODs."
            }
        }
    }

    Component.onCompleted: {
        listView.positionViewAtBeginning()
    }

    onStatusChanged: {
        switch (status) {
        case PageStatus.Activating:
            matchItemConnections.cancelImplicitVodFileFetch(_videoId)
            break
        }
    }

    Component.onDestruction: Global.performOwnerGone(listView.contentItem)
}

