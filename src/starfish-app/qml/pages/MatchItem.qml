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

ListItem {
    id: root
    contentHeight: requiredHeight
    readonly property int is_match_item: 1 // poor type check to call ownerGone()
    property bool showDate: true
    property bool showSides: _c && ((_c.side1 && _c.side2) || (_c.side1 && !_c.urlShare.title))
    property bool showTitle: _c && !_c.side2
    property bool menuEnabled: true
    property int rowId: -1
    property bool _playWhenTransitionDone: false
    readonly property string vodUrl: _vodUrl
    property string _vodUrl
    property int startOffset: 0
    readonly property int baseOffset: _c ? _c.videoStartOffset : 0
    property bool _validRange: _c && baseOffset >= 0 && baseOffset < _c.videoEndOffset
    readonly property bool _hasValidRaces: _c && _c.race1 >= 1 && _c.race2 >= 1
    readonly property int requiredHeight: content.height + watchProgress.height
    readonly property string title: titleLabel.text
    readonly property string thumbnailSource: thumbnail.source
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

    menu: menuEnabled ? contextMenu : null
    signal playRequest(var self)

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
                visible: !!_c.eventFullName || !!_c.stageName
                truncationMode: TruncationMode.Fade
                font.pixelSize: Theme.fontSizeSmall
                text: {
                    var result = _c.eventFullName
                    if (_c.stageName) {
                        if (result) {
                            result += " / " + _c.stageName
                        } else {
                            result = _c.stageName
                        }
                    }

                    return result
                }
            }

            Rectangle {
                id: body
                color: "transparent"
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
                            if (showSides) {
                                if (_c.side2) {
                                    return _c.side1 + " - " + _c.side2
                                }

                                return _c.side1
                            }

                            if (showTitle && _c.urlShare.title) {
                                return _c.urlShare.title
                            }

                            return _c.matchName
                        }
                    } // title/vs label

                    SidesBar {
                        id: sidesWithRaceLogosGroup
                        visible: showSides && _hasValidRaces
                        imageHeight: Theme.iconSizeSmall
                        imageWidth: Theme.iconSizeSmall
                        side1IconSource: VodDataManager.raceIcon(_c.race1)
                        side2IconSource: VodDataManager.raceIcon(_c.race2)
                        side1Label: _c.side1
                        side2Label: _c.side2
                        fontSize: Theme.fontSizeMedium
                        spacing: Theme.paddingSmall * 0.68
                    }

//                    Item {
//                        id: sidesWithRaceLogosGroup
//                        width: parent.width
//                        height: Math.max(race1Icon.height, side1Name.height)
//                        visible: showSides && _hasValidRaces
//                        property real labelWidth: (width - 2 * race1Icon.width) * 0.5

//                        Image {
//                            id: race1Icon
//                            anchors.left: parent.left
//                            anchors.verticalCenter: parent.verticalCenter
//                            width: Theme.iconSizeSmall
//                            height: Theme.iconSizeSmall
//                            sourceSize.width: width
//                            sourceSize.height: height
//                            source: VodDataManager.raceIcon(_c.race1)
//                        }

//                        Label {
//                            id: side1Name
//                            anchors.left: race1Icon.right
//                            anchors.verticalCenter: parent.verticalCenter
//                            width: parent.labelWidth
//                            truncationMode: TruncationMode.Fade
//                            text: " " + _c.side1
//                            horizontalAlignment: Text.AlignLeft
//                        }

//                        Label {
//                            id: side2Name
//                            anchors.right: race2Icon.left
//                            anchors.verticalCenter: parent.verticalCenter
//                            width: parent.labelWidth
//                            truncationMode: TruncationMode.Fade
//                            text: _c.side2 + " "
//                            horizontalAlignment: Text.AlignRight
//                        }

//                        Image {
//                            id: race2Icon
//                            anchors.right: parent.right
//                            anchors.verticalCenter: parent.verticalCenter
//                            width: Theme.iconSizeSmall
//                            height: Theme.iconSizeSmall
//                            sourceSize.width: width
//                            sourceSize.height: height
//                            source: VodDataManager.raceIcon(_c.race2)
//                        }
//                    } // vs label with race icons

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

//                                if (startOffset > 0) {
//                                    if (result) {
//                                        result += ", "
//                                    }

