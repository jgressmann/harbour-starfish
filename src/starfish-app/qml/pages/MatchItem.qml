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
import ".."

ListItem {
    id: root
    contentHeight: requiredHeight
    readonly property int is_match_item: 1 // poor type check to call ownerGone()
    property bool showDate: true
    readonly property bool showSides: _c && ((_c.side1 && _c.side2) || (_c.side1 && !_c.urlShare.title))
    readonly property bool showTitle: _c && !_c.side2
    property int rowId: -1
    property var _playWhenPageTransitionIsDoneCallback: null
    property alias playlist: _playlist
    readonly property int baseOffset: _c ? _c.videoStartOffset : 0
    property bool _validRange: _c && baseOffset >= 0 && baseOffset < _c.videoEndOffset
    readonly property bool _hasValidRaces: _c && _c.race1 >= 1 && _c.race2 >= 1
    readonly property int requiredHeight: content.height + watchProgress.height
    readonly property string title: titleLabel.text
    property int _vodDownloadExplicitlyStarted: -1
    property MatchItemMemory memory
    readonly property bool _toolEnabled: !VodDataManager.busy
    readonly property bool seen: _c && _c.seen
    property QtObject _c: null
//    property MatchItemData _c: null
    readonly property bool tangibleDownloadProgress:
        _c &&
        (_c.urlShare.vodFetchStatus === UrlShare.Available ||
        (_c.urlShare.vodFetchStatus === UrlShare.Fetching && _c.urlShare.downloadProgress > 0))
    property int headerMode: Global.matchItemHeaderModeDefault
    property int bodyMode: Global.matchItemBodyModeDefault
    property string matchNameOverride
    readonly property string matchName: {
        var result = ""
        if (_c) {
            if (matchNameOverride) {
                result = matchNameOverride + " " + _c.matchNumber
            } else {
                result = _c.matchName
            }
        }
        return result
    }


    signal playRequest(var self)

    menu: contextMenu

    onMenuOpenChanged: {
        console.debug("menu open=" + menuOpen)
    }

    VodPlaylist {
        id: _playlist
        startOffset: _c ? _c.videoStartOffset : 0
        endOffset: _c ? _c.videoEndOffset : -1
        playbackOffset: _c ? _c.videoPlaybackOffset : -1
    }

    RemorsePopup { id: remorse }

    ProgressOverlay {
        id: progressOverlay
        show: _c && _c.urlShare.vodFetchStatus === UrlShare.Fetching
        progress: _c ? _c.urlShare.downloadProgress : 0

        anchors.fill: parent

        Column {
            id: content
            x: Theme.horizontalPageMargin
            width: parent.width - 2*x

            Label {
                width: parent.width
                visible: !!text
                truncationMode: TruncationMode.Fade
                font.pixelSize: Theme.fontSizeSmall
                text: {
                    var result = ""
                    if (_c) {
                        switch (headerMode) {
                        case Global.matchItemHeaderModeDefault:
                            result = _c.eventFullName
                            if (_c.stageName) {
                                if (result) {
                                    result += " / " + _c.stageName
                                } else {
                                    result = _c.stageName
                                }
                            }
                            break
                        case Global.matchItemHeaderModeMatchName:
                            result = matchName
                            break
                        case Global.matchItemHeaderModeNone:
                            break
                        }
                    }

                    return result
                }
            }

            Item {
                id: body
                width: parent.width
                height: thumbnailGroup.height

                Item {
                    id: thumbnailGroup
                    width: Global.itemHeight
                    height: Global.itemHeight
                    anchors.verticalCenter: parent.verticalCenter

                    Image {
                        id: thumbnail
                        property bool _hadImageLoadError: false
                        anchors.centerIn: parent
                        height: _hadImageLoadError
                                ? thumbnailBusyIndicator.height
                                : (_c.urlShare.thumbnailFetchStatus === UrlShare.Available ? parent.height : thumbnailBusyIndicator.height)
                        width: _hadImageLoadError
                                ? thumbnailBusyIndicator.width
                                : (_c.urlShare.thumbnailFetchStatus === UrlShare.Available ? parent.width : thumbnailBusyIndicator.width)
                        sourceSize.width: parent.width
                        sourceSize.height: parent.height
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                        cache: false
                        visible: !thumbnailBusyIndicator.visible
                        //_c.urlShare.thumbnailFetchStatus !== UrlShare.Fetching
                        source: {
                            if (_hadImageLoadError) {
                                return "image://theme/icon-m-reload"
                            }

                            switch (_c.urlShare.thumbnailFetchStatus) {
                            case UrlShare.Available:
                                return _c.urlShare.thumbnailFilePath
                            case UrlShare.Failed:
                                return "image://theme/icon-m-reload"
                            default:
                            case UrlShare.Unavailable:
                                return "image://theme/icon-m-sailfish"
                            }
                        }

                        onStatusChanged: {
                            if (Image.Error === status) {
                                _hadImageLoadError = true
                                console.debug("rowid=" + rowId + " error loading image " + _c.urlShare.thumbnailFilePath)
                            }
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        z: thumbnail.z + 1
                        //enabled: parent.visible && _c.urlShare.thumbnailFetchStatus !== UrlShare.Available && Image.Error !== status
                        //enabled: parent._hadImageLoadError || _c.urlShare.thumbnailFetchStatus === thumbnailStateDownloadFailed
                        enabled: thumbnail.visible && (thumbnail._hadImageLoadError || _c.urlShare.thumbnailFetchStatus !== UrlShare.Available)
                        onClicked: {
                            thumbnail._hadImageLoadError = false
                            _c.urlShare.fetchThumbnail()
                        }

//                        onEnabledChanged: {
//                            console.debug("rowid=" + rowId  + " reload enabled=" + enabled)
//                        }
                    }

                    BusyIndicator {
                        id: thumbnailBusyIndicator
                        size: BusyIndicatorSize.Medium
                        anchors.centerIn: parent
                        running: visible
                        visible: _c.urlShare.thumbnailFetchStatus === UrlShare.Fetching ||
                                 (_c.urlShare.thumbnailFetchStatus === UrlShare.Unavailable && _c.urlShare.metaDataFetchStatus === UrlShare.Fetching)
                    }
                } // thumbnail group

                Item {
                    id: imageSpacer
                    width: Theme.paddingMedium
                    height: parent.height

                    anchors.left: thumbnailGroup.right
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: imageSpacer.right
                    anchors.right: seenButton.left

                    Label {
                        id: titleLabel
                        width: parent.width
                        truncationMode: TruncationMode.Fade
                        visible: !sidesWithRaceLogosGroup.visible
                        text: {
                            if (_c) {
                                switch (bodyMode) {
                                case Global.matchItemBodyModeDefault:
                                    if (showSides) {
                                        if (_c.side2) {
                                            return _c.side1 + " - " + _c.side2
                                        }

                                        return _c.side1
                                    }

                                    if (showTitle && _c.urlShare.title) {
                                        return _c.urlShare.title
                                    }

                                    return matchName
                                case Global.matchItemBodyModeMatchName:
                                    return matchName
                                case Global.matchItemBodyModeNone:
                                    return ""
                                default:
                                    return "unhandled"
                                }
                            }

                            return ""
                        }

                        onTextChanged: {
                            console.debug("rowid=" + rowId + " title=" + text)
                        }
                    } // title/vs label

                    SidesBar {
                        id: sidesWithRaceLogosGroup
                        visible: bodyMode === Global.matchItemBodyModeDefault && _hasValidRaces
                        imageHeight: Theme.iconSizeSmall
                        imageWidth: Theme.iconSizeSmall
                        side1IconSource: VodDataManager.raceIcon(_c.race1)
                        side2IconSource: VodDataManager.raceIcon(_c.race2)
                        side1Label: _c.side1
                        side2Label: _c.side2
                        fontSize: Theme.fontSizeMedium
                        spacing: Theme.paddingSmall * 0.68
                    }

                    Item {
                        height: dateLabel2.height
                        width: parent.width


                        Label {
                            id: dateLabel2
            //                    visible: showDate || _loading
                            anchors.left: parent.left
                            anchors.right: loadCompleteImage2.left
            //                horizontalAlignment: Text.AlignLeft
                            font.pixelSize: Theme.fontSizeTiny
                            truncationMode: TruncationMode.Fade
                            anchors.verticalCenter: parent.verticalCenter
                            text: {
                                var result = ""
                                if (showDate) {
                                    result = Qt.formatDate(_c.matchDate)
                                }

                                if (_c.urlShare.vodResolution.width > 0) {
                                    if (result) {
                                        result += ", "
                                    }

                                    result += _c.urlShare.vodResolution.width + "x" + _c.urlShare.vodResolution.height
                                }

                                if (debugApp.value) {
                                    if (result) {
                                        result += ", "
                                    }

                                    result += rowId

                                    result += ", "
                                    result += _c.urlShare.urlShareId
                                }


                                return result
                            }


                        }

                        Row {
                            id: loadCompleteImage2
                            anchors.right: parent.right
                            spacing: Theme.paddingSmall / 2


                            Item {
                                visible: _c.urlShare.metaDataFetchStatus === UrlShare.Fetching
                                width: visible ? parent.height : 0
                                height: parent.height
                                anchors.verticalCenter: parent.verticalCenter

                                BusyIndicator {
                                    id: loadingIndicator
                                    size: BusyIndicatorSize.ExtraSmall
                                    anchors.centerIn: parent
                                    running: parent.visible
                                }
                            }

                            Icon {
                                width: Theme.iconSizeSmall
                                height: Theme.iconSizeSmall
                                sourceSize.width: width
                                sourceSize.height: height
                                source: {
                                    switch (_c.urlShare.metaDataFetchStatus) {
                                    case UrlShare.Gone:
                                        return "image://theme/icon-s-clear-opaque-cross"
                                    case UrlShare.Failed:
                                        return "/usr/share/harbour-starfish/icons/flash.png"
                                    default:
                                        return "image://theme/icon-s-date"
                                    }
                                }

                                anchors.verticalCenter: parent.verticalCenter
                                visible: _c.urlShare.metaDataFetchStatus === UrlShare.Available ||
                                         _c.urlShare.metaDataFetchStatus === UrlShare.Failed ||
                                         _c.urlShare.metaDataFetchStatus === UrlShare.Gone
                            }

                            Item {
                                width: loadCompleteImage2a.width
                                height: loadCompleteImage2a.height
                                anchors.verticalCenter: parent.verticalCenter


                                Image {
                                    id: loadCompleteImage2a
                                    source: "image://theme/icon-s-cloud-download"
                                    visible: false
                                }

                                ProgressMaskEffect {
                                    src: loadCompleteImage2a
                                    width: loadCompleteImage2a.width
                                    height: loadCompleteImage2a.height
                                    visible: _c.urlShare.vodFetchStatus === UrlShare.Available && _c.urlShare.downloadProgress < 1
                                    progress: {
                                        var s = Math.max(0, Math.min(_c.urlShare.downloadProgress, 1))
                                        if (s > 0) {
                                            if (s >= 1) {
                                                return 1
                                            }

                                            return 0.1 + s * 0.8 // ignore transparent sides
                                        }

                                        return 0
                                    }
                                }

                                Image {
                                    source: "image://theme/icon-s-like"
    //                                anchors.verticalCenter: parent.verticalCenter
    //                                anchors.right: parent.right
                                    visible: _c.urlShare.vodFetchStatus === UrlShare.Available && _c.urlShare.downloadProgress >= 1
                                }

                                Icon {
                                    width: Theme.iconSizeSmall
                                    height: Theme.iconSizeSmall
                                    sourceSize.width: width
                                    sourceSize.height: height
                                    source: "/usr/share/harbour-starfish/icons/flash.png"
                                    anchors.verticalCenter: parent.verticalCenter
                                    visible: _c.urlShare.vodFetchStatus === UrlShare.Failed
                                }

                                BusyIndicator {
                                    id: downloadingVodIndicator
                                    size: BusyIndicatorSize.ExtraSmall
                                    anchors.centerIn: parent
                                    running: _c.urlShare.vodFetchStatus === UrlShare.Fetching
                                }
                            }
                        }
                    } // date / format / status group
                }

                IconButton {
                    id: seenButton
                    anchors.right: parent.right
                    enabled: !_c.deleted

                    //icon.source: seen ? "image://theme/icon-m-favorite-selected" : "image://theme/icon-m-favorite"
                    icon.source: _c.seen ? "image://theme/icon-m-favorite-selected" : "image://theme/icon-m-favorite"
                    anchors.verticalCenter: parent.verticalCenter
//                    property bool seen: false

                    onClicked: {
                        _c.seen = !_c.seen
                        console.debug("seen=" + _c.seen)
                    }
                }
            }
        }

        Rectangle {
            id: watchProgress
            color: Theme.highlightColor
            opacity: Theme.highlightBackgroundOpacity
            width: parent.width * _range
            visible: !seen && _validRange
            readonly property real _range:
                _c.videoPlaybackOffset < baseOffset
                ? 0
                : (_c.videoPlaybackOffset > _c.videoEndOffset ? 1 : (_c.videoPlaybackOffset-baseOffset)/(_c.videoEndOffset-baseOffset))
            height: 4
            anchors.top: content.bottom
//            onVisibleChanged: {
//                console.debug("rowid " + rowId + " watch progress visible " + visible)
//            }

//            onWidthChanged: {
//                console.debug("rowid=" + rowId + " range=" + _range)
//            }
        }
    }

    onClicked: _tryPlay()

    Connections {
        target: pageStack
        onBusyChanged: {
            if (!pageStack.busy && _playWhenPageTransitionIsDoneCallback) {
                var c = _playWhenPageTransitionIsDoneCallback;
                _playWhenPageTransitionIsDoneCallback = null
                c()
            }
        }
    }

    Component.onCompleted: {
        _c = VodDataManager.acquireMatchItem(rowId)

        playlist.mediaKey = VodDataManager.recentlyWatched.vodKey(rowId)

        memory.addMatchItem(root)

        if (requiredHeight > height) {
            console.warn("rowid " + rowId + " match item height is " + height + " which is less than required height " + requiredHeight)
        }
    }

    Component.onDestruction: {

//        console.debug("destroy match item rowid=" + rowId)
        if (memory) { // QML binding may have been removed due to the parent view being destroyed
            memory.removeMatchItem(root)
        }

        VodDataManager.releaseMatchItem(_c)
    }

    Component {
        id: contextMenu
        ContextMenu {
            MenuItem {
                //% "Copy URL to clipboard"
                text: qsTrId("sf-match-item-copy-url-to-clipboard")
                onClicked: Clipboard.text = _c.urlShare.url
            }

            MenuItem {
                //% "Download meta data"
                text: qsTrId("sf-match-item-download-meta-data")
                visible: _c.urlShare.metaDataFetchStatus !== UrlShare.Fetching &&
                         _c.urlShare.metaDataFetchStatus !== UrlShare.Available &&
                         App.isOnline &&
                         !_c.deleted
                onClicked: _c.urlShare.fetchMetaData()
            }

            MenuItem {
                //% "Download VOD"
                text: qsTrId("sf-match-item-download-vod")
                visible: _c.urlShare.metaDataFetchStatus === UrlShare.Available &&
                         _c.urlShare.vodFetchStatus !== UrlShare.Fetching &&
                         _c.urlShare.downloadProgress < 1 &&
                         App.isOnline &&
                         !_c.deleted
                onClicked: {
                    var format = _getVideoFormatFromBearerMode()
                    _download(false, format)
                }
            }

            MenuItem {
                //% "Download VOD with format..."
                text: qsTrId("sf-match-item-download-vod-with-format")
                visible: _c.urlShare.metaDataFetchStatus === UrlShare.Available &&
                         _c.urlShare.vodFetchStatus !== UrlShare.Fetching &&
                         _c.urlShare.downloadProgress < 1 &&
                         App.isOnline &&
                         !_c.deleted
                onClicked: _download(false, VM.VM_Any)
            }

            MenuItem {
                //% "Cancel VOD download"
                text: qsTrId("sf-match-item-cancel-vod-download")
                visible: _c.urlShare.vodFetchStatus === UrlShare.Fetching && _c.urlShare.downloadProgress < 1 && !_c.deleted
                onClicked: _cancelDownload()
            }

            MenuItem {
                //% "Play stream"
                text: qsTrId("sf-match-item-play-stream")
                visible: _c.urlShare.metaDataFetchStatus === UrlShare.Available && App.isOnline && !_c.deleted
                onClicked: _playStream()
            }

            MenuItem {
                //% "Play stream with format..."
                text: qsTrId("sf-match-item-play-stream-with-format")
                visible: _c.urlShare.metaDataFetchStatus === UrlShare.Available && App.isOnline && !_c.deleted
                onClicked: _playStreamWithFormat(VM.VM_Any)
            }

            MenuItem {
                //% "Reset watch progress"
                text: qsTrId("sf-match-item-reset-watch-progress")
                visible: !_c.deleted
                onClicked: VodDataManager.setPlaybackOffset(_playlist.mediaKey, _playlist.startOffset)
            }

            MenuItem {
                //% "Delete meta data"
                text: qsTrId("sf-match-item-delete-meta-data")
                visible: _c.urlShare.metaDataFetchStatus === UrlShare.Available && !_c.deleted
                onClicked: _c.urlShare.deleteMetaData()
            }

            MenuItem {
                //% "Delete VOD file"
                text: qsTrId("sf-match-item-delete-vod-file")
                enabled: _toolEnabled
                visible: _c.urlShare.vodFetchStatus === UrlShare.Available && !_c.deleted

                onClicked: {
                    // still not sure why local vars are needed but they are!!!!
                    var item = root
                    var r = remorse
                    if (_c.urlShare.shareCount > 1) {
                        var dialog = pageStack.push(
                               Qt.resolvedUrl("ConfirmDeleteVodFile.qml"),
                               {
                                   name: titleLabel.text,
                                   count: _c.urlShare.shareCount - 1
                               })
                        dialog.accepted.connect(function () {
                            //% "Deleting files for %1 VODs"
                            r.execute(qsTrId("sf-match-item-deleting-shared-vod-files").arg(item._c.urlShare.shareCount), function () {
                                item._cancelDownload()
                                item._c.urlShare.deleteVodFiles()
                            })
                        })
                    } else {
                        //% "Deleting files for %1"
                        item.remorseAction(qsTrId("sf-match-item-deleting-vod-files-for-title").arg(title), function() {
                            item._cancelDownload()
                            item._c.urlShare.deleteVodFiles()
                        })
                    }
                }
            }

            MenuItem {
                //% "VOD details"
                text: qsTrId("sf-match-item-vod-details")
//                visible: tangibleDownloadProgress
                onClicked: pageStack.push(
                               Qt.resolvedUrl("VodDetailPage.qml"),
                               {
                                   match: _c
                               })
            }

//            MenuItem {
//                //% "Copy VOD file path to clipboard"
//                text: qsTrId("sf-match-item-copy-vod-file-path-to-clipboard")
//                visible: tangibleDownloadProgress && _c.urlShare.files === 1
//                onClicked: Clipboard.text = _c.urlShare.file(0).vodFilePath
//            }

            MenuItem {
                visible: !_c.deleted
                //% "Delete thumbnail"
                text: qsTrId("sf-match-item-delete-thumbnail")
                onClicked: _c.urlShare.deleteThumbnail()
            }

            MenuItem {
                //% "Delete VOD from database"
                text: qsTrId("sf-match-item-delete-vod-from-database")
                visible: !_c.deleted
                enabled: _toolEnabled
                onClicked: {
                    // still not sure why local vars are needed but they are!!!!
                    var item = root
                    var g = Global
                    //% "Deleting %1"
                    item.remorseAction(qsTrId("sf-match-item-deleting-vod-from-database-remorse").arg(_c.urlShare.title), function() {
                        item._cancelDownload()
                        g.deleteVods("where id=" + item.rowId)
                    })
                }
            }

            MenuItem {
                //% "Undelete VOD from database"
                text: qsTrId("sf-match-item-vod-undelete-from-database")
                visible: _c.deleted
                enabled: _toolEnabled
                onClicked: {
                    // still not sure why local vars are needed but they are!!!!
                    var item = root
                    var g = Global
                    //% "Undeleting %1"
                    item.remorseAction(qsTrId("sf-match-item-undeleting-vod-from-database-remorse").arg(_c.urlShare.title), function() {
                        g.undeleteVods("where id=" + item.rowId)
                    })
                }
            }
        }
    }

    function _getVideoFormatFromBearerMode() {
        var formatId
        switch (settingBearerMode.value) {
        case Global.bearerModeBroadband:
            console.debug("force broadband format selection")
            formatId = settingBroadbandDefaultFormat.value
            break
        case Global.bearerModeMobile:
            console.debug("force mobile format selection")
            formatId = settingMobileDefaultFormat.value
            break
        default:
            if (App.isOnBroadband) {
                console.debug("use broadband format selection")
                formatId = settingBroadbandDefaultFormat.value
            } else if (App.isOnMobile) {
                console.debug("use mobile format selection")
                formatId = settingMobileDefaultFormat.value
            } else {
                console.debug("unknown bearer using mobile default format")
                formatId = settingMobileDefaultFormat.value
            }
            break
        }

        console.debug("format=" + formatId)

        return formatId
    }

    function _findBestFormat(vod, formatId) {
        var formatIndex = -1
        var anyProperFormat = false
        for (var i = 0; i < vod.avFormats; ++i) {
            var f = vod.avFormat(i)
            if (f.height > 0 && f.width > 0) {
                anyProperFormat = true
                break
            }
        }

        if (VM.VM_Smallest === formatId) {
            if (anyProperFormat) {
                var best = vod.avFormat(0)
                formatIndex = 0
                for (var i = 1; i < vod.avFormats; ++i) {
                    var f = vod.avFormat(i)
                    if (f.height > 0 && f.height < best.height) {
                        best = f;
                        formatIndex = i;
                    }
                }
            } else {
                formatIndex = 0 // typically the smallest
            }
        } else if (VM.VM_Largest === formatId) {
            if (anyProperFormat) {
                var best = vod.avFormat(0)
                formatIndex = 0
                for (var i = 1; i < vod.avFormats; ++i) {
                    var f = vod.avFormat(i)
                    if (f.height > 0 && f.height > best.height) {
                        best = f;
                        formatIndex = i;
                    }
                }
            } else {
                formatIndex = vod.avFormats - 1 // typically the largest
            }
        } else {
            // try to find exact match
            if (anyProperFormat) {
                for (var i = 0; i < vod.avFormats; ++i) {
                    var f = vod.avFormat(i)
                    if (f.format === formatId) {
                        formatIndex = i
                        break
                    }
                }

                if (formatIndex === -1) {
                    var target = _targetHeight(formatId)
                    var bestdelta = Math.abs(vod.avFormat(0).height - target)
                    formatIndex = 0
                    for (var i = 1; i < vod.avFormats; ++i) {
                        var f = vod.avFormat(i)
                        var delta = Math.abs(f.width - target)
                        if (delta < bestdelta) {
                            bestdelta = delta;
                            formatIndex = i;
                        }
                    }
                }
            } else {
                // try to match the selected format onto the range of formats
                var qualityTarget = Math.max(0, Math.min((formatId - VM.VM_240p) / (VM.VM_1080p - VM.VM_240p), 1))
                var availableQualityStep = 1 / vod.avFormats
                var div = Math.floor(qualityTarget / availableQualityStep)
                var rem = qualityTarget - div * availableQualityStep
                formatIndex = Math.min(vod.avFormats - 1, div + (rem >= 0.5 * availableQualityStep ? 1 : 0))
                console.debug("format id=" + formatId + " quality target=" + qualityTarget + " step=" + availableQualityStep + " format index=" + formatIndex)
            }
        }

        return formatIndex
    }

    function _tryPlay() {
        if (_c.deleted) {
            return
        }

        if (_c.urlShare.vodFetchStatus === UrlShare.Fetching) {
            _playFiles()
        } else if (_c.urlShare.vodFetchStatus === UrlShare.Available) {
            // try download rest if incomplete and format matches
            if (_c.urlShare.downloadProgress < 1) {
                if (_c.urlShare.metaDataFetchStatus === UrlShare.Available) {
                    var index = _c.urlShare.vodFormatIndex
                    if (index >= 0) {
                        _downloadFormat(index, true)
                    }
                } else {
                    _c.urlShare.fetchMetaData()
                }
            }

            // at any rate, play the file
            _playFiles()
        } else if (_c.urlShare.metaDataFetchStatus === UrlShare.Available) {
            _playStream()
        } else if (_c.urlShare.metaDataFetchStatus !== UrlShare.Fetching) {
            _c.urlShare.fetchMetaData()
        }
    }

    function _playFiles() {
        playlist.parts = _c.urlShare.files
        for (var i = 0; i < _c.urlShare.files; ++i) {
            var file = _c.urlShare.file(i)
            if (_c.urlShare.metaDataFetchStatus === UrlShare.Available) {
                playlist.setDuration(i, _c.urlShare.metaData.vod(i).duration)
            } else {
                var duration = file.duration;
                if (duration > 0) {
                    playlist.setDuration(i, duration)
                } else {
                    playlist.setDuration(i, -1)
                }
            }

            playlist.setUrl(i, file.vodFilePath)
            console.debug("index=" + i + " duration=" + playlist.duration(i) + " file=" + playlist.url(i))
        }

        console.debug("playlist parts=" + playlist.parts)
        playRequest(root)
    }

    function _playStream() {
        var vmFormatId = _getVideoFormatFromBearerMode()
        _playStreamWithFormat(vmFormatId)
    }

    function _playStreamWithFormat(vmFormatId) {
        var metaData = _c.urlShare.metaData
        playlist.parts = metaData.vods

        if (VM.VM_Any === vmFormatId) {
            _selectAvFormat(metaData.vod(0), function(formatIndex) {

                for (var i = 0; i < metaData.vods; ++i) {
                    var vod = metaData.vod(i)
                    var format = vod.avFormat(formatIndex)

                    playlist.setDuration(i, vod.duration)
                    playlist.setUrl(i, format.streamUrl)
                }

                console.debug("playlist=" + playlist)
                _playWhenPageTransitionIsDoneCallback = function () {
                    playRequest(root)
                }
            })
        } else {
            for (var i = 0; i < metaData.vods; ++i) {
                var vod = metaData.vod(i)
                console.debug("index=" + i + " vod formats=" + vod.avFormats)
                var formatIndex = _findBestFormat(vod, vmFormatId)
                var format = vod.avFormat(formatIndex)

                playlist.setDuration(i, vod.duration)
                playlist.setUrl(i, format.streamUrl)
            }

            console.debug("playlist parts=" + playlist.parts + " playback_offset=" + playlist.playbackOffset)
            playRequest(root)
        }
    }

    function _downloadFormat(formatIndex, autoStarted) {
        _c.urlShare.fetchVodFiles(formatIndex)
        switch (_vodDownloadExplicitlyStarted) {
        case -1:
        case 0:
            _vodDownloadExplicitlyStarted = autoStarted ? 0 : 1
            break
        }
    }

    function _download(autoStarted, vmFormatId) {
        if (_c.urlShare.vodFormatIndex >= 0) {
            _downloadFormat(_c.urlShare.vodFormatIndex, autoStarted)
        } else {
            var vod = _c.urlShare.metaData.vod(0)
            if (VM.VM_Any === vmFormatId) {
                _selectAvFormat(vod, function(index) {
                    _downloadFormat(index, autoStarted)
                })
            } else {
                var formatIndex = _findBestFormat(vod, vmFormatId)
                _downloadFormat(formatIndex, autoStarted)
            }
        }
    }

    function _cancelDownload() {
        _c.urlShare.cancelFetchVodFiles()
        _vodDownloadExplicitlyStarted = -1
    }

    function cancelImplicitVodFileFetch() {
        // only cancel download if started on this item
        if (0 === _vodDownloadExplicitlyStarted) {
            console.debug("canceling download for rowid=" + rowId)
            _cancelDownload()
            return true
        }
        return false
    }

    function ownerGone() {
        if (!cancelImplicitVodFileFetch() && !settingNetworkContinueDownload.value) {
            console.debug("canceling download for rowid=" + rowId)
            _cancelDownload()
        }
    }

    function _targetHeight(vmFormatId) {
        switch (vmFormatId) {
        case VM.VM_240p:
            return 240
        case VM.VM_360p:
            return 360
        case VM.VM_720p:
            return 720
        case VM.VM_1080p:
            return 1080
        case VM.VM_1440p:
            return 1440
        case VM.VM_2160p:
            return 2160
        default:
            return 2160
        }
    }

    function _selectAvFormat(vod, more) {
        if (vod.avFormats > 1) {
            var labels = []
            var values = []
            for (var i = 0; i < vod.avFormats; ++i) {
                var f = vod.avFormat(i)
                labels.push(f.displayName + " / " + f.tbr.toFixed(0) + " [tbr] " + f.extension)
                values.push(f.id)
            }

            var dialog = pageStack.push(
                Qt.resolvedUrl("SelectFormatDialog.qml"), {
                            //% "Select a format"
                            "title": qsTrId("sf-select-av-format-dialog-title"),
                            "labels" : labels,
                            "values": values
                        })
            dialog.accepted.connect(function() {
                more(dialog.formatIndex)
            })
        } else {
            more(0)
        }
    }
}

