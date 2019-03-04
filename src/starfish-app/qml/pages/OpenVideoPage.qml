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
import Sailfish.Pickers 1.0
import org.duckdns.jgressmann 1.0

import ".."


Page {
    id: root

    signal videoSelected(var key, string playbackUrl, int offset, bool saveScreenShot)

    PageHeader {
        id: header
        title: "Open video"
    }

    Column {
        anchors.top: header.bottom
        id: container
        width: parent.width


        ButtonLayout {
            id: buttons
            width: parent.width

            Button {
                //% "From clipboard"
                text: qsTrId("open-video-page-open-from-clipboard")
                enabled: Clipboard.hasText && App.isUrl(Clipboard.text)
                onClicked: {
                    videoSelected(
                                VodDataManager.recentlyWatched.urlKey(Clipboard.text),
                                Clipboard.text,
                                0,
                                true)
                }
            }
            Button {
                //% "From file"
                text: qsTrId("open-video-page-open-from-file")

                onClicked: {
                    pageStack.push(filePickerPage)
                }

                Component {
                    id: filePickerPage
                    FilePickerPage {
                        popOnSelection: false
                        onSelectedContentPropertiesChanged: {
                            videoSelected(VodDataManager.recentlyWatched.urlKey(selectedContentProperties.url),
                                          selectedContentProperties.url,
                                          0,
                                          true)
                        }
                    }
                }
            }
        }

        SectionHeader {
            visible: recentlyWatchedVideoView.count > 0
            //% "Recently watched"
            text: qsTrId("open-video-page-recently-watched-section-header")
        }
    }


    SilicaFlickable {
        width: parent.width
        anchors.top: container.bottom
        anchors.bottom: parent.bottom
        contentWidth: parent.width

        VerticalScrollDecorator {}

        RecentlyWatchedVideoView {
            id: recentlyWatchedVideoView
            anchors.fill: parent
            clip: true
            onClicked: function (a, b, c) {
                videoSelected(a, b, c, false)
            }
        }
    }
}

