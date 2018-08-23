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
    property int _vodCount: 0
    property int _seenVodCount: 0

    SqlVodModel {
        columns: ["count"]
        dataManager: VodDataManager
        onModelReset: {
//            console.debug("sql model reset")
            _vodCount = data(index(0, 0), 0)
        }

        Component.onCompleted: update()

        function update() {
            select = ""
            select = "select count(*) as count from vods"
            _vodCount = data(index(0, 0), 0)
        }
    }

    SqlVodModel {
        columns: ["count"]
        dataManager: VodDataManager
        onModelReset: {
//            console.debug("sql model reset")
            _seenVodCount = data(index(0, 0), 0)
        }

        Component.onCompleted: update()

        function update() {
            select = ""
            select = "select count(*) as count from vods where seen=1"
            _seenVodCount = data(index(0, 0), 0)
        }
    }

    VisualItemModel {
        id: model

        DetailItem {
            label: "No VODs"
            value: _vodCount.toFixed(0)
        }

        DetailItem {
            label: "No new VODs"
            value: (_vodCount - _seenVodCount).toFixed(0)
        }

        DetailItem {
            label: "No VOD fetches"
            value: settingNumberOfUpdates.value.toFixed(0)
        }

        DetailItem {
            label: "Last VOD fetch"
            value: Qt.formatDateTime(new Date(settingLastUpdateTimestamp.value * 1000))
        }

        Label {
            // spacer
        }


        ButtonLayout {
            width: root.width

            Button {
                text: "Reset"
                onClicked: {
                    console.debug("reset stats")
                    settingNumberOfUpdates.value = 0
                    settingLastUpdateTimestamp.value = 0
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
                title: "Stats"
            }
        }
    }
}

