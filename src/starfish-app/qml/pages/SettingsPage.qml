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
import Vodman 2.1
import org.duckdns.jgressmann 1.0
import ".."


BasePage {
    id: root
    showProgressPanel: false
    property string _currentdataDirectory
    property string _targetdataDirectory
    readonly property bool _toolEnabled: !VodDataManager.busy

    Component.onDestruction: {
        settings.sync()
    }

    Component.onCompleted: {
        _currentdataDirectory = VodDataManager.dataDirectory
        _targetdataDirectory = _currentdataDirectory
    }

    onStatusChanged: {
        switch (status) {
        case PageStatus.Activating:
            _currentdataDirectory = VodDataManager.dataDirectory
            break
        }
    }

    Component {
        id: confirmMovePage
        Dialog {

            DialogHeader {
                id: dialogHeader
                //% "Confirm data directory move"
                title: qsTrId("settings-page-confirm-data-dir-move-dialog-title")
                //% "Move"
                acceptText: qsTrId("settings-page-confirm-data-dir-move-dialog-accept-text")
            }

            Label {
                color: Theme.highlightColor
                anchors.top: dialogHeader.bottom
                anchors.bottom: parent.bottom
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                wrapMode: Text.Wrap
                //% "The application's data will be moved to %1.<br/><br/>This operation could take a good long while. During this time you will not be able to use the application.<br/><br/>Do you want to continue?"
                text: qsTrId("settings-page-confirm-data-dir-move-dialog-text").arg(_targetdataDirectory)
            }

            // page is created prior to onAccepted, see api reference doc
            acceptDestination: Qt.resolvedUrl("MoveDataDirectoryPage.qml")
            acceptDestinationAction: PageStackAction.Replace
            acceptDestinationProperties: {
                "targetDirectory": _targetdataDirectory
            }


            onAccepted: {
                VodDataManager.moveDataDirectory(_targetdataDirectory)
//                pageStack.replace(
//                            Qt.resolvedUrl("MoveDataDirectoryPage.qml"),
//                            {
//                                "targetDirectory": _targetdataDirectory
//                            },
//                            PageStackAction.Immediate)
            }

        }
    }

    Component {
        id: filePickerPage
        FilePickerPage {
            //nameFilters: [ '*.pdf', '*.doc' ]
            nameFilters: []
            onSelectedContentPropertiesChanged: {
                _targetdataDirectory = saveDirectoryTextField.text = _getDirectory(selectedContentProperties.filePath)
            }
        }
    }

    VisualItemModel {
        id: model

        SectionHeader {
            //% "Data"
            text: qsTrId("settings-page-data-dir-section-header")
        }


        ComboBox {
            id: saveDirectoryComboBox
            width: root.width
            //% "Directory"
            label: qsTrId("settings-page-data-dir-move-combobox-label")
            enabled: _toolEnabled
            menu: ContextMenu {
                MenuItem {
                    //% "Cache"
                    text: qsTrId("settings-page-data-dir-move-choice-cache")
                }
                MenuItem {
                    //% "Custom"
                    text: qsTrId("settings-page-data-dir-move-choice-custom")
                }
            }
        }

        TextField {
            id: saveDirectoryTextField
            width: root.width
            text: _currentdataDirectory
            //% "Directory for VOD and meta data"
            label: qsTrId("settings-page-data-dir-text-field-label")
            //% "Data directory"
            placeholderText: qsTrId("settings-page-data-dir-text-field-placeholder")
            EnterKey.iconSource: "image://theme/icon-m-enter-close"
            EnterKey.onClicked: focus = false
            enabled: _toolEnabled

            Component.onCompleted: {
                // combobox initially has index 0 which may be wrong
                _onSaveDirectoryTextFieldTextChanged()
            }
        }

        ButtonLayout {
            width: root.width

            Button {
                //% "Pick directory"
                text: qsTrId("settings-page-data-dir-pick-directory")
                enabled: _toolEnabled
                onClicked: pageStack.push(filePickerPage)
            }

            Button {
                enabled: _toolEnabled && !!_targetdataDirectory &&
                         _currentdataDirectory !== _targetdataDirectory
                //% "Apply change"
                text: qsTrId("settings-page-data-dir-apply-change")
                onClicked: pageStack.push(confirmMovePage)
            }
        }

        SectionHeader {
            //% "Network"
            text: qsTrId("settings-page-network-section-header")
        }

        ComboBox {
            id: bearerModeComboBox
            width: root.width
            //% "Network connection type"
            label: qsTrId("settings-page-network-connection-type-label")
            menu: ContextMenu {
                MenuItem {
                    //% "Autodetect"
                    text: qsTrId("settings-page-network-connection-choice-autodetect")
                }
                MenuItem {
                    //% "Broadband"
                    text: qsTrId("settings-page-network-connection-choice-broadband")
                }
                MenuItem {
                    //% "Mobile"
                    text: qsTrId("settings-page-network-connection-choice-mobile")
                }
            }

            Component.onCompleted: currentIndex = settingBearerMode.value

            onCurrentIndexChanged: {
                console.debug("bearer mode onCurrentIndexChanged " + currentIndex)
                settingBearerMode.value = currentIndex
            }
        }

        TextField {
            width: root.width
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            //% "Max concurrent meta data downloads"
            label: qsTrId("settings-page-network-max-meta-data-downloads")
            text: settingNetworkMaxConcurrentMetaDataDownloads.value.toFixed(0)
            EnterKey.iconSource: "image://theme/icon-m-enter-next"
            EnterKey.onClicked: focus = false
            enabled: _toolEnabled
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

        ComboBox {
            width: root.width
            //% "VOD site"
            label: qsTrId("settings-page-network-vod-site-label")
            //% "This site is used to check for new VODs."
            description: qsTrId("settings-page-network-vod-site-desc")
            enabled: _toolEnabled
            menu: ContextMenu {
                id: menu
                MenuItem { text: "sc2casts.com" }
                MenuItem { text: "sc2links.com" }
            }

            Component.onCompleted: {
                if (sc2CastsDotComScraper.id === settingNetworkScraper.value) {
                    currentIndex = 0
                } else {
                    currentIndex = 1
                }
            }

            onCurrentIndexChanged: {
                switch (currentIndex) {
                case 0:
                    settingNetworkScraper.value = sc2CastsDotComScraper.id
                    break
                case 1:
                    settingNetworkScraper.value = sc2LinksDotComScraper.id
                    break
                }
            }
        }

        TextSwitch {
            //% "Periodically check for new VODs"
            text: qsTrId("settings-page-network-periodically-check-for-vods-switch")
            checked: settingNetworkAutoUpdate.value
            enabled: _toolEnabled
            onCheckedChanged: {
                console.debug("auto update=" + checked)
                settingNetworkAutoUpdate.value = checked
            }
        }

        TextField {
            enabled: _toolEnabled && settingNetworkAutoUpdate.value
            width: root.width
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            //% "Minutes between checks"
            label: qsTrId("settings-page-network-check-interval-label")
            text: settingNetworkAutoUpdateIntervalM.value.toFixed(0)
            EnterKey.iconSource: "image://theme/icon-m-enter-next"
            EnterKey.onClicked: focus = false
            validator: IntValidator {
                bottom: debugApp.value ? 1 : 10
            }

            onTextChanged: {
                console.debug("text: " + text)
                if (acceptableInput) {
                    var number = parseInt(text)
                    console.debug("number: " + number)
                    if (typeof(number) === "number" && number >= validator.bottom) {
                        settingNetworkAutoUpdateIntervalM.value = number
                    }
                }
            }
        }

        TextSwitch {
            //% "Continue VOD file download when page closes"
            text: qsTrId("settings-page-network-continue-download-on-page-destruction")
            checked: settingNetworkContinueDownload.value
            enabled: _toolEnabled
            onCheckedChanged: {
                console.debug("continue download=" + checked)
                settingNetworkContinueDownload.value = checked
            }
        }


        SectionHeader {
            //% "Format"
            text: qsTrId("settings-page-format-section-header")
        }

        FormatComboBox {
            width: root.width
            //% "Broadband"
            label: qsTrId("settings-page-format-broadband-label")
            excludeAskEveryTime: false
            format: settingBroadbandDefaultFormat.value
            enabled: _toolEnabled
            onFormatChanged: {
//                console.debug("save")
                settingBroadbandDefaultFormat.value = format
            }
        }

        FormatComboBox {
            width: root.width
            //% "Mobile"
            label: qsTrId("settings-page-format-mobile-label")
            excludeAskEveryTime: false
            format: settingMobileDefaultFormat.value
            enabled: _toolEnabled
            onFormatChanged: {
//                console.debug("save")
                settingMobileDefaultFormat.value = format
            }
        }

        SectionHeader {
            //% "Playback"
            text: qsTrId("settings-page-playback-section-header")
        }

        TextSwitch {
            //% "Use external media player"
            text: qsTrId("settings-page-playback-external-media-player-switch")
            checked: settingExternalMediaPlayer.value
            enabled: _toolEnabled
            onCheckedChanged: {
                console.debug("external media player=" + checked)
                settingExternalMediaPlayer.value = checked
            }
        }

        TextField {
            width: root.width
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            //% "Number of recently watched videos to keep"
            label: qsTrId("settings-page-playback-no-videos-in-recently-watched")
            text: settingPlaybackRecentVideosToKeep.value.toFixed(0)
            EnterKey.iconSource: "image://theme/icon-m-enter-next"
            EnterKey.onClicked: focus = false
            enabled: _toolEnabled
            validator: IntValidator {
                bottom: 1
            }

            onTextChanged: {
                console.debug("text: " + text)
                if (acceptableInput) {
                    var number = parseFloat(text)
                    console.debug("number: " + number)
                    if (typeof(number) === "number") {
                        settingPlaybackRecentVideosToKeep.value = number
                    }
                }
            }
        }

        TextSwitch {
            //% "Pause playback on device lock"
            text: qsTrId("settings-page-playback-pause-on-device-lock-switch")
            checked: settingPlaybackPauseOnDeviceLock.value
            enabled: _toolEnabled
            onCheckedChanged: {
                console.debug("continue playback on device lock=" + checked)
                settingPlaybackPauseOnDeviceLock.value = checked
            }
        }

        TextSwitch {
            //% "Pause playback when the cover page is shown"
            text: qsTrId("settings-page-playback-pause-if-cover-page-switch")
            checked: settingPlaybackPauseInCoverMode.value
            enabled: _toolEnabled
            onCheckedChanged: {
                console.debug("continue playback in cover mode=" + checked)
                settingPlaybackPauseInCoverMode.value = checked
            }
        }

        SectionHeader {
            //% "New"
            text: qsTrId("settings-page-new-section-header")
        }

        TextField {
            width: root.width
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            //% "Number of days to keep a VOD in 'New'"
            label: qsTrId("settings-page-new-no-days-to-keep-vods-label")
            text: settingNewWindowDays.value.toFixed(0)
            EnterKey.iconSource: "image://theme/icon-m-enter-next"
            EnterKey.onClicked: focus = false
            enabled: _toolEnabled
            validator: IntValidator {
                bottom: 1
            }

            onTextChanged: {
                console.debug("text: " + text)
                if (acceptableInput) {
                    var number = parseFloat(text)
                    console.debug("number: " + number)
                    if (typeof(number) === "number") {
                        settingNewWindowDays.value = number
                    }
                }
            }
        }

        TextSwitch {
            //% "Remove watched VODs from 'New'"
            text: qsTrId("settings-page-new-remove-seen-vods-switch")
            checked: settingNewRemoveSeen.value
            enabled: _toolEnabled
            onCheckedChanged: {
                console.debug("remove seen vods from new=" + checked)
                settingNewRemoveSeen.value = checked
            }
        }

        Item {
            width: root.width
            height: Theme.paddingLarge
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
                //% "Settings"
                title: qsTrId("settings-page-title")
                VodDataManagerBusyIndicator {}
            }
        }
    }

    Connections {
        id: saveDirectoryComboBoxConnections
        target: saveDirectoryComboBox
        onCurrentIndexChanged: {
            saveDirectoryTextFieldConnections.target = null
            _updateSaveDirectoryTextField()
            saveDirectoryTextFieldConnections.target = saveDirectoryTextField
        }
    }

    Connections {
        id: saveDirectoryTextFieldConnections
        target: saveDirectoryTextField
        onTextChanged: _onSaveDirectoryTextFieldTextChanged()
    }

    function _getDirectory(str) {
        var lastSlashIndex = str.lastIndexOf("/")
        if (lastSlashIndex > 0) {
            str = str.substr(0, lastSlashIndex);
        }

        return str
    }

    function _updateSaveDirectoryComboBox() {
        _targetdataDirectory = saveDirectoryTextField.text
        if (saveDirectoryTextField.text === StandardPaths.cache) {
            saveDirectoryComboBox.currentIndex = 0
//        } else if (settingDefaultDirectory.value === StandardPaths.videos) {
//            saveDirectoryComboBox.currentIndex = 1
        } else {
            saveDirectoryComboBox.currentIndex = 1
        }
    }

    function _onSaveDirectoryTextFieldTextChanged() {
        saveDirectoryComboBoxConnections.target = null
        _updateSaveDirectoryComboBox()
        saveDirectoryComboBoxConnections.target = saveDirectoryComboBox
    }

    function _updateSaveDirectoryTextField() {
        switch (saveDirectoryComboBox.currentIndex) {
        case 0:
            _targetdataDirectory = saveDirectoryTextField.text = StandardPaths.cache
            break
        default:
            //pageStack.push(filePickerPage)
            break
        }
    }

}

