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
import Vodman 2.1
import org.duckdns.jgressmann 1.0

Page {
    id: root


    SilicaFlickable {
        anchors.fill: parent
        contentWidth: parent.width
        contentHeight: column.height

        VerticalScrollDecorator {}

        Column {
            id: column
            width: parent.width

            PageHeader {
                //% "About %1"
                title: qsTrId("sf-about-page-header").arg(App.displayName)
            }

            Column {
                spacing: Theme.paddingLarge
                width: parent.width

                Column {
                    spacing: Theme.paddingSmall
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2*x


                    Image {
                        source: "/usr/share/icons/hicolor/128x128/apps/harbour-starfish.png"
                        anchors.horizontalCenter: parent.horizontalCenter
                        fillMode: Image.PreserveAspectFit
                        width: Theme.iconSizeLarge
                        height: Theme.iconSizeLarge


                        MouseArea {
                            id: debugEnabler
                            property int clicks: 0
                            anchors.fill: parent

                            onClicked: {
                                timer.running = true
                                clicks = clicks + 1
                            }

                            function timerDone() {
    //                            console.debug("triggered")
                                if (clicks >= 10) {
                                    debugApp.value = true
                                }

                                clicks = 0
                            }

                            Timer {
                                id: timer
                                interval: 3000; running: false; repeat: false
                                onTriggered: debugEnabler.timerDone()
                            }
                        }
                    }


                    Label {
                        //% "%1 %2"
                        text: qsTrId("sf-about-version-text").arg(App.displayName).arg(App.version)
                        anchors.horizontalCenter: parent.horizontalCenter
                        font.pixelSize: Theme.fontSizeLarge
                        color: Theme.highlightColor
    //                    font.bold: true
                    }

                    Label {
                        visible: YTDLDownloader.downloadStatus === YTDLDownloader.StatusReady
                        text: "youtube-dl " + YTDLDownloader.ytdlVersion
                        anchors.horizontalCenter: parent.horizontalCenter
                        font.pixelSize: Theme.fontSizeMedium
                        color: Theme.highlightColor
                    }

                    Button {
                        text: "Disable debugging"
                        anchors.horizontalCenter: parent.horizontalCenter
                        visible: debugApp.value
                        onClicked: {
                            debugApp.value = false
                        }
                    }
                }

                SectionHeader {
                    //% "Description"
                    text: qsTrId("sf-about-description-header")
                }

                LinkedLabel {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2*x
                    //% "%1 lets you download and stream StarCraft Brood War and StarCraft II VODs from the internet."
                    text: qsTrId("sf-about-description-text").arg(App.displayName)
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryHighlightColor
                    linkColor: Theme.secondaryColor
                }

                SectionHeader {
                    //% "Licensing"
                    text: qsTrId("sf-about-licensing-header")
                }

                LinkedLabel {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2*x

                    //% "Copyright (c) 2018, 2019 Jean Gressmann.<br/><br/>%1 is distributed under the <a href='https://opensource.org/licenses/MIT'>MIT license</a> and available from <a href='https://github.com/jgressmann/harbour-starfish'>GitHub</a>.<br/><br/>%1 includes code from <a href='https://github.com/jgressmann/harbour-vodman'>Vodman</a> and <a href='https://github.com/google/brotli'>Brotli</a>.<br/><br/>This application uses StarCraft (tm) related media from various sites on the internet. Should you feel you rights have been infringed please contact me and I'll remove the offending data.<br/><br/>%1 uses icons downloaded from <a href='https://www.flaticon.com'>flaticon</a> created by <a href='https://www.flaticon.com/authors/gregor-cresnar'>Gregor Cresnar</a>, <a href='https://www.flaticon.com/authors/freepik'>Freepik</a>, and <a href='https://www.flaticon.com/authors/smashicons'>Smashicons</a>."
                    text: qsTrId("sf-about-licensing-text").arg(App.displayName)
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryHighlightColor
                    linkColor: Theme.secondaryColor
                }

                SectionHeader {
                    //% "Translations"
                    text: qsTrId("sf-about-translations-header")
                }

                Column {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2*x
                    spacing: Theme.paddingSmall

                    DetailItem {
                        //% "English"
                        label: qsTrId("sf-about-translations-english-label")
                        //% "names of translators"
                        value: qsTrId("sf-about-translations-english-value")
                    }

                    DetailItem {
                        //% "German"
                        label: qsTrId("sf-about-translations-german-label")
                        //% "names of translators"
                        value: qsTrId("sf-about-translations-german-value")
                    }

                    Item {
                        width: parent.width
                        height: Theme.paddingLarge
                    }
                }
            }
        }
    }
}

