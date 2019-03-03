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

    //property var itemPlaying: null
    property int _videoId: -1
    readonly property string _table: Global.defaultTable

    SqlVodModel {
        id: sqlModel
        database: VodDataManager.database
        columns: ["id"]
        columnAliases: {
            var x = {}
            x["id"] = "vod_id"
            return x
        }
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
                //% "Active downloads"
                title: qsTrId("active-downloads-title")
            }


            delegate: Component {
                MatchItem {
                    width: listView.width
//                    contentHeight: Global.itemHeight + Theme.fontSizeMedium // MatchItem is a ListItem
                    rowId: vod_id
                    memory: matchItemConnections

                    onClicked: {
                        ListView.view.currentIndex = index
                    }

                    onPlayRequest: function (self, callback) {
//                        itemPlaying = self
                        _videoId = vod_id
                        Global.playVideoHandler(updater, VodDataManager.recentlyWatched.vodKey(self.rowId), self.vodUrl, self.startOffset, self.seen)
                    }
                }
            }

            ViewPlaceholder {
                enabled: listView.count === 0
                //% "There are no active downloads."
                text: qsTrId("active-downloads-no-downloads")
            }
        }
    }

    Component.onCompleted: {
        listView.positionViewAtBeginning()
        VodDataManager.vodDownloadsChanged.connect(_vodDownloadsChanged)
        VodDataManager.vodsChanged.connect(_vodDownloadsChanged)
        _vodDownloadsChanged()
    }

    Component.onDestruction: {
        VodDataManager.vodDownloadsChanged.disconnect(_vodDownloadsChanged)
        VodDataManager.vodsChanged.disconnect(_vodDownloadsChanged)
        Global.performOwnerGone(listView.contentItem)
    }

    onStatusChanged: {
        switch (status) {
        case PageStatus.Activating:
            matchItemConnections.cancelImplicitVodFileFetch(_videoId)
            break
        }
    }

    function _vodDownloadsChanged() {
        sqlModel.select =
                "select "
                + sqlModel.columns.join(",")
                + " from " + _table + " where id in ("
                + VodDataManager.vodsBeingDownloaded().join(",")
                + ") order by match_date desc, event_full_name asc, match_name asc"
    }
}

