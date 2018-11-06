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

ListItem {
    id: root
    objectName: "MatchItem" // poor type check to call ownerGone()
    contentHeight: requiredHeight
    property string table
    property string eventFullName
    property string stageName
    property string side1
    property string side2
    property string matchName
    property int race1: -1
    property int race2: -1
    property date matchDate
    property bool showDate: true
    property bool showSides: (side1 && side2) || (side1 && !_vodTitle)
    property bool showTitle: !side2
    property bool menuEnabled: true
    property int rowId: -1
    property string url
    property string _vodFilePath
    property int _vodFileSize
    property bool _fetchingVod: false
    property bool _clicked: false
    property bool _clicking: false
    property bool _playWhenTransitionDone: false
    property variant _vod
    readonly property string vodUrl: _vodUrl
    property string _vodUrl
    property string _vodTitle
    property string _formatId
    property int startOffset: 0
    property bool _downloading: false
    property int _width: 0
    property int _height: 0
    property int baseOffset: 0
    property int length: 0
    property int _endOffset: 0
    property bool _validRange: baseOffset >= 0 && baseOffset < _endOffset
    property bool _tryingToPlay: false
    property int _metaDataState: metaDataStateInitial
    property int _thumbnailState: thumbnailStateInitial
    property bool _vodDownloadFailed: false
    property real _progress: -1
    property string _thumbnailFilePath
    readonly property int metaDataStateFetching: -2
    readonly property int metaDataStateInitial: -1
    readonly property int metaDataStateDownloadFailed: 0
    readonly property int metaDataStateAvailable: 1
    readonly property int thumbnailStateFetching: -2
    readonly property int thumbnailStateInitial: -1
    readonly property int thumbnailStateDownloadFailed: 0
    readonly property int thumbnailStateAvailable: 1
    readonly property string _where: " where id=" + rowId
    readonly property bool _hasValidRaces: race1 >= 1 && race2 >= 1
    readonly property int requiredHeight: content.height + watchProgress.height
    readonly property string title: titleLabel.text
    readonly property string thumbnailSource: thumbnail.source

    menu: menuEnabled ? contextMenu : null
    signal playRequest(var self)



    ProgressOverlay {
        id: progressOverlay
        show: false
        progress: 0

        anchors.fill: parent

        Column {
            id: content
            x: Theme.horizontalPageMargin
            width: parent.width - 2*x

            Label {
                width: parent.width
                visible: !!eventFullName || !!stageName
                truncationMode: TruncationMode.Fade
                font.pixelSize: Theme.fontSizeSmall
                text: {
                    var result = eventFullName
                    if (stageName) {
                        if (result) {
                            result += " / " + stageName
                        } else {
                            result = stageName
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
                        anchors.centerIn: parent
                        height: _thumbnailState === thumbnailStateAvailable ? parent.height : thumbnailBusyIndicator.height
                        width: _thumbnailState === thumbnailStateAvailable ? parent.width : thumbnailBusyIndicator.width
                        sourceSize.width: width
                        sourceSize.height: height
                        fillMode: Image.PreserveAspectFit
                        // prevents the image from loading on device
                        //asynchronous: true
                        visible: status === Image.Ready && _thumbnailState !== thumbnailStateFetching
                        source: _thumbnailState === thumbnailStateAvailable ? _thumbnailFilePath : "image://theme/icon-m-reload"

                        MouseArea {
                            anchors.fill: parent
                            enabled: parent.visible && _thumbnailState !== thumbnailStateAvailable
                            onClicked: _fetchThumbnail()
                        }
                    }

                    BusyIndicator {
                        id: thumbnailBusyIndicator
                        size: BusyIndicatorSize.Medium
                        anchors.centerIn: parent
                        running: visible
                        visible: _thumbnailState === thumbnailStateFetching
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
                                if (side2) {
                                    return side1 + " - " + side2
                                }

                                return side1
                            }

                            if (showTitle && _vodTitle) {
                                return _vodTitle
                            }

                            return matchName
                        }
                    } // title/vs label

                    Item {
                        id: sidesWithRaceLogosGroup
                        width: parent.width
                        height: Math.max(race1Icon.height, side1Name.height)
                        visible: showSides && _hasValidRaces
                        property real labelWidth: (width - 2 * race1Icon.width) * 0.5

                        Image {
                            id: race1Icon
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            width: Theme.iconSizeSmall
                            height: Theme.iconSizeSmall
                            sourceSize.width: width
                            sourceSize.height: height
                            source: VodDataManager.raceIcon(race1)
                        }

                        Label {
                            id: side1Name
                            anchors.left: race1Icon.right
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.labelWidth
                            truncationMode: TruncationMode.Fade
                            text: " " + side1
                            horizontalAlignment: Text.AlignLeft
                        }

                        Label {
                            id: side2Name
                            anchors.right: race2Icon.left
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.labelWidth
                            truncationMode: TruncationMode.Fade
                            text: side2  + " "
                            horizontalAlignment: Text.AlignRight
                        }

                        Image {
                            id: race2Icon
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            width: Theme.iconSizeSmall
                            height: Theme.iconSizeSmall
                            sourceSize.width: width
                            sourceSize.height: height
                            source: VodDataManager.raceIcon(race2)
                        }
                    } // vs label with race icons

                    Item {
                        height: dateLabel2.height
                        width: parent.width
//                        visible: !eventFullName && !stageName


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
                                    result = Qt.formatDate(matchDate)
                                }

                                if (_width > 0) {
                                    if (result) {
                                        result += ", "
                                    }

                                    result += _width + "x" + _height
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
                                visible: _metaDataState === metaDataStateFetching
                                width: visible ? parent.height : 0
                                height: parent.height
                                anchors.verticalCenter: parent.verticalCenter

                                BusyIndicator {
                                    id: loadingIndicator
                                    size: BusyIndicatorSize.Small
                                    anchors.centerIn: parent
                                    running: parent.visible
                                }
                            }

                            Image {
                                width: Theme.iconSizeSmall
                                height: Theme.iconSizeSmall
                                sourceSize.width: width
                                sourceSize.height: height
                                source: _metaDataState == metaDataStateDownloadFailed
                                        ? //"/usr/share/harbour-starfish/icons/warning.png"
                                          "/usr/share/harbour-starfish/icons/flash.png"
                                        : "image://theme/icon-s-date"

                                anchors.verticalCenter: parent.verticalCenter
                                visible: _metaDataState === metaDataStateAvailable || _metaDataState === metaDataStateDownloadFailed
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
                                    visible: !_downloading && !_vodDownloadFailed && progressOverlay.progress < 1
                                    progress: {
                                        var s = Math.max(0, Math.min(progressOverlay.progress, 1))
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
                                    visible: !_downloading && !_vodDownloadFailed && progressOverlay.progress >= 1
                                }

                                Image {
                                    width: Theme.iconSizeSmall
                                    height: Theme.iconSizeSmall
                                    sourceSize.width: width
                                    sourceSize.height: height
                                    source: "/usr/share/harbour-starfish/icons/flash.png"
                                    anchors.verticalCenter: parent.verticalCenter
                                    visible: !_downloading && _vodDownloadFailed
                                }

                                BusyIndicator {
                                    id: downloadingVodIndicator
                                    size: BusyIndicatorSize.Small
                                    anchors.centerIn: parent
                                    running: _downloading
                                }
                            }
                        }
                    } // date / format / status group


//                    Label {
//                        visible: !eventFullName && !stageName
//                        width: parent.width
//                        horizontalAlignment: Text.AlignLeft
//                        font.pixelSize: Theme.fontSizeTiny
//                        truncationMode: TruncationMode.Fade
//                        text: {
//                            var result = ""
//                            if (showDate) {
//                                result = Qt.formatDate(matchDate)
//                            }

//                            if (result.length > 0) {
//                                result += ", "
//                            }

//                            result += (progressOverlay.progress * 100).toFixed(0) + "% downloaded"
//                            if (_width > 0) {
//                                result += ", " + _width + "x" + _height
//                            }

//                            return result
//                        }
//                    }
                }

                IconButton {
                    id: seenButton
                    anchors.right: parent.right
                    enabled: rowId >= 0
                    icon.source: seen ? "image://theme/icon-m-favorite-selected" : "image://theme/icon-m-favorite"
                    anchors.verticalCenter: parent.verticalCenter
                    property bool seen: false

                    onClicked: {
                        seen = !seen
                        console.debug("seen=" + seen)
                        VodDataManager.setSeen(table, _where, seen)
                    }

                }
            }
        }

        Rectangle {
            id: watchProgress
            color: Theme.highlightColor
            opacity: Theme.highlightBackgroundOpacity
            width: parent.width * _range
            visible: !seenButton.seen && _validRange
            readonly property real _range:
                startOffset < baseOffset
                ? 0
                : (startOffset > _endOffset ? 1 : (startOffset-baseOffset)/(_endOffset-baseOffset))
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

    onClicked: {
        _clicking = true
        _clicked = true

        if (_vod) {
            _tryPlay()
        } else {
            if (_vodFilePath && _progress >= 1) {
                // play fully downloaded vods immediately
                _tryPlay()
            } else {
                switch (_metaDataState) {
                case metaDataStateAvailable:
                    // should have _vod then
                    console.debug("OHHH NOOOOOESS")
                    break
                case metaDataStateDownloadFailed:
                    if (_vodFilePath) {
                        _tryPlay()
                    }
                    break
                case metaDataStateFetching:
                    // wait for meta data,
                    // playing now will abort md fetch
                    break
                case metaDataStateInitial:
                    if (App.isOnline) {
                        _fetchMetaData()
                    } else {
                        _metaDataState = metaDataStateDownloadFailed
                    }
                    break
                }
            }
        }

//        if (!_vod) {
//            if (_fetchingMetaData) {
//                // don't play, player will cancel meta data fetch
//            } else {
//                if (App.isOnline) {
//                    // Fetch meta data only if online
//                    // Can't fetch in the background, onDestruction will
//                    // cancel the fetch upon push of video player page

//                } else {
//                    _metaDataDownloadFailed = true
//                }
//            }
//        } else if (_vodFilePath || _vod) {

//        }
        _clicking = false
    }

    on_ClickedChanged: {
        console.debug("_clicked=" + _clicked)
    }

    Connections {
        target: pageStack
        onBusyChanged: {
            if (!pageStack.busy && _playWhenTransitionDone) {
                _playWhenTransitionDone = false
                _play()
            }
        }
    }

    Connections {
        target: App
        onIsOnlineChanged: {
            if (App.isOnline) {
                if (_metaDataState === metaDataStateDownloadFailed) {
                    _metaDataState = metaDataStateInitial
                }

                if (_thumbnailState === thumbnailStateDownloadFailed) {
                    _thumbnailState = thumbnailStateInitial
                }
            }
        }
    }

    Component.onCompleted: {
        VodDataManager.vodAvailable.connect(vodAvailable)
        VodDataManager.thumbnailAvailable.connect(thumbnailAvailable)
        VodDataManager.thumbnailDownloadFailed.connect(thumbnailDownloadFailed)
        VodDataManager.fetchingMetaData.connect(fetchingMetaData)
        VodDataManager.metaDataAvailable.connect(metaDataAvailable)
        VodDataManager.metaDataDownloadFailed.connect(metaDataDownloadFailed)
        VodDataManager.vodDownloadFailed.connect(vodDownloadFailed)
        VodDataManager.vodDownloadCanceled.connect(vodDownloadCanceled)
        _vodTitle = VodDataManager.title(rowId)
        seenButton.seen = VodDataManager.seen(table, _where) >= 1
//        console.debug("rowid=" + rowId + " baseOffset=" + baseOffset+ " length=" + length)
        _endOffset = VodDataManager.vodEndOffset(rowId, baseOffset, length)
//        console.debug("rowid=" + rowId + " endoffset=" + _endOffset)

        // also fetch a valid meta data from cache
        VodDataManager.fetchMetaDataFromCache(rowId)
        VodDataManager.fetchThumbnailFromCache(rowId)
        VodDataManager.queryVodFiles(rowId)
        if (App.isOnline) {
            _fetchThumbnail()
        }

//        console.debug("create match item rowid=" + rowId)

        if (requiredHeight > height) {
            console.warn("rowid " + rowId + " match item height is " + height + " which is less than required height " + requiredHeight)
        }
    }

    Component.onDestruction: {
        if (_metaDataState === metaDataStateFetching) {
            VodDataManager.cancelFetchMetaData(rowId)
        }
//        console.debug("destroy match item rowid=" + rowId)
    }

    RemorsePopup { id: remorse }

    Component {
        id: contextMenu
        ContextMenu {
            MenuItem {
                text: "Copy URL to clipboard"
                visible: rowId >= 0
                onClicked: Clipboard.text = url
            }

            MenuItem {
                text: "Download meta data"
                visible: rowId >= 0 && !_vod && App.isOnline
                onClicked: {
                    _fetchMetaData()
                }
            }
            MenuItem {
                text: "Download VOD"
                visible: rowId >= 0 && !!_vod && !_downloading && progressOverlay.progress < 1 && App.isOnline
                onClicked: {
                    var format = _getVideoFormatFromBearerMode()
                    _download(false, format)
                }
            }

            MenuItem {
                text: "Download VOD with format..."
                visible: rowId >= 0 && !!_vod && !_downloading && progressOverlay.progress < 1 && App.isOnline
                onClicked: _download(false, VM.VM_Any)
            }

            MenuItem {
                text: "Cancel VOD download"
                visible: rowId >= 0 && _downloading && progressOverlay.progress < 1
                onClicked: _cancelDownload()
            }

            MenuItem {
                text: "Play stream"
                visible: rowId >= 0 && !!_vod && App.isOnline //&& !!_vodFilePath
                onClicked: _playStream()
            }

            MenuItem {
                text: "Play stream with format..."
                visible: rowId >= 0 && !!_vod && App.isOnline //&& !!_vodFilePath
                onClicked: _playStreamWithFormat(VM.VM_Any)
            }

            MenuItem {
                text: "Delete meta data"
                visible: rowId >= 0 && !!_vod
                onClicked: {
                    _cancelDownload()
                    progressOverlay.progress = 0
                    _clicked = false
                    _vodUrl = ""
                    _vod = null
                    VodDataManager.deleteMetaData(rowId)
                    if (App.isOnline) {
                        _fetchMetaData()
                        _fetchThumbnail()
                    }
                }
            }

            MenuItem {
                text: "Delete VOD file"
                visible: rowId >= 0 && !!_vodFilePath
//                onClicked: {
//                    remorse.execute("Deleting VOD " + title, function() {
//                        _cancelDownload()
//                        progressOverlay.progress = 0
//                        _clicked = false
//                        _vodUrl = ""
//                        _vodFilePath = ""
//                        _vodFileSize = 0
//                        _width = 0
//                        _height = 0
//                        _formatId = ""
//                        VodDataManager.deleteVod(rowId)
//                    })
//                }

                onClicked: {
                    // still not sure why local vars are needed
                    var item = root
                    var overlay = progressOverlay
                    var manager = VodDataManager
                    item.remorseAction("Deleting " + title, function() {
                        item._cancelDownload()
                        overlay.progress = 0
                        item._clicked = false
                        item._vodUrl = ""
                        item._vodFilePath = ""
                        item._vodFileSize = 0
                        item._width = 0
                        item._height = 0
                        item._formatId = ""
                        manager.deleteVod(item.rowId)
                    })
                }
            }

            MenuItem {
                text: "VOD details"
                visible: rowId >= 0 && _vodFileSize > 0 && !!_vodFilePath
                onClicked: pageStack.push(
                               Qt.resolvedUrl("VodDetailPage.qml"),
                               {
                                   vodTitle: titleLabel.text,
                                   vodFilePath: _vodFilePath,
                                   vodFileSize: _vodFileSize,
                                   vodWidth: _width,
                                   vodHeight: _height,
                                   vodRowId: rowId,
                                   vodProgress: _progress
                               })
            }

            MenuItem {
                text: "Copy VOD file path to clipboard"
                visible: rowId >= 0 && _vodFileSize > 0 && !!_vodFilePath
                onClicked: Clipboard.text = _vodFilePath
            }
        }
    }

    function thumbnailAvailable(rowid, filePath) {
        if (rowid === rowId) {
//            console.debug("thumbnailAvailable rowid=" + rowid + " path=" + filePath)
            _thumbnailFilePath = "" // force reload
            _thumbnailFilePath = filePath
            _thumbnailState = thumbnailStateAvailable
        }
    }

    function thumbnailDownloadFailed(rowid, error, url) {
        if (rowid === rowId) {
            console.debug("thumbnail download failed rowid=" + rowid + " error=" + error + " url=" + url)
            _thumbnailState = thumbnailStateDownloadFailed
        }
    }



    function fetchingMetaData(rowid) {
        if (rowid === rowId) {
            _metaDataState = metaDataStateFetching
        }
    }

    function metaDataDownloadFailed(rowid, error) {
        if (rowid === rowId) {
            console.debug("meta data download failed rowid=" + rowid + " error=" + error)
            _metaDataState = metaDataStateDownloadFailed
            _clicked = false
            if (_thumbnailState === thumbnailStateFetching) {
                _thumbnailState = thumbnailStateDownloadFailed
            }
        }
    }

    function metaDataAvailable(rowid, vod) {
        if (rowid === rowId) {
//            console.debug("metaDataAvailable rowid=" + rowid)
            _metaDataState = metaDataStateAvailable
            _vod = vod
            _vodTitle = vod.description.title
            _clicked = false

            // also set length which might be zero if we just downloaded the meta data
            length = vod.description.duration

            // now that we have meta data, we might
            // be able to get a watch progress
//            console.debug("rowid=" + rowId + " baseOffset=" + baseOffset+ " length=" + length)
            _endOffset = VodDataManager.vodEndOffset(rowId, baseOffset, length)
//            console.debug("rowid=" + rowId + " endoffset=" + _endOffset)


            if (App.isOnline) {
                _fetchThumbnail()
            }

            if (_clicking) {
                _tryPlay()
            }
        }
    }

    function vodDownloadFailed(rowid, error) {
        if (rowid === rowId) {
            console.debug("vod download failed rowid=" + rowid + " error=" + error)
            _vodDownloadFailed = true
            _downloading = false
            progressOverlay.show = false
            _progress = -1
        }
    }

    function vodDownloadCanceled(rowid) {
        if (rowid === rowId) {
            console.debug("vod download canceled rowid=" + rowid)
            _vodDownloadFailed = false
            _downloading = false
            progressOverlay.show = false
            _progress = -1
        }
    }

    function _play() {
        console.debug("play " + _vodUrl)
        playRequest(root)
    }

    function vodAvailable(rowid, filePath, progress, fileSize, width, height, formatId) {
        if (rowid === rowId) {
            console.debug(
                        "vodAvailable rowid=" + rowid + " path=" + filePath + " progress=" + progress +
                        " size=" + fileSize + " width=" + width + " height=" + height + " formatId=" + formatId)
            _vodFilePath = filePath
            _vodFileSize = fileSize
            if (-1 === _progress) {
                _progress = progress
            } else if (_progress < progress) {
                _downloading = progress < 1
                _progress = progress
            }

            progressOverlay.show = _downloading && progress > 0 && progress < 1
            progressOverlay.progress = progress
            _width = width
            _height = height
            _formatId = formatId
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

    function _getVodFormatIndexFromId() {
        for (var i = 0; i < _vod.formats; ++i) {
            var f = _vod.format(i)
            if (f.id === _formatId) {
                return i
            }
        }

        return -1
    }

    function _tryPlay() {
        if (!_tryingToPlay) {
            _tryingToPlay = true
            if (_vodFilePath && _vodFileSize > 0) {
                // try download rest if incomplete and format matches
                if (_vod && progressOverlay.progress < 1 && !_downloading) {
                    var index = _getVodFormatIndexFromId()
                    if (index >= 0) {
                        _downloadFormat(index, true)
                    }
                }

                // at any rate, play the file
                _vodUrl = _vodFilePath
                _play()
            } else if (_vod) {
                _playStream()
            }
            _tryingToPlay = false
        }
    }

    function _playStream(format) {
        var format = _getVideoFormatFromBearerMode()
        _playStreamWithFormat(format)
    }

    function _playStreamWithFormat(format) {
        if (VM.VM_Any === format) {
            _selectFormat(function(index) {
                _vodUrl = _vod.format(index).fileUrl
                _playWhenTransitionDone = true
            })
            return
        }

        var formatIndex = _findBestFormat(_vod, format)
        _vodUrl = _vod.format(formatIndex).fileUrl
        _play()
    }

    function _downloadFormat(formatIndex, autoStarted) {
        _vodDownloadFailed = false
        VodDataManager.fetchVod(rowId, formatIndex, autoStarted)
        _downloading = true
    }

    function _download(autoStarted, format) {
        if (VM.VM_Any === format) {
            _selectFormat(function(index) {
                _downloadFormat(index, autoStarted)
            })
            return
        }

        var formatIndex = _findBestFormat(_vod, format)
        _downloadFormat(formatIndex, autoStarted)
    }

    function _cancelDownload() {
        VodDataManager.cancelFetchVod(rowId)
        _progress = -1
        _downloading = false
        progressOverlay.show = false
    }

    function _fetchMetaData() {
        _metaDataState = metaDataStateFetching
        VodDataManager.fetchMetaData(rowId)
    }

    function _fetchThumbnail() {
        _thumbnailState = thumbnailStateFetching
        VodDataManager.fetchThumbnail(rowId)
    }

    function cancelImplicitVodFileFetch() {
        if (VodDataManager.implicitlyStartedVodFetch(rowId)) {
            console.debug("canceling download for rowid=" + rowId)
            _cancelDownload()
        }
    }

    function ownerGone() {
        cancelImplicitVodFileFetch()
        if (!settingNetworkContinueDownload.value) {
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
}

