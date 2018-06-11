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
    property string eventFullName
    property string stageName
    property string side1
    property string side2
    property string matchName
    property int race1: -1
    property int race2: -1
    property date matchDate
    property bool seen: false
    property bool showDate: true
    property bool showSides: (side1 && side2) || (side1 && !_vodTitle)
    property bool showTitle: !side2
    property bool menuEnabled: true
    property int rowId: -1
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
    property bool _seen: false
    property bool _tryingToPlay: false
    property bool _metaDataDownloadFailed: false
    property bool _vodDownloadFailed: false
    property real _progress: -1

    menu: menuEnabled ? contextMenu : null
    signal playRequest(var self)
    readonly property string title: titleLabel.text
    readonly property string thumbnailSource: thumbnail.source

    ProgressOverlay {
        id: progressOverlay
        show: false
        progress: 0

        anchors.fill: parent

        Item {
            x: Theme.horizontalPageMargin
            width: parent.width - 2*x
            height: parent.height

            Column {
                id: heading
                width: parent.width

                Label {
                    id: eventStageLabel
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

                Item {
                    height: dateLabel.height
                    width: parent.width
//                    visible: !!eventFullName || !!stageName
                    visible: false

                    Label {
                        id: dateLabel
        //                    visible: showDate || _loading
                        anchors.left: parent.left
                        anchors.right: loadCompleteImageHeading.left
        //                horizontalAlignment: Text.AlignLeft
                        font.pixelSize: Theme.fontSizeTiny
                        truncationMode: TruncationMode.Fade
                        anchors.verticalCenter: parent.verticalCenter
                        text: {
                            var result = ""
                            if (showDate) {
                                result = Qt.formatDate(matchDate)
                            }

//                            if (result.length > 0) {
//                                result += ", "
//                            }

                            //result += (progressOverlay.progress * 100).toFixed(0) + "% loaded"
                            if (_width > 0) {
                                if (result) {
                                    result += ", "
                                }

                                result += _width + "x" + _height
                            }

                            return result
                        }
                    }

                    Image {
                        id: loadCompleteImageHeading
                        source: "image://theme/icon-s-cloud-download"
                        visible: false
                    }

                    ProgressMaskEffect {
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.right: parent.right
                        anchors.fill: parent
                        src: loadCompleteImageHeading
                        progress: {
                            var p = progressOverlay.show ? 0 : progressOverlay.progress
                            var s = Math.max(0, Math.min(p, 1))
                            if (s > 0) {
                                if (s >= 1) {
                                    return 1
                                }

                                return 0.1 + s * 0.8 // ignore transparent sides
                            }

                            return 0
                        }
                    }
                }
            }

            Item {
                width: parent.width
                height: parent.height - heading.height
                anchors.top: heading.bottom

                Item {
                    id: thumbnailGroup
                    width: Global.itemHeight
                    height: Global.itemHeight
                    anchors.verticalCenter: parent.verticalCenter
                    property bool downloadFailed: false

                    Image {
                        id: thumbnail
                        anchors.fill: parent
                        sourceSize.width: width
                        sourceSize.height: height
                        fillMode: Image.PreserveAspectFit
                        // prevents the image from loading on device
                        //asynchronous: true
                        visible: status === Image.Ready && !thumbnailGroup.downloadFailed

                        MouseArea {
                            anchors.fill: parent
                            enabled: parent.visible
                            onClicked: VodDataManager.fetchThumbnail(rowId)
                        }
                    }

                    Image {
                        id: reloadThumbnail
                        source: source = "image://theme/icon-m-reload"
                        anchors.centerIn: parent
                        visible: thumbnailGroup.downloadFailed

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                thumbnailGroup.downloadFailed = false
                                VodDataManager.fetchThumbnail(rowId)
                            }
                        }
                    }

                    BusyIndicator {
                        size: BusyIndicatorSize.Medium
                        anchors.centerIn: parent
                        running: visible
                        visible: !thumbnail.visible && !reloadThumbnail.visible
                    }
                }

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
                    }

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

    //                            if (result.length > 0) {
    //                                result += ", "
    //                            }

                                //result += (progressOverlay.progress * 100).toFixed(0) + "% loaded"
                                if (_width > 0) {
                                    if (result) {
                                        result += ", "
                                    }

                                    result += _width + "x" + _height
                                }

                                return result
                            }
                        }

                        Row {
                            id: loadCompleteImage2
                            anchors.right: parent.right
                            spacing: Theme.paddingSmall / 2


                            Item {
                                visible: _clicked && !_vod && !_metaDataDownloadFailed
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
                                source: _metaDataDownloadFailed
                                        ? //"/usr/share/harbour-starfish/icons/warning.png"
                                          "/usr/share/harbour-starfish/icons/flash.png"
                                        : "image://theme/icon-s-date"

                                anchors.verticalCenter: parent.verticalCenter
                                visible: !!_vod || _metaDataDownloadFailed
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
                                    visible: !_vodDownloadFailed && progressOverlay.progress < 1
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
                                    visible: !_vodDownloadFailed && progressOverlay.progress >= 1
                                }

                                Image {
                                    width: Theme.iconSizeSmall
                                    height: Theme.iconSizeSmall
                                    sourceSize.width: width
                                    sourceSize.height: height
                                    source: "/usr/share/harbour-starfish/icons/flash.png"
                                    anchors.verticalCenter: parent.verticalCenter
                                    visible: _vodDownloadFailed
                                }
                            }
                        }
                    }


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
//                    width: parent.height
//                    height: parent.height
//                    anchors.right: loadingIndicator.left
                    anchors.right: parent.right
                    enabled: rowId >= 0
                    icon.source: seen ? "image://theme/icon-m-favorite-selected" : "image://theme/icon-m-favorite"
                    anchors.verticalCenter: parent.verticalCenter
                    property bool seen: false

                    onClicked: {
                        seen = !seen
                        console.debug("seen=" + seen)
                        VodDataManager.setSeen({"id": rowId}, seen)
                    }

                }

