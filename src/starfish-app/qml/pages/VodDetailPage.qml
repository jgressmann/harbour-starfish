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


BasePage {
    id: page
    property string vodFilePath
    property int vodFileSize: -1
    property int vodWidth: -1
    property int vodHeight: -1
    property int vodRowId: -1
    property real vodProgress: 0


    VisualItemModel {
        id: model

        DetailItem {
            //% "ID"
            label: qsTrId("sf-vod-details-page-id")
            value: vodRowId
        }

        DetailItem {
            //% "Path"
            label: qsTrId("sf-vod-details-page-path")
            value: vodFilePath
        }


        DetailItem {
            //% "Size"
            label: qsTrId("sf-vod-details-page-size")
            value: makeSizeString(vodFileSize)

            function makeSizeString(size) {
                var oneGb = 1000000000
                var oneMb = 1000000

                if (size >= 10*oneGb) { // 10GB
                    //% "%1 GB"
                    return qsTrId("sf-vod-details-page-size-gb").arg((size/oneGb).toFixed(1))
                }

                if (size >= oneGb) { // 1GB
                    return qsTrId("sf-vod-details-page-size-gb").arg((size/oneGb).toFixed(2))
                }

                //% "%1 MB"
                return qsTrId("sf-vod-details-page-size-mb").arg((size/oneMb).toFixed(0))
            }
        }

        DetailItem {
            //% "Download progress"
            label: qsTrId("sf-vod-details-page-download-progress")
            value: (vodProgress * 100).toFixed(0) + " %"
        }

        DetailItem {
            //% "Resolution"
            label: qsTrId("sf-vod-details-page-resolution")
            value: vodWidth + "x" + vodHeight
        }


        ButtonLayout {
            width: parent.width

            Button {
                //% "Copy file path to clipboard"
                text: qsTrId("sf-vod-details-page-copy file path to clipboard")
                onClicked: Clipboard.text = vodFilePath
            }
        }
    }

    contentItem: SilicaFlickable {
        anchors.fill: parent
        contentWidth: parent.width

        VerticalScrollDecorator {}

        SilicaListView {
            id: listView
            anchors.fill: parent
            model: model
            header: PageHeader {
                //% "VOD details"
                title: qsTrId("sf-vod-details-page-title")
            }
        }
    }
}

