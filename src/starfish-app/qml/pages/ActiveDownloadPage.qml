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

    property var itemPlaying: null
    readonly property string _table: Global.defaultTable

    SqlVodModel {
        id: sqlModel
        dataManager: VodDataManager
        columns: ["event_full_name", "stage_name", "side1_name", "side2_name", "side1_race", "side2_race", "match_date", "match_name", "id", "offset", "length", "url"]
        columnAliases: {
            var x = {}
            x["id"] = "vod_id"
            x["length"] = "vod_length"
            x["url"] = "vod_url"
            return x
        }
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
                title: qsTr("Active downloads")
            }

            RecentlyWatchedVideoUpdater {
                id: updater
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
                    table: _table
                    length: vod_length
                    url: vod_url

                    onClicked: {
                        ListView.view.currentIndex = index
                    }

                    onPlayRequest: function (self, callback) {
                        itemPlaying = self
                        Global.playVideoHandler(updater, {video_id: vod_id}, self.vodUrl, self.startOffset)
                    }

                    function updateStartOffset() {
                        var rows = recentlyUsedVideos.select(["offset"], {video_id: vod_id});
                        if (rows.length === 1) {
                            startOffset = rows[0].offset
                        } else {
                            startOffset = offset // base offset into multi match video
                        }

    //                    console.debug("rowid=" + vod_id + " start=" + Global.secondsToTimeString(startOffset))
                    }

                    Component.onCompleted: updateStartOffset()
                }
            }

            ViewPlaceholder {
                enabled: listView.count === 0
                text: "There are no active downloads."
            }
        }
    }

    Component.onCompleted: {
        listView.positionViewAtBeginning()
        VodDataManager.vodDownloadsChanged.connect(_vodDownloadsChanged)
        VodDataManager.vodsChanged.connect(_vodDownloadsChanged)
        _vodDownloadsChanged()
    }

    onStatusChanged: {
        switch (status) {
        case PageStatus.Activating:
            if (itemPlaying) {
                itemPlaying.cancelImplicitVodFileFetch()
                itemPlaying = null
            }
            break
        }
    }

    Component.onDestruction: Global.performOwnerGone(listView.contentItem)

    function _vodDownloadsChanged() {
        sqlModel.select =
                "select "
                + sqlModel.columns.join(",")
                + " from " + _table + " where id in ("
                + VodDataManager.vodsBeingDownloaded().join(",")
                + ") order by match_date desc, event_full_name asc, match_name asc"
    }
}