//                                    result += Global.secondsToTimeString(startOffset)
//                                }

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

                            Image {
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

                                Image {
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
                startOffset < baseOffset
                ? 0
                : (startOffset > _c.videoEndOffset ? 1 : (startOffset-baseOffset)/(_c.videoEndOffset-baseOffset))
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
            if (!pageStack.busy && _playWhenTransitionDone) {
                _playWhenTransitionDone = false
                _play()
            }
        }
    }

    Component.onCompleted: {
        _c = VodDataManager.acquireMatchItem(rowId)

        memory.addMatchItem(root)

        updateStartOffset()

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
                text: "Copy URL to clipboard"
                onClicked: Clipboard.text = _c.urlShare.url
            }

            MenuItem {
                text: "Download meta data"
                visible: _c.urlShare.metaDataFetchStatus !== UrlShare.Fetching &&
                         _c.urlShare.metaDataFetchStatus !== UrlShare.Available &&
                         App.isOnline
                onClicked: _c.urlShare.fetchMetaData()
            }

            MenuItem {
                text: "Download VOD"
                visible: _c.urlShare.metaDataFetchStatus === UrlShare.Available &&
                         _c.urlShare.vodFetchStatus !== UrlShare.Fetching &&
                         _c.urlShare.downloadProgress < 1 &&
                         App.isOnline
                onClicked: {
                    var format = _getVideoFormatFromBearerMode()
                    _download(false, format)
                }
            }

            MenuItem {
                text: "Download VOD with format..."
                visible: _c.urlShare.metaDataFetchStatus === UrlShare.Available &&
                         _c.urlShare.vodFetchStatus !== UrlShare.Fetching &&
                         _c.urlShare.downloadProgress < 1 &&
                         App.isOnline
                onClicked: _download(false, VM.VM_Any)
            }

            MenuItem {
                text: "Cancel VOD download"
                visible: _c.urlShare.vodFetchStatus === UrlShare.Fetching && _c.urlShare.downloadProgress < 1
                onClicked: _cancelDownload()
            }

            MenuItem {
                text: "Play stream"
                visible: _c.urlShare.metaDataFetchStatus === UrlShare.Available && App.isOnline
                onClicked: _playStream()
            }

            MenuItem {
                text: "Play stream with format..."
                visible: _c.urlShare.metaDataFetchStatus === UrlShare.Available && App.isOnline
                onClicked: _playStreamWithFormat(VM.VM_Any)
            }

            MenuItem {
                text: "Delete meta data"
                visible: _c.urlShare.metaDataFetchStatus === UrlShare.Available
                onClicked: _c.urlShare.deleteMetaData()
            }

            MenuItem {
                text: "Delete VOD file"
                enabled: _toolEnabled
                visible: _c.urlShare.vodFetchStatus === UrlShare.Available

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
                            r.execute("Deleting %1 VOD files".arg(item._c.urlShare.shareCount), function () {
                                item._cancelDownload()
                                item._c.urlShare.deleteVodFile()
                            })
                        })
                    } else {

                        item.remorseAction("Deleting VOD file for " + title, function() {
                            item._cancelDownload()
                            item._c.urlShare.deleteVodFile()
                        })
                    }
                }
            }

            MenuItem {
                text: "VOD details"
                visible: tangibleDownloadProgress
                onClicked: pageStack.push(
                               Qt.resolvedUrl("VodDetailPage.qml"),
                               {
                                   vodTitle: titleLabel.text,
                                   vodFilePath: _c.urlShare.vodFilePath,
                                   vodFileSize: _c.urlShare.vodFileSize,
                                   vodWidth: _c.urlShare.vodResolution.width,
                                   vodHeight: _c.urlShare.vodResolution.height,
                                   vodRowId: rowId,
                                   vodProgress: _c.urlShare.downloadProgress
                               })
            }

            MenuItem {
                text: "Copy VOD file path to clipboard"
                visible: tangibleDownloadProgress
                onClicked: Clipboard.text = _c.urlShare.vodFilePath
            }

            MenuItem {
                text: "Delete thumbnail"
                onClicked: _c.urlShare.deleteThumbnail()
            }

            MenuItem {
                text: "Delete VOD from database"
                enabled: _toolEnabled
                onClicked: {
                    // still not sure why local vars are needed but they are!!!!
                    var item = root
                    var g = Global
                    item.remorseAction("Deleting " + _c.urlShare.title, function() {
                        item._cancelDownload()
                        g.deleteVods("where id=" + item.rowId)
                    })
                }
            }
        }
    }

    function _play() {
        console.debug("play " + _vodUrl)
        playRequest(root)
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
        if (VM.VM_Smallest === formatId) {
            var best = vod.format(0)
            formatIndex = 0
            for (var i = 1; i < vod.formats; ++i) {
                var f = vod.format(i)
                if (f.height < best.height) {
                    best = f;
                    formatIndex = i;
                }
            }
        } else if (VM.VM_Largest === formatId) {
            var best = vod.format(0)
            formatIndex = 0
            for (var i = 1; i < vod.formats; ++i) {
                var f = vod.format(i)
                if (f.height > best.height) {
                    best = f;
                    formatIndex = i;
                }
            }
        } else {
            // try to find exact match
            for (var i = 0; i < vod.formats; ++i) {
                var f = vod.format(i)
                if (f.format === formatId) {
                    formatIndex = i
                    break
                }
            }

            if (formatIndex === -1) {
                var target = _targetWidth(formatId)
                var bestdelta = Math.abs(vod.format(0).height - target)
                formatIndex = 0
                for (var i = 1; i < vod.formats; ++i) {
                    var f = vod.format(i)
                    var delta = Math.abs(f.width - target)
                    if (delta < bestdelta) {
                        bestdelta = delta;
                        formatIndex = i;
                    }
                }
            }
        }

        return formatIndex
    }

    function _selectFormat(callback) {
        var labels = []
        var values = []
        var _vod = _c.urlShare.metaData
        for (var i = 0; i < _vod.formats; ++i) {
            var f = _vod.format(i)
            labels.push(f.displayName)
            values.push(f.id)
        }

        var dialog = pageStack.push(
            Qt.resolvedUrl("SelectFormatDialog.qml"), {
                        "autoUpdateMenu": false,
                        "labels" : labels,
                        "values": values
                                    })
        dialog.accepted.connect(function () {
            callback(dialog.formatIndex)
        })
        // instantiate the menu
        dialog.updateMenu()
    }

    function _tryPlay() {
        if (_c.urlShare.vodFetchStatus === UrlShare.Fetching) {
            _vodUrl = _c.urlShare.vodFilePath
            _play()
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
            _vodUrl = _c.urlShare.vodFilePath
            _play()
        } else if (_c.urlShare.metaDataFetchStatus === UrlShare.Available) {
            _playStream()
        } else if (_c.urlShare.metaDataFetchStatus !== UrlShare.Fetching) {
            _c.urlShare.fetchMetaData()
        }
    }

    function _playStream() {
        var format = _getVideoFormatFromBearerMode()
        _playStreamWithFormat(format)
    }

    function _playStreamWithFormat(format) {
        var vod = _c.urlShare.metaData
        if (VM.VM_Any === format) {
            _selectFormat(function(index) {
                _vodUrl = vod.format(index).fileUrl
                _playWhenTransitionDone = true
            })
        } else {
            var formatIndex = _findBestFormat(vod, format)
            _vodUrl = vod.format(formatIndex).fileUrl
            _play()
        }
    }

    function _downloadFormat(formatIndex, autoStarted) {
        _c.urlShare.fetchVodFile(formatIndex)
        switch (_vodDownloadExplicitlyStarted) {
        case -1:
        case 0:
            _vodDownloadExplicitlyStarted = autoStarted ? 0 : 1
            break
        }
    }

    function _download(autoStarted, format) {
        if (VM.VM_Any === format) {
            _selectFormat(function(index) {
                _downloadFormat(index, autoStarted)
            })
        } else {
            var formatIndex = _findBestFormat(_c.urlShare.metaData, format)
            _downloadFormat(formatIndex, autoStarted)
        }
    }

    function _cancelDownload() {
        _c.urlShare.cancelFetchVodFile()
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

    function _targetWidth(format) {
        switch (format) {
        case VM.VM_240p:
            return 240
        case VM.VM_360p:
            return 360
        case VM.VM_720p:
            return 720
        default:
            return 1080
        }
    }


    function updateStartOffset() {
        var offset = VodDataManager.recentlyWatched.offset(VodDataManager.recentlyWatched.vodKey(rowId));
        if (offset >= 0) {
            startOffset = offset
        } else {
            startOffset = baseOffset // base offset into multi match video
        }
    }
}

