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

Page {
    id: page

    readonly property int vodCount: _vodCount
    property int _vodCount: 0

    onVodCountChanged: {
        console.debug("vod count="+ vodCount)
        _tryNavigateAway()
    }

    onStatusChanged: _tryNavigateAway()


    SqlVodModel {
        id: model
        columns: ["count"]
        database: VodDataManager.database
        select: "select count(*) as count from vods"
        onModelReset: {
            console.debug("sql model reset")
            _vodCount = data(index(0, 0), 0)
        }

        Component.onCompleted: {
            VodDataManager.vodsChanged.connect(reload)
            update()
        }

        function update() {
            //select = ""
            //select = "select count(*) as count from vods"
            reload()
            _vodCount = data(index(0, 0), 0)
        }
    }

    SilicaFlickable {
        anchors.fill: parent

        // Why is this necessary?
        contentWidth: parent.width

        TopMenu {}

        ViewPlaceholder {
            enabled: VodDataManager.ready && vodCount === 0 && vodDatabaseDownloader.status !== VodDatabaseDownloader.Status_Downloading
            text: Strings.noContent
            hintText: App.isOnline ? "Pull down to fetch new VODs." : "Connect to the internet to fetch new VODs"
        }

        ViewPlaceholder {
            enabled: VodDataManager.ready && vodCount === 0 && vodDatabaseDownloader.status === VodDatabaseDownloader.Status_Downloading
            text: "VODs are being downloaded. It won't be long, now."
        }

        ViewPlaceholder {
            enabled: !VodDataManager.ready
            text: App.displayName + " failed to start."
            hintText: "For details check the log file located in " + App.logDir
        }
    }

    Timer {
        interval: 1000
        running: VodDataManager.ready
        repeat: true
        onTriggered: model.update()
    }

    function _tryNavigateAway() {
        if (PageStatus.Active === status &&
                VodDataManager.ready &&
                vodCount > 0) {
//            pageStack.replace(
//                Qt.resolvedUrl("FilterPage.qml"),
//                {
//                    title: qsTr("Game"),
//                    filters: {},
//                    key: "game",
//                    grid: true,
//                })
//            pageStack.replace(
//                Qt.resolvedUrl("NewPage.qml"))
            pageStack.replace(
                        Qt.resolvedUrl("EntryPage.qml"))
        }
    }
}

