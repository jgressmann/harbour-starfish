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
import QtGraphicalEffects 1.0
import QtMultimedia 5.0
import Sailfish.Silica 1.0
import Nemo.KeepAlive 1.2
import org.duckdns.jgressmann 1.0
import ".."

Page {
    id: page
    property int _seekFixup: 0
    property int _seekPositionMs: 0
    property int _currentUrlIndex: -1

    property bool _startSeek: false
    property bool _reseekOnDurationChange: false
    property string title
    property real _volume

    property int playbackAllowedOrientations: Orientation.Landscape | Orientation.LandscapeInverted
    backNavigation: controlPanel.open || !_hasSource



    readonly property int playbackOffset: _streamPositionS
    readonly property int _streamPositionMs: _streamBasePositionS * 1000 + mediaplayer.position
    readonly property int _streamPositionS: Math.floor(_streamPositionMs * 1e-3)
    readonly property int _streamMaxPositionS: Math.max(_streamBasePositionS, _streamPositionS)
    property int _streamBasePositionS: 0
    property int _streamDurationS: 0


    property bool _forceBusyIndicator: false
    property bool _hasSource: _playlist ? _playlist.isValid : false
    property bool _paused: false
    property int _pauseCount: 0
    property int _openCount: 0
    property bool _pauseDueToStall: false
    property bool _clickedToOpen: false
    property var openHandler
    property bool _grabFrameWhenReady: false
    property int _grabFrameControlPanelOpens: 0
    readonly property bool isPlaying: mediaplayer.playbackState === MediaPlayer.PlayingState
    property string thumbnailFilePath
    property string coverFilePath

    signal videoFrameCaptured(string filepath)
    signal videoCoverCaptured(string filepath)
    signal videoThumbnailCaptured(string filepath)
    signal videoStreamLengthAvailable(int seconds)
    signal bye()

    on_StreamPositionMsChanged: {
        console.debug("stream position ms=" + _streamPositionMs)
        positionSlider.setPosition(_streamPositionMs)
    }

    on_CurrentUrlIndexChanged: {
        console.debug("current url index=" + _currentUrlIndex)
    }

    on_StreamBasePositionSChanged: {
        console.debug("stream base position=" + _streamBasePositionS)
    }

    VodPlaylist {
        id: _playlist
    }

    Timer {
        id: stallTimer
        interval: 10000
        onTriggered: {
            console.debug("stall timer expired")
            _pauseDueToStall = true
            pause()
            openControlPanel()
        }
    }

    Timer {
        id: reseekOnDurationChangeTimer
        interval: 1
        repeat: false
        onTriggered: {
            console.debug("reseek timer expired")
            _seek(_seekPositionMs)
        }
    }

    DisplayBlanking {
        id: displayBlanking
    }

    MediaPlayer {
        id: mediaplayer
        autoPlay: true

        onStatusChanged: {
            console.debug("media player status=" + status)
            switch (status) {
            case MediaPlayer.Buffering:
                console.debug("buffering")
                stallTimer.start()
                break
            case MediaPlayer.Stalled:
                console.debug("stalled")
                stallTimer.start()
                break
            case MediaPlayer.Buffered:
                console.debug("buffered")
                stallTimer.stop()
                if (_grabFrameWhenReady) {
                    _grabFrameWhenReady = false
                    _grabFrame()

//                    _clickedToOpen = true
//                    openControlPanel() // otherwise video hangs
                }
                if (_pauseDueToStall) {
                    _pauseDueToStall = false
                    resume()
                    closeControlPanel()
                }
                break
            case MediaPlayer.EndOfMedia:
                console.debug("end of media")
                if (_playlist.isValid && _currentUrlIndex >= 0 && _currentUrlIndex + 1 < _playlist.parts) {
                    ++_currentUrlIndex;
                    _streamBasePositionS = _computeStreamBasePosition()
                    mediaplayer.source = _playlist.url(_currentUrlIndex)
                } else {
                    openControlPanel()
                }
                break
            case MediaPlayer.Loaded:
                console.debug("loaded")
                break
            case MediaPlayer.InvalidMedia:
                console.debug("invalid media")
                openControlPanel()
                break
            }
        }

        onPlaybackStateChanged: {
            console.debug("media player playback state=" + playbackState)

            switch (playbackState) {
            case MediaPlayer.PlayingState:
                console.debug("playing")
                break
            case MediaPlayer.PausedState:
                console.debug("paused")
                break
            case MediaPlayer.StoppedState:
                console.debug("stopped")
                break
            default:
                console.debug("unhandled playback state")
                break
            }

            // apparently isPlaying isn't updated here yet, sigh
//            console.debug("isPlaying=" + isPlaying)

            _preventBlanking(playbackState === MediaPlayer.PlayingState)
        }




        onDurationChanged: {
            if (duration > 0) {
                if (_trySetDuration() && _reseekOnDurationChange) {
                    console.debug("reseek")
                    _reseekOnDurationChange = false
                    reseekOnDurationChangeTimer.start()
                }
            }
        }
    }

    Item {
        id: thumbnailOutput
        property real factor: 1.0 / Math.max(videoOutput.width, videoOutput.height)
        width: Theme.iconSizeLarge * factor * videoOutput.width
        height: Theme.iconSizeLarge * factor * videoOutput.height


        ColorOverlay {
            anchors.fill: parent
            source: videoOutput
        }
    }

    Item {
        id: coverOutput
        property real factor: 1.0 / Math.max(videoOutput.width, videoOutput.height)
        width: 512 * factor * videoOutput.width
        height: 512 * factor * videoOutput.height


        ColorOverlay {
            anchors.fill: parent
            source: videoOutput
        }
    }


    Rectangle {
        id: videoOutputRectangle
        anchors.fill: parent
        color: "black"

        onVisibleChanged: {
            if (visible) {
                page.allowedOrientations = playbackAllowedOrientations
            }
        }


        VideoOutput {
            id: videoOutput
            anchors.fill: parent
            source: mediaplayer
            fillMode: VideoOutput.PreserveAspectFit
        }

        BusyIndicator {
            anchors.centerIn: parent
            running: _forceBusyIndicator || (mediaplayer.status === MediaPlayer.Stalled)
            size: BusyIndicatorSize.Medium
        }

// circle looks terrible and blocks out video
//        ProgressCircle {
//            anchors.centerIn: parent
//            visible: value < 1
//            value: mediaplayer.bufferProgress
//            width: Theme.itemSizeLarge
//            height: Theme.itemSizeLarge
//        }

        Rectangle {
            id: seekRectangle
            color: "white"
            width: Theme.horizontalPageMargin * 2 + seekLabel.width
            height: 2*Theme.fontSizeExtraLarge
            visible: false
            anchors.centerIn: parent
            layer.enabled: true
            radius: Theme.itemSizeSmall

            Label {
                id: seekLabel
                color: "black"
                text: "Sir Skipalot"
                font.bold: true
                font.pixelSize: Theme.fontSizeExtraLarge
                anchors.centerIn: parent
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true

            property real startx: -1
            property real starty: -1
//            readonly property real dragDistance: Theme.iconSizeLarge / 2
            readonly property real dragDistance: Theme.startDragDistance

            property bool held: false
            property int seekType: -1
            property real initialVolume: 0

            onPositionChanged: function (e) {
//                console.debug("x=" + e.x + " y="+e.y)
                e.accepted = true

                if (startx >= 0 && starty >= 0) {
                    var dx = e.x - startx
                    var dy = e.y - starty

                    if (-1 === seekType) {
                        if (Math.abs(dx) >= dragDistance ||
                            Math.abs(dy) >= dragDistance) {

                            if (Math.abs(dx) >= Math.abs(dy)) {
                                seekType = 0
                            } else {
                                seekType = 1
                            }
                        }
                    }

                    seekRectangle.visible = seekType !== -1
                    switch (seekType) {
                    case 0: { // position
                        var skipSeconds = computePositionSeek(dx)
                        var streamPosition = Math.max(0, Math.min(_streamPositionS + skipSeconds, _streamDurationS))
                        seekLabel.text = (dx >= 0 ? "+" : "-") + _toTime(Math.abs(skipSeconds)) + " (" + _toTime(streamPosition) + ")"
                    } break
                    case 1: { // volume
                        var volumeChange = -(dy / parent.height)
                        var volume = Math.max(0, Math.min(initialVolume + volumeChange, 1))
                        seekLabel.text = "Volume " + (volume * 100).toFixed(0) + "%"
                        mediaplayer.volume = volume
                    } break
                    }
                }
            }

            onPressed: function (e) {
//                console.debug("pressed")
                if (!controlPanel.open) {
                    startx = e.x;
                    starty = e.y;
                    initialVolume = mediaplayer.volume
                }
            }

            onReleased: function (e) {
//                console.debug("released")

                var dx = e.x - startx
                var dy = e.y - starty

                switch (seekType) {
                case 0: { // position

                        var skipSeconds = computePositionSeek(dx)
                        if (Math.abs(skipSeconds) >= 3) { // prevent small skips
                            var streamPosition = Math.floor(Math.max(0, Math.min(_streamPositionS + skipSeconds, _streamDurationS)))
                            if (streamPosition !== _streamPositionS) {
                                console.debug("skip to=" + streamPosition)
                                _startSeek = false
                                _seek(streamPosition * 1000)
                            }
                        }
                } break
                case 1: { // volume
//                    var volumeChange = -dy / dragDistance
//                    var volume = Math.max(0, Math.min(mediaplayer.volume + volumeChange, 1))
//                    mediaplayer.volume = volume
                } break
                default: {
                    if (!held) {
                        if (controlPanel.open) {
                            if (_clickedToOpen) {
                                _clickedToOpen = false
                                closeControlPanel()
                            }
//                            mediaplayer.play()
                        } else {
//                            mediaplayer.pause()
                            if (!_clickedToOpen) {
                                _clickedToOpen = true
                                openControlPanel()
                            }
                        }
                    }
                } break
                }

                seekType = -1
                held = false
                startx = -1
                starty = -1
                seekRectangle.visible = false
                initialVolume = 0
            }

            onPressAndHold: function (e) {
                console.debug("onPressAndHold")
                e.accepted = true
                held = true
            }

            function computePositionSeek(dx) {
                var sign = dx < 0 ? -1 : 1
                var absDx = sign * dx
                return sign * Math.pow(absDx / dragDistance, 1 + absDx / Screen.width)
            }

            DockedPanel {
                id: controlPanel
                width: parent.width
                height: 2*Theme.itemSizeLarge
                dock: Dock.Bottom

                onOpenChanged: {
                    console.debug("control panel open=" + open)
                    //positionSlider.value = Math.max(0, mediaplayer.position)

//                    // Grab frame when closing again, better to have it than
//                    // to miss it :D
//                    if (_hasSource && mediaplayer.status === MediaPlayer.Buffered) {
//                        _grabFrame()
//                    }
                }

                Column {
                    width: parent.width

                    Slider {
                        id: positionSlider
                        width: parent.width
                        animateValue: false
                        maximumValue: Math.max(1, _streamDurationS * 1000)

                        property int _downPositionMs: 0
                        property int _targetPositionMs: 0

                        onMaximumValueChanged: {
                            console.debug("position slider max=" + maximumValue)
                        }

                        onValueChanged: {
                            console.debug("position value=" + value)
                        }

                        onDownChanged: {
                            console.debug("slider down=" + down)
                            if (down) {
                                seekTimer.stop()
                                _downPositionMs = _streamPositionMs
                            } else {
                                if (Math.abs(sliderValue - _downPositionMs) > 1000) {
                                    _targetPositionMs = sliderValue
                                    seekTimer.start()
                                }
                            }
                        }

                        function setPosition(newValue) {
                            console.debug("target position=" + newValue)
                            if (!positionSlider.down && !seekTimer.running) {
                                value = newValue
                            }
                        }


                        Timer {
                            id: seekTimer
                            running: false
                            interval: 500
                            repeat: false
                            onTriggered: {
                                console.debug("seek from down=" + parent._downPositionMs + " to=" + parent._targetPositionMs)
                                _startSeek = false
                                _seek(parent._targetPositionMs)
                            }
                        }
                    }

                    Item {
                        width: parent.width
                        height: first.height

                        Item {
                            id: leftMargin
                            width: Theme.horizontalPageMargin
                            height: parent.height
                            anchors.left: parent.left
                        }

                        Item {
                            id: rightMargin
                            width: Theme.horizontalPageMargin
                            height: parent.height
                            anchors.right: parent.right
                        }

                        Label {
                            id: first
                            anchors.left: leftMargin.right
                            text: _toTime(_streamPositionS)
                            font.pixelSize: Theme.fontSizeExtraSmall
                        }

                        Label {
                            anchors.right: rightMargin.left
                            text: _toTime(_streamDurationS)
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: Theme.highlightColor
                        }
                    }

                    Item {
                        width: parent.width
                        height: functionalButtonRow.height

                        Item {
                            id: leftMargin2
                            width: Theme.horizontalPageMargin
                            height: parent.height
                            anchors.left: parent.left
                        }

                        Item {
                            id: rightMargin2
                            width: Theme.horizontalPageMargin
                            height: parent.height
                            anchors.right: parent.right
                        }

                        Row {
                            id: functionalButtonRow
                            anchors.centerIn: parent
                            spacing: Theme.paddingLarge

                            IconButton {
                                icon.source: isPlaying ? "image://theme/icon-m-pause" : "image://theme/icon-m-play"
                                anchors.verticalCenter: parent.verticalCenter
                                onClicked: {
                                    console.debug("play/pause")
                                    switch (mediaplayer.playbackState) {
                                    case MediaPlayer.PlayingState:
                                        page.pause()
                                        break
                                    case MediaPlayer.PausedState:
                                    case MediaPlayer.StoppedState:
                                        switch (mediaplayer.status) {
                                        case MediaPlayer.Buffered:
                                            page.resume()
                                            break
                                        case MediaPlayer.EndOfMedia:
                                            if (_playlist.isValid && _currentUrlIndex >= 0 && _currentUrlIndex + 1 == _playlist.parts) {
                                                closeControlPanel()
                                                _startSeek = true
                                                _seek(_playlist.startOffset * 1000) // seek to start of vod
                                                mediaplayer.play()
                                            }
                                            break
                                        }
                                        break
                                    }
                                }
                            }

                            IconButton {
                                visible: false
    //                            source: "image://theme/icon-m-close"
                                icon.source: "image://theme/icon-m-back"

                                anchors.verticalCenter: parent.verticalCenter

                                onClicked: {
                                    console.debug("close")
                                    pageStack.pop()
                                }
                            }

                            IconButton {
                                icon.source: "image://theme/icon-m-folder"
                                visible: !!openHandler

                                anchors.verticalCenter: parent.verticalCenter

                                onClicked: {
                                    console.debug("open")
                                    page.pause()
                                    openHandler()
                                }
                            }
                        }

                        Row {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.right: rightMargin2.left

                            Image {
                                visible: _hasSource
                                width: Theme.iconSizeSmall
                                height: Theme.iconSizeSmall
                                sourceSize: Qt.size(width, height)
                                source: ((mediaplayer.source || "") + "").indexOf("file://") === 0 ? "image://theme/icon-s-like" : "image://theme/icon-s-cloud-download"
                            }
                        }

                        Row {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: leftMargin2.right

                            Label {
                                visible: videoOutput.sourceRect.width > 0 && videoOutput.sourceRect.height > 0
                                text: videoOutput.sourceRect.width + "x" + videoOutput.sourceRect.height
                                font.pixelSize: Theme.fontSizeExtraSmall
                                color: Theme.highlightColor
                            }
                        }
                    }
                }
            }
        }
    }

    Component.onDestruction: {
        _savePlaybackProgress()

        console.debug("destruction")
        _preventBlanking(false)

        mediaplayer.pause()

        bye()
    }

    Timer {
        id: busyTimer
        interval: 1000
        onTriggered: {
            console.debug("_forceBusyIndicator = false")
            _forceBusyIndicator = false
        }
    }

    onStatusChanged: {
        console.debug("page status=" + status)
        switch (status) {
        case PageStatus.Deactivating: {
            console.debug("page status=deactivating")
            // moved here from panel open/close, seems to be less disruptive to
            // user experience
            if (_hasSource && mediaplayer.status === MediaPlayer.Buffered) {
                _grabFrame()
            }
            //pause()
        } break
        case PageStatus.Activating:
            console.debug("page status=activating")
            //resume()
            break
        }
    }

    function playPlaylist(playlistArg, saveScreenShot) {
        _savePlaybackProgress()

        if (playlistArg.isValid) {
            _playlist.copyFrom(playlistArg)
            _play(saveScreenShot)
        } else {
            console.debug("invalid playlist")
        }
    }

    function _play(saveScreenShot) {
        console.debug("playlist offset=" + _playlist.playbackOffset + " parts=" + _playlist.parts + " save screen shot=" + saveScreenShot)
        _reseekOnDurationChange = false
        _seekPositionMs = 0
        _currentUrlIndex = 0;
        controlPanel.open = false
        _grabFrameWhenReady = saveScreenShot
        _startSeek = false
        _seekFixup = 0
        _pauseCount = 0;
        _paused = false
        _streamDurationS = _computeStreamDuration()
        _streamBasePositionS = _computeStreamBasePosition()
        _volume = mediaplayer.volume
        if (_playlist.playbackOffset > 0) {
            console.debug("seek to " + _playlist.playbackOffset)
            _startSeek = true
            _seek(_playlist.playbackOffset * 1000)
        } else {
            mediaplayer.source = _playlist.url(_currentUrlIndex)
        }
    }

    function _savePlaybackProgress() {
        if (_playlist.isValid) {
            VodDataManager.setPlaybackOffset(_playlist.mediaKey, _streamPositionS)
            _playlist.invalidate()
        }
    }

    function pause() {
        _pauseCount += 1
        console.debug("pause count="+ _pauseCount)
        if (isPlaying) {
            console.debug("video player page pause playback")
//            mediaplayer.pause()

            _paused = true
            mediaplayer.pause()
        }

        if (mediaplayer.playbackState === MediaPlayer.PausedState) {
            _grabFrame()
        }
    }

    function resume() {
        _pauseCount -= 1
        console.debug("pause count="+ _pauseCount)
        if (_pauseCount === 0 && _paused) {
            console.debug("video player page resume playback")
            _paused = false
            mediaplayer.play()
            // toggle item visibility to to unfreeze video
            // after coming out of device lock
            _unfreezeVideo()
        }
    }

    function _unfreezeVideo() {
        videoOutputRectangle.visible = false
        videoOutputRectangle.visible = true
    }

    function openControlPanel() {
        _openCount += 1
        console.debug("open count="+ _openCount)
        if (!controlPanel.open) {
            console.debug("opening control pannel")
            controlPanel.open = true
        }
    }

    function closeControlPanel() {
        _openCount -= 1
        console.debug("open count="+ _openCount)
        if (_openCount === 0 && controlPanel.open) {
            console.debug("closing control panel")
            controlPanel.open = false
        }
    }

    function _stopPlayback() {
        pause()
    }

    function _toTime(n) {
        return Global.secondsToTimeString(n)
    }

    function _seek(positionMs) {
        _seekPositionMs = positionMs

        // figure out which url we need
        for (var i = 0; i < _playlist.parts; ++i) {
            var dS = _playlist.duration(i);
            if (dS <= 0) {
                mediaplayer.source = _playlist.url(i)
                _currentUrlIndex = i;
                _reseekOnDurationChange = true
                videoOutput.visible = false
                mediaplayer.volume = 0
                return
            }

            var dMs = dS * 1000

            if (positionMs <= dMs) {
                _seekToIndex(i, positionMs)
                videoOutput.visible = true
                mediaplayer.volume = _volume
                return
            }

            positionMs -= dMs;
        }

        _seekToIndex(_playlist.parts-1, (_playlist.duration(_playlist.parts-1) - 1) * 1000)
    }

    function _seekToIndex(index, positionMs) {
        _currentUrlIndex = index;
        _streamBasePositionS = _computeStreamBasePosition()
        mediaplayer.source = _playlist.url(index)
        _forceBusyIndicator = true
        console.debug("_forceBusyIndicator=true")
        busyTimer.restart()
        console.debug("media player seek to " + positionMs)
        mediaplayer.seek(positionMs)
    }

    function _grabFrame() {
        if (thumbnailFilePath) {
            ++_grabFrameControlPanelOpens
            openControlPanel()
            thumbnailOutput.grabToImage(function (a) {
                a.saveToFile(thumbnailFilePath)
                videoThumbnailCaptured(thumbnailFilePath)
                if (_grabFrameControlPanelOpens > 0) {
                    --_grabFrameControlPanelOpens
                    closeControlPanel()
                }
            })
        }

        if (coverFilePath) {
            ++_grabFrameControlPanelOpens
            openControlPanel()
            coverOutput.grabToImage(function (a) {
                a.saveToFile(coverFilePath)
                videoCoverCaptured(coverFilePath)
                if (_grabFrameControlPanelOpens > 0) {
                    --_grabFrameControlPanelOpens
                    closeControlPanel()
                }
            })
        }
    }

    function _preventBlanking(b) {
        displayBlanking.preventBlanking = b
    }

    function _trySetDuration() {
        if (_playlist &&
                _playlist.isValid &&
                mediaplayer.duration > 0 &&
                _currentUrlIndex >= 0 &&
                _currentUrlIndex < _playlist.parts &&
                _playlist.duration(_currentUrlIndex) <= 0) {
            _playlist.setDuration(_currentUrlIndex, Math.floor(mediaplayer.duration * 1e-3))
            _streamDurationS = _computeStreamDuration()
            _streamBasePositionS = _computeStreamBasePosition()
            return true
        }

        return false
    }

    function _computeStreamBasePosition() {
        var position = 0;
        if (_playlist && _playlist.isValid && _currentUrlIndex > 0) {
            for (var i = _currentUrlIndex - 1; i >= 0; --i) {
                position += _playlist.duration(i);
            }
        }

        return position
    }


    function _computeStreamDuration() {
        var duration = 0;
        if (_playlist && _playlist.isValid) {
            for (var i = 0; i < _playlist.parts; ++i) {
                var seconds = _playlist.duration(i);
                if (seconds <= 0) {
                    break;
                }

                duration += seconds;
            }
        }
        return duration;
    }
}

