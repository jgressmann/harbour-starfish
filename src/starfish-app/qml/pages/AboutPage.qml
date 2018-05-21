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

Page {
    id: root


    SilicaFlickable {
        anchors.fill: parent
        contentWidth: parent.width

        VerticalScrollDecorator {}

        Column {
            spacing: Theme.paddingLarge
            x: Theme.paddingMedium
            width: parent.width - 2*x
            PageHeader { title: "About " + App.displayName }

            Column {
                spacing: Theme.paddingSmall
                width: parent.width

                Image {
                    source: "/usr/share/icons/hicolor/128x128/apps/harbour-starfish.png"
                    anchors.horizontalCenter: parent.horizontalCenter
                    fillMode: Image.PreserveAspectFit
                    width: Theme.iconSizeLarge
                    height: Theme.iconSizeLarge
//                    sourceSize.width: 512
//                    sourceSize.height: 512

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
                    text: App.displayName + " " + App.version
                    anchors.horizontalCenter: parent.horizontalCenter
                    font.pixelSize: Theme.fontSizeLarge
                    color: Theme.highlightColor
//                    font.bold: true
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
                text: "Description"
            }

            Label {
                x: Theme.horizontalPageMargin
                 width: parent.width-2*x
                 text:
App.displayName + " lets you download and stream StarCraft Brood War " +
"and StarCraft II VODs from the internet."
                 wrapMode: Text.WordWrap
                 font.pixelSize: Theme.fontSizeExtraSmall
                 color: Theme.highlightColor
                 textFormat: TextEdit.RichText
            }

            SectionHeader {
                text: "Licensing"
            }

            Label {
                x: Theme.horizontalPageMargin
                 width: parent.width-2*x
                 text: "Copyright (c) 2018 by Jean Gressmann.

" + App.displayName  + " is available under the <a href=\"https://opensource.org/licenses/MIT\">MIT</a> license."
                 wrapMode: Text.WordWrap
                 font.pixelSize: Theme.fontSizeExtraSmall
                 color: Theme.highlightColor
                 textFormat: TextEdit.RichText
            }

            Label {
                x: Theme.horizontalPageMargin
                 width: parent.width-2*x
                 text: "The application icon is copyright (c) by Blizzard Entertainment.
This application uses StarCraft (tm) related media from various sites on the internet.
Should you feel you rights have been infringed please contact me and I'll remove the offending data."
                 wrapMode: Text.WordWrap
                 font.pixelSize: Theme.fontSizeExtraSmall
                 color: Theme.highlightColor
                 textFormat: TextEdit.RichText
            }

            Label {
                x: Theme.horizontalPageMargin
                 width: parent.width-2*x
                 text: App.displayName + " uses icons downloaded from <a href=\"https://www.flaticon.com\">flaticon</a> created by <a href=\"https://www.flaticon.com/authors/gregor-cresnar\">Gregor Cresnar</a>,
and <a href=\"https://www.flaticon.com/authors/smashicons\">Smashicons</a>."
                 wrapMode: Text.WordWrap
                 font.pixelSize: Theme.fontSizeExtraSmall
                 color: Theme.highlightColor
                 textFormat: TextEdit.RichText
            }
        }
    }
}

