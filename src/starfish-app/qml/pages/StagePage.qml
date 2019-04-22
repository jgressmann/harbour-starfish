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
    property string table
    property string where
    property alias stage: pageHeader.title
    property bool _sameDate: false
    property bool _sameSides: false
    property bool _hasTwoSides: false
    property date _matchDate
    property string _side1
    property string _side2
    property int _side1Race
    property int _side2Race
    property int _videoId: -1
    readonly property bool _hasValidRaces: _side1Race >= 1 && _side2Race >= 1

    SqlVodModel {
        id: sqlModel
        database: VodDataManager.database
        columns: ["id", "side1_race", "side2_race"]
        columnAliases: {
            var x = {}
            x["id"] = "vod_id"
            return x
        }
        select: "select " + columns.join(",") + " from " + table + where + " order by match_date desc, match_number asc, match_name desc"
        Component.onCompleted: {
            _side1Race = data(index(0, 1))
            _side2Race = data(index(0, 2))
        }
    }

    SqlVodModel {
        id: matchModel
        database: VodDataManager.database
        columns: ["match_date"]
        select: "select distinct " + columns.join(",") + " from " + table + where

        onModelReset: _updateSameMatchDate()
        Component.onCompleted: _updateSameMatchDate()
    }

    SqlVodModel {
        id: sidesModel
        database: VodDataManager.database
        columns: ["side1_name", "side2_name"]
        select: "select distinct " + columns.join(",") + " from " + table + where + " and side2_name is not null"

        onModelReset: _updateSameSides()
        Component.onCompleted: _updateSameSides()
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

    contentItem: SilicaFlickable {
        anchors.fill: parent

        // Why is this necessary?
        contentWidth: parent.width


        VerticalScrollDecorator {}
        TopMenu {}

        ContentPageHeader {
            id: pageHeader
        }

        Column {
            id: fixture
            x: Theme.horizontalPageMargin
            width: parent.width - 2*x
            anchors.top: pageHeader.bottom

            SidesBar {
                id: sidesBar
                visible: _hasValidRaces && _sameSides && !!_side2
                side1Label: _side1
                side2Label: _side2
                side1IconSource: VodDataManager.raceIcon(_side1Race)
                side2IconSource: VodDataManager.raceIcon(_side2Race)
                fontSize: Theme.fontSizeExtraLarge
                imageHeight: Theme.iconSizeMedium
                imageWidth: Theme.iconSizeMedium
                spacing: Theme.paddingSmall
                color: Theme.highlightColor
            }

            Label {
                id: sidesLabel
                visible: !_hasValidRaces && _sameSides && !!_side2
                width: parent.width
                text: _side1 + " - " + _side2
                truncationMode: TruncationMode.Fade
                font.pixelSize: Theme.fontSizeExtraLarge
                color: Theme.highlightColor
            }

            Label {
                id: dateLabel
                visible: _sameDate
                width: parent.width
                text: Qt.formatDate(_matchDate)
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.highlightColor
            }

            Item { // spacer to move list view down a little
                width: parent.width
                height: sidesBar.visible || sidesLabel.visible || dateLabel.visible ? Theme.paddingSmall : 0
            }
        }

        HighlightingListView {
            id: listView

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.top: fixture.bottom
            model: sqlModel
            clip: true



            delegate: MatchItem {
                width: listView.width
                rowId: vod_id
                showDate: !_sameDate
                showSides: _hasTwoSides && !_sameSides
                memory: matchItemConnections

                onPlayRequest: function (self) {
                    _videoId = vod_id
                    Global.playVideoHandler(updater, VodDataManager.recentlyWatched.vodKey(self.rowId), self.playlist.vodUrl, self.playlist.startOffset, self.seen)
                }
            }

            ViewPlaceholder {
                enabled: listView.count === 0
                text: Strings.noContent
            }
        }
    }

    function _updateSameMatchDate() {
        _sameDate = matchModel.rowCount() === 1
        if (_sameDate) {
            _matchDate = matchModel.data(matchModel.index(0, 0))
        }
    }


    function _updateSameSides() {
        var count = sidesModel.rowCount()
        _hasTwoSides = count > 0
        _sameSides = count === 1
        if (_sameSides) {
            _side1 = sidesModel.data(sidesModel.index(0, 0))
            _side2 = sidesModel.data(sidesModel.index(0, 1))
        }
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

