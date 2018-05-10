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

    Component.onCompleted: {
        console.debug("broadband format=" + settingBroadbandDefaultFormat.value)
        console.debug("mobile format=" + settingMobileDefaultFormat.value)
        console.debug("max cache size (GB)=" + settingCacheMaxSize.value)
    }

    Component.onDestruction: {
        settings.sync()
        VodDataManager.vodCacheLimit = settingCacheMaxSize.value * 1e9
    }


    VisualItemModel {
        id: model

        SectionHeader {
            text: "Network"
        }

        ComboBox {
            id: bearerModeComboBox
            width: parent.width
            label: "Network connection type"
            menu: ContextMenu {
                MenuItem { text: "Autodetect" }
                MenuItem { text: "Broadband" }
                MenuItem { text: "Mobile" }
            }

            Component.onCompleted: currentIndex = settingBearerMode.value

            onCurrentIndexChanged: {
                console.debug("bearer mode onCurrentIndexChanged " + currentIndex)
                settingBearerMode.value = currentIndex
            }
        }

        TextField {
            width: parent.width
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            label: "Max concurrent meta data downloads"
            text: settingNetworkMaxConcurrentMetaDataDownloads.value.toFixed(0)
            EnterKey.iconSource: "image://theme/icon-m-enter-next"
            EnterKey.onClicked: focus = false
            validator: IntValidator {
                bottom: 1
            }

            onTextChanged: {
                console.debug("text: " + text)
                if (acceptableInput) {
                    var number = parseFloat(text)
                    console.debug("number: " + number)
                    if (typeof(number) === "number") {
                        settingNetworkMaxConcurrentMetaDataDownloads.value = number
                    }
                }
            }
        }

        SectionHeader {
            text: "Format"
        }

        FormatComboBox {
            label: "Broadband"
            excludeAskEveryTime: false
            format: settingBroadbandDefaultFormat.value
            onFormatChanged: {
//                console.debug("save")
                settingBroadbandDefaultFormat.value = format
            }
        }

        FormatComboBox {
            label: "Mobile"
            excludeAskEveryTime: false
            format: settingMobileDefaultFormat.value
            onFormatChanged: {
//                console.debug("save")
                settingMobileDefaultFormat.value = format
            }
        }

        SectionHeader {
            text: "Playback"
        }

        TextSwitch {
            text: "Use external media player"
            checked: settingExternalMediaPlayer.value
            onCheckedChanged: {
                console.debug("external media player=" + checked)
                settingExternalMediaPlayer.value = checked
            }
        }

//        Column {
//            width: parent.width
////            spacing: Theme.paddingSmall

//            TextField {
//                id: cacheMaxSizeTextField
//                width: parent.width
//                inputMethodHints: Qt.ImhFormattedNumbersOnly
//                label: "Max cache size in GB"
////                placeholderText: "File name template for VODs"
////                horizontalAlignment: Theme.
//                text: settingCacheMaxSize.value.toFixed(1)
//                EnterKey.iconSource: "image://theme/icon-m-enter-next"
//                EnterKey.onClicked: focus = false
//                validator: DoubleValidator {
//                    notation: DoubleValidator.StandardNotation
//                    bottom: -1
//                    decimals: 1
//                }

//                onTextChanged: {
//                    console.debug("text: " + text)
//                    if (acceptableInput) {
//                        var number = parseFloat(text)
//                        console.debug("number: " + number)
//                        if (typeof(number) === "number") {
//                            settingCacheMaxSize.value = number
//                        }
//                    }
//                }
//            }

//            Label {
//                x: Theme.horizontalPageMargin
//                width: parent.width-2*x
//                text: "A value of -1 allows the cache to grow indefinitely."
//                wrapMode: Text.Wrap
//                font.pixelSize: Theme.fontSizeTiny
//                color: Theme.highlightColor
//            }
//        }
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
                title: "Settings"
            }
        }
    }
}

