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
    property QtObject match

    contentItem: SilicaFlickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: column.height

        VerticalScrollDecorator {}

        Column {
            id: column
            width: parent.width

            PageHeader {

                //% "VOD details"
                title: qsTrId("sf-vod-details-page-title")
            }

            DetailItem {
                //% "Vod id"
                label: qsTrId("sf-vod-details-page-vod-id")
                value: match.rowId
            }

            DetailItem {
                //% "Url share id"
                label: qsTrId("sf-vod-details-page-url-share-id")
                value: match.urlShare.urlShareId
            }

            DetailItem {
                //% "Download progress"
                label: qsTrId("sf-vod-details-page-download-progress")
                value: (match.urlShare.downloadProgress * 100).toFixed(0) + " %"
            }

            DetailItem {
                visible: match.urlShare.vodResolution.width > 0 && match.urlShare.vodResolution.height > 0
                //% "Resolution"
                label: qsTrId("sf-vod-details-page-resolution")
                value: match.urlShare.vodResolution.width + "x" + match.urlShare.vodResolution.height
            }

            DetailItem {
                //% "Url"
                label: qsTrId("sf-vod-details-page-url")
                value: match.urlShare.url
            }

            ButtonLayout {
                width: parent.width

                Button {
                    //% "Copy url to clipboard"
                    text: qsTrId("sf-vod-details-page-copy-url-clipboard")
                    onClicked: Clipboard.text = match.urlShare.url
                }
            }

            SectionHeader {
                //% "Files"
                text: qsTrId("sf-vod-details-page-files-section-header")
                visible: match.urlShare.files > 0
            }

            Repeater {
                id: filesView
                width: parent.width
                model: match.urlShare.files
                delegate: Component {


                    Column {
                        property QtObject file
                        width: filesView.width

                        Label {
                            x: Theme.horizontalPageMargin
                            width: parent.width - 2*x
                            //% "File #%1"
                            text: qsTrId("sf-vod-details-page-files-section-file-number").arg(index+1)
                            color: Theme.highlightColor
                            font.pixelSize: Theme.fontSizeSmall
                        }

                        DetailItem {
                            //% "Path"
                            label: qsTrId("sf-vod-details-page-path")
                            value: file.vodFilePath
                        }

                        DetailItem {
                            //% "Download progress"
                            label: qsTrId("sf-vod-details-page-download-progress")
                            value: (file.downloadProgress * 100).toFixed(0) + " %"
                        }


                        DetailItem {
                            //% "Size"
                            label: qsTrId("sf-vod-details-page-size")
                            value: makeSizeString(file.vodFileSize)

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


                        ButtonLayout {
                            width: parent.width

                            Button {
                                //% "Copy file path to clipboard"
                                text: qsTrId("sf-vod-details-page-copy-file-path-to-clipboard")
                                onClicked: Clipboard.text = file.vodFilePath
                            }
                        }

                        Item {
                            width: parent.width
                            height: Theme.paddingLarge
                        }

                        Component.onCompleted: {
                            file = match.urlShare.file(index)
//                            console.debug("vod file index " + index)
                        }
                    }
                }
            }
        }
    }
}