//                Item {
//                    id: loadingIndicator
//                    visible: _clicked && !_vod
//                    width: visible ? parent.height : 0
//                    height: parent.height
//                    anchors.right: parent.right

//                    BusyIndicator {
//                        size: BusyIndicatorSize.Medium
//                        anchors.centerIn: parent
//                        running: parent.visible
//                    }
//                }
            }
        }
    }

    onClicked: {
        _clicking = true
        _clicked = true
        if (!_vod && App.isOnline) {
            VodDataManager.fetchMetaData(rowId)
        } else if (_vodFilePath) {
            _tryPlay()
        }
        _clicking = false
    }

    on_MetaDataDownloadFailedChanged: {
        console.debug("_metaDataDownloadFailed=" + _metaDataDownloadFailed)
    }

    on_ClickedChanged: {
        console.debug("_clicked=" + _clicked)
    }

    on_VodChanged: {
        console.debug("_vod=" + _vod)
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



    Component.onCompleted: {
        VodDataManager.vodAvailable.connect(vodAvailable)
        VodDataManager.thumbnailAvailable.connect(thumbnailAvailable)
        VodDataManager.thumbnailDownloadFailed.connect(thumbnailDownloadFailed)
        VodDataManager.metaDataAvailable.connect(metaDataAvailable)
        VodDataManager.metaDataDownloadFailed.connect(metaDataDownloadFailed)
        VodDataManager.vodDownloadFailed.connect(vodDownloadFailed)
        _vodTitle = VodDataManager.title(rowId)
        seenButton.seen = VodDataManager.seen({"id": rowId}) >= 1
        if (App.isOnline) {
            VodDataManager.fetchThumbnail(rowId)
        } else {
            thumbnailGroup.downloadFailed = true
        }

        // also fetch a valid meta data from cache
        VodDataManager.fetchMetaDataFromCache(rowId)
        VodDataManager.queryVodFiles(rowId)
        console.debug("create match item rowid=" + rowId)
    }

    Component.onDestruction: {
        if (!settingNetworkContinueDownload.value) {
            console.debug("canceling download for rowid=" + rowId)
            _cancelDownload()
        }

        VodDataManager.cancelFetchMetaData(rowId)
        console.debug("destroy match item rowid=" + rowId)
    }

    Component {
        id: contextMenu
        ContextMenu {
            MenuItem {
                text: "Download VOD"
                visible: rowId >= 0 && !!_vod && !_downloading && progressOverlay.progress < 1 && App.isOnline
                onClicked: _download()
            }

            MenuItem {
                text: "Cancel VOD download"
                visible: rowId >= 0 && _downloading && progressOverlay.progress < 1
                onClicked: _cancelDownload()
            }

            MenuItem {
                text: "Play stream"
                visible: rowId >= 0 && !!_vod && !!_vodFilePath
                onClicked: _playStream()
            }

            MenuItem {
                text: "Delete meta data"
                enabled: rowId >= 0
                onClicked: {
                    _cancelDownload()
                    progressOverlay.progress = 0
                    _clicked = false
                    _vodUrl = ""
//                    _vodFilePath = ""
                    _vod = null
                    VodDataManager.deleteMetaData(rowId)
                    if (App.isOnline) {
                        VodDataManager.fetchMetaData(rowId)
                        VodDataManager.fetchThumbnail(rowId)
                    }
                }
            }

            MenuItem {
                text: "Delete VOD"
                enabled: rowId >= 0
                onClicked: {
                    _cancelDownload()
                    progressOverlay.progress = 0
                    _clicked = false
                    _vodUrl = ""
                    _vodFilePath = ""
                    _vodFileSize = 0
                    _width = 0
                    _height = 0
                    _formatId = ""
                    VodDataManager.deleteVod(rowId)
                }
            }
        }
    }

    function thumbnailDownloadFailed(rowid, error, url) {
        if (rowid === rowId) {
            console.debug("thumbnail download failed rowid=" + rowid + " error=" + error + " url=" + url)
            thumbnailGroup.downloadFailed = true
        }
    }

    function metaDataDownloadFailed(rowid, error) {
        if (rowid === rowId) {
            console.debug("meta data download failed rowid=" + rowid + " error=" + error)
            _metaDataDownloadFailed = true
            if (_clicking || _clicked) {
                 _clicked = false
                _tryPlay()
            }
        }
    }

    function vodDownloadFailed(rowid, error) {
        if (rowid === rowId) {
            console.debug("vod download failed rowid=" + rowid + " error=" + error)
            _vodDownloadFailed = true
            _downloading = false
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
                _downloading = true
                _progress = progress
            }

            progressOverlay.show = _downloading && progress > 0 && progress < 1
            progressOverlay.progress = progress
            _width = width
            _height = height
            _formatId = formatId
        }
    }

    function thumbnailAvailable(rowid, filePath) {
        if (rowid === rowId) {
            console.debug("thumbnailAvailable rowid=" + rowid + " path=" + filePath)
            thumbnail.source = "" // force reload
            thumbnail.source = filePath
        }
    }

    function metaDataAvailable(rowid, vod) {
        if (rowid === rowId) {
            console.debug("metaDataAvailable rowid=" + rowid)
            _metaDataDownloadFailed = false
            _vod = vod
            _vodTitle = vod.description.title
            VodDataManager.fetchThumbnail(rowId)
            if (_clicking || _clicked) {
                 _clicked = false
                _tryPlay()
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
                var target = targetWidth(formatId)
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
                        "labels" : labels,
                        "values": values
                                    })
        dialog.accepted.connect(function () {
            callback(dialog.formatIndex)
        })
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
                        _downloadFormat(index)
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

    function _playStream() {
        var format = _getVideoFormatFromBearerMode()
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

    function _downloadFormat(formatIndex) {
        _vodDownloadFailed = false
        VodDataManager.fetchVod(rowId, formatIndex)
        _downloading = true
    }

    function _download() {
        var format = _getVideoFormatFromBearerMode()
        if (VM.VM_Any === format) {
            _selectFormat(function(index) {
                _downloadFormat(index)
            })
            return
        }

        var formatIndex = _findBestFormat(_vod, format)
        _downloadFormat(formatIndex)
    }

    function _cancelDownload() {
        VodDataManager.cancelFetchVod(rowId)
        _progress = -1
        _downloading = false
        progressOverlay.show = false
    }

}

