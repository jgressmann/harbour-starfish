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
//    property string vodEvent
//    property string vodStage
//    property string vodMatch
//    property string vodTitle
    property string vodFilePath
//    property int vodYear: -1
//    property int vodSeason: -1
    property int vodFileSize: -1
    property int vodWidth: -1
    property int vodHeight: -1
    property int vodRowId: -1
    property real vodProgress: 0

//    SqlVodModel {
//        id: sqlModel
//        dataManager: VodDataManager
//        columns: ["side1_name", "side2_name", "side1_race", "side2_race", "match_date", "match_name", "id", "offset", "event_name", "season", "year"]
//        columnAliases: {
//            var x = {}
//            x["id"] = "vod_id"
//            return x
//        }
//        select: "select " + columns.join(",") + " from " + table + where + " order by match_date desc, match_number asc, match_name desc"
//    }

    VisualItemModel {
        id: model

//        DetailItem {
//            label: "Year"
//            value: vodYear
//        }

//        DetailItem {
//            visible: !!vodEvent
//            label: "Event"
//            value: vodEvent
//        }

//        DetailItem {
//            label: "Season"
//            value: vodSeason
//        }

//        DetailItem {
//            visible: !!vodStage
//            label: "Stage"
//            value: vodStage
//        }

//        DetailItem {
//            visible: !!vodMatch
//            label: "Match"
//            value: vodMatch
//        }

        DetailItem {
            label: "ID"
            value: vodRowId
        }

        DetailItem {
            label: "Path"
            value: vodFilePath

//            BackgroundItem {
//                anchors.fill: parent
//                onClicked: {

//                }
//            }
        }



        DetailItem {
            label: "Size"
            value: makeSizeString(vodFileSize)

            function makeSizeString(size) {
                var oneGb = 1000000000
                var oneMb = 1000000

                if (size >= 10*oneGb) { // 10GB
                    return (size/oneGb).toFixed(1) + " GB"
                }

                if (size >= oneGb) { // 1GB
                    return (size/oneGb).toFixed(2) + " GB"
                }

                return (size/oneMb).toFixed(0) + " MB"
            }
        }

        DetailItem {
            label: "Download progress"
            value: (vodProgress * 100).toFixed(0) + " %"
        }

        DetailItem {
            label: "Resolution"
            value: vodWidth + "x" + vodHeight
        }


        ButtonLayout {
            width: parent.width

            Button {
                text: "Copy file path to clipboard"
                onClicked: Clipboard.text = vodFilePath
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
                title: "VOD details"
            }
        }
    }
}

