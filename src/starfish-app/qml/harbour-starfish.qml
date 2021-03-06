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
import Nemo.DBus 2.0
import Nemo.Notifications 1.0
import Vodman 2.1
import org.duckdns.jgressmann 1.0
import "pages"
import "."



ApplicationWindow {
    id: window

    //    initialPage: Qt.resolvedUrl("pages/SettingsPage.qml")
    initialPage: YTDLDownloader.downloadStatus === YTDLDownloader.StatusReady
                 ? Qt.resolvedUrl("pages/StartPage.qml")
                 : Qt.resolvedUrl("pages/YTDLPage.qml")
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations
    property int _vodsAdded: 0
    readonly property bool canFetchVods:
        YTDLDownloader.downloadStatus === YTDLDownloader.StatusReady &&
        !VodDataManager.busy &&
        App.isOnline &&
        vodDatabaseDownloader.status !== VodDatabaseDownloader.Status_Downloading
    property bool _hasCheckedForYtdlUpdate: false
    property VideoPlayerPage _videoPlayer
    readonly property VideoPlayerPage videoPlayer: _videoPlayer

    onVisibleChanged: {
        console.debug("window visible=" + visible)
        if (videoPlayer) {
            videoPlayer.enableFrameGrabbing = visible
        }
    }

    Sc2LinksDotComScraper {
        id: sc2LinksDotComScraper
        stateFilePath: App.dataDir + "/sc2links.com.state"
    }

    Sc2CastsDotComScraper {
        id: sc2CastsDotComScraper
        stateFilePath: App.dataDir + "/sc2casts.com.state"
    }

    VodDatabaseDownloader {
        id: vodDatabaseDownloader
        dataManager: VodDataManager


        onStatusChanged: {
            switch (status) {
            case VodDatabaseDownloader.Status_Downloading:
                _vodsAdded = 0
                break
            case VodDatabaseDownloader.Status_Canceled:
            case VodDatabaseDownloader.Status_Finished:
                settingLastUpdateTimestamp.value = Global.secondsSinceTheEpoch()
                break
            }

            _notifyOfAddedVods();
        }

        onErrorChanged: {
            switch (error) {
            case VodDatabaseDownloader.Error_None:
                break
            case VodDatabaseDownloader.Error_NetworkFailure:
                //% "Network down"
                errorNotification.body = errorNotification.previewBody = qsTrId("sf-database-error-network-failure")
                errorNotification.publish()
                break
            case VodDatabaseDownloader.Error_DataInvalid:
                //% "Data downloaded is invalid"
                errorNotification.body = errorNotification.previewBody = qsTrId("sf-database-error-invalid-data")
                errorNotification.publish()
                break
            case VodDatabaseDownloader.Error_DataDecompressionFailed:
                //% "Data decompression failed"
                errorNotification.body = errorNotification.previewBody = qsTrId("sf-database-error-decompression-failed")
                errorNotification.publish()
                break
            default:
                //% "Yikes! An unknown error has occurred"
                errorNotification.body = errorNotification.previewBody = qsTrId("sf-database-error-unknown")
                errorNotification.publish()
                break
            }
        }
    }



    ConfigValue {
        id: settingBroadbandDefaultFormat
        defaultValue: VM.VM_Largest
        key: "/format/broadband"
    }

    ConfigValue {
        id: settingMobileDefaultFormat
        defaultValue: VM.VM_Smallest
        key: "/format/mobile"
    }

    ConfigValue {
        id: settingBearerMode
        defaultValue: Global.bearerModeAutoDetect
        key: "/bearer/mode"
    }

    ConfigValue {
        id: settingNetworkMaxConcurrentMetaDataDownloads
        key: "/network/max_concurrent_meta_data_downloads"
        defaultValue: 4

        onValueChanged: {
            VodDataManager.maxConcurrentMetaDataDownloads = value
        }
    }

    ConfigValue {
        id: settingNetworkMaxConcurrentVodFileDownloads
        key: "/network/max_concurrent_vod_file_downloads"
        defaultValue: 0

        onValueChanged: {
            VodDataManager.maxConcurrentVodFileDownloads = value
        }
    }

    ConfigValue {
        id: settingNetworkScraper
        key: "/network/scraper"
        defaultValue: sc2CastsDotComScraper.id

        onValueChanged: _setScraper()
    }

    ConfigValue {
        id: settingNetworkContinueDownload
        key: "/network/continue_download"
        defaultValue: true
    }

    ConfigValue {
        id: settingNetworkAutoUpdate
        key: "/network/auto_update"
        defaultValue: true
    }

    ConfigValue {
        id: settingNetworkAutoUpdateIntervalM
        key: "/network/auto_update_interval_m"
        defaultValue: 60
    }

    ConfigValue {
        id: settingPlaybackRecentVideosToKeep
        key: "/playback/recent_videos_to_keep"
        defaultValue: 10
        onValueChanged: VodDataManager.recentlyWatched.count = value
    }

    ConfigValue {
        id: settingPlaybackPauseInCoverMode
        key: "/playback/pause_in_cover_mode"
        defaultValue: false
    }

    ConfigValue {
        id: settingPlaybackPauseOnDeviceLock
        key: "/playback/pause_on_device_lock"
        defaultValue: true
    }

    ConfigValue {
        id: debugApp
        key: "/debug"
        defaultValue: false
        onValueChanged: _setMode()
    }

    ConfigValue {
        id: settingLastUpdateTimestamp
        key: "/last_update_timestamp"
        defaultValue: 0
    }

    ConfigValue {
        id: settingNumberOfUpdates
        key: "/stats/no_updates"
        defaultValue: 0
    }

    ConfigValue {
        id: settingNewWindowDays
        key: "/new/window_days"
        defaultValue: 30
    }

    ConfigValue {
        id: settingNewRemoveSeen
        key: "/new/remove_seen"
        defaultValue: true
    }

    Notification {
         id: errorNotification
         category: "x-nemo.transfer.error"
         //% "Download failed"
         summary: qsTrId("sf-vod-download-failed-notification-summary")
         previewSummary: qsTrId("sf-vod-download-failed-notification-summary")
         appName: App.displayName
         appIcon: "/usr/share/icons/hicolor/86x86/apps/harbour-starfish.png"
//         icon: "icon-lock-transfer"
         icon: appIcon
         isTransient: true
    }

    Notification {
         id: noSpaceLeftOnDeviceNotification
         category: "x-nemo.transfer.error"
         //% "No space left on device"
         summary: qsTrId("sf-no-space-left-on-device-notification-summary")
         previewSummary: qsTrId("sf-no-space-left-on-device-notification-summary")
         appName: App.displayName
         appIcon: "/usr/share/icons/hicolor/86x86/apps/harbour-starfish.png"
         icon: appIcon
         isTransient: true
    }

    Notification {
        id: newVodNotification
        appName: App.displayName
        previewSummary: summary
        appIcon: "/usr/share/icons/hicolor/86x86/apps/harbour-starfish.png"
        icon: appIcon

        remoteActions: [ {
             "name": "default",
             "displayName": "Show new VODs",
//             "icon": "icon-lock-application-update",
             "service": "org.duckdns.jgressmann.starfish.app",
             "path": "/instance",
             "iface": "org.duckdns.jgressmann.starfish.app",
             "method": "showNewVods",
             "arguments": []
         } ]
    }

    Notification {
        id: deleteSeenVodFilesNotification
        appName: App.displayName
        previewSummary: summary
        appIcon: "/usr/share/icons/hicolor/86x86/apps/harbour-starfish.png"
        icon: appIcon
        isTransient: true
    }

    Notification {
        id: deleteVodNotification
        appName: App.displayName
        previewSummary: summary
        appIcon: "/usr/share/icons/hicolor/86x86/apps/harbour-starfish.png"
        icon: appIcon
        isTransient: true
    }

    Notification {
        id: undeleteVodNotification
        appName: App.displayName
        previewSummary: summary
        appIcon: "/usr/share/icons/hicolor/86x86/apps/harbour-starfish.png"
        icon: appIcon
        isTransient: true
    }

    Notification {
        id: updateNotification
        appName: App.displayName
        appIcon: "/usr/share/icons/hicolor/86x86/apps/harbour-starfish.png"
        icon: appIcon
        //% "youtube-dl update available"
        summary: qsTrId("sf-nofification-download-ytdl-update-available-summary")
        previewSummary: summary
        //% "youtube-dl version %1 available"
        body: qsTrId("sf-nofification-ytdl-update-available-body").arg(YTDLDownloader.updateVersion)
        previewBody: body
        remoteActions: [ {
             "name": "default",
             //% "Update youtube-dl"
             "displayName": qsTrId("sf-nofification-ytdl-update-available-action"),
             "service": "org.duckdns.jgressmann.starfish.app",
             "path": "/instance",
             "iface": "org.duckdns.jgressmann.starfish.app",
             "method": "updateYtdl",
         } ]
    }

    DBusAdaptor {
        bus: DBus.SessionBus
        service: 'org.duckdns.jgressmann.starfish.app'
        iface: 'org.duckdns.jgressmann.starfish.app'
        path: '/instance'

        function showNewVods() {
            var item = pageStack.currentPage
            if (!item || item.objectName !== "NewPage") {
                Global.openNewVodPage(pageStack)
            }

            window.activate()
        }

        function updateYtdl() {
            if (false) {
                errorNotification.body = errorNotification.previewBody =
                        //% "%1 is busy. Try again later."
                        qsTrId("sf-notification-busy").arg(App.displayName)
                errorNotification.publish()
            } else {
                YTDLDownloader.download()
            }
        }
    }

    DBusInterface {
        bus: DBus.SystemBus
        service: 'com.nokia.mce'
        iface: 'com.nokia.mce.signal'
        path: '/com/nokia/mce/signal'

        signalsEnabled: true

        function tklock_mode_ind(arg) {
            console.debug("tklock_mode_ind=" + arg)
            switch (arg) {
            case "locked":
                if (settingPlaybackPauseOnDeviceLock.value && videoPlayer) {
                    videoPlayer.pause()
                }

//                Global.addPlaybackPause()
                break;
            case "unlocked":
                if (settingPlaybackPauseOnDeviceLock.value && videoPlayer) {
                    videoPlayer.resume()
                }
//                Global.removePlaybackPause()
                break;
            }
        }
    }

//    DBusInterface {
//        bus: DBus.SystemBus
//        service: 'com.nokia.mce'
//        iface: 'com.nokia.mce.request'
//        path: '/com/nokia/mce/request'

//        Component.onCompleted: {
//            typedCall(
//                "get_tklock_mode",
//                undefined,
//                function (result) {
//                    console.debug("initial value of tklock_mode=" + result)
//                    if ("locked" === result) {
//                        if (settingPlaybackPauseOnDeviceLock.value &&  && window.videoPlayer) {
//                            window.videoPlayer.pause()
//                        }
////                        Global.addPlaybackPause()
//                    }
//                })
//        }
//    }

    Timer {
        repeat: true
//        interval: {
//            var diffS = Global.secondsSinceTheEpoch() - Global.lastUpdateTimeStamp
//            if (diffS >= 60000 * settingNetworkAutoUpdateIntervalM.value) {
//                return 1
//            }
//            return (60 * settingNetworkAutoUpdateIntervalM.value - diffS) * 1000
//        }
        interval: 60000 * settingNetworkAutoUpdateIntervalM.value
        running: settingNetworkAutoUpdate.value && window.canFetchVods
        onTriggered: {
            console.debug("Auto update timer expired")
            fetchNewVods()
        }

//        onIntervalChanged: {
//            console.debug("interval=" + interval)
//            if (running) {
//                console.debug("restarting timer")
//                restart()
//            }
//        }

        onTriggeredOnStartChanged: {
            console.debug("auto update timer triggered on start=" + triggeredOnStart)
        }

        triggeredOnStart: running * 0 + // tie into running changed
            Global.secondsSinceTheEpoch() - settingLastUpdateTimestamp.value >= interval * 1e-3

        onRunningChanged: {
            console.debug("auto update timer running=" + running)
        }
    }

    Connections {
        target: YTDLDownloader
        onYtdlPathChanged: _setYtdlPath()
        onDownloadStatusChanged: {
            _setYtdlPath()
            _switchContentPage()
            _checkForYtdlUpate()
        }
        onIsOnlineChanged: _checkForYtdlUpate()
    }

    Connections {
        target: pageStack
        onCurrentPageChanged: _switchContentPage()
        onBusyChanged: _switchContentPage()
    }

    Component.onCompleted: {
        VodDataManager.recentlyWatched.count = settingPlaybackRecentVideosToKeep.value
        VodDataManager.vodsAdded.connect(_onVodsAdded)
        VodDataManager.busyChanged.connect(_busyChanged)
        VodDataManager.errorNotification.connect(_onErrorNotification)
        YTDLDownloader.isUpdateAvailableChanged.connect(_ytdlUpdateAvailableChanged)
        VodDataManager.maxConcurrentMetaDataDownloads = settingNetworkMaxConcurrentMetaDataDownloads.value
        VodDataManager.maxConcurrentVodFileDownloads = settingNetworkMaxConcurrentVodFileDownloads.value
        _setMode()
        _setYtdlPath()
        _checkForYtdlUpate()
        _setScraper()
        console.debug("last fetch=" + settingLastUpdateTimestamp.value)

        Global.playVideoHandler = function (updater, key, playlist, seen) {
            if (playlist && playlist.isValid) {
                console.debug("playlist=" + playlist)
                VodDataManager.recentlyWatched.add(key, seen)
                VodDataManager.recentlyWatched.setOffset(key, playlist.playbackOffset)
                console.debug("offset=" + playlist.playbackOffset)
                var playerPage = pageStack.push(Qt.resolvedUrl("pages/VideoPlayerPage.qml"))
                playerPage.playPlaylist(playlist)
                updater.playerPage = playerPage
                updater.setKey(key)
            }
        }

        Global.deleteVodNotification = deleteVodNotification
        Global.undeleteVodNotification = undeleteVodNotification
        Global.deleteSeenVodFilesNotification = deleteSeenVodFilesNotification
        Global.getNewVodsContstraints = function () {
            return "hidden=0 AND match_date>=date('now', '-" + settingNewWindowDays.value.toFixed(0) + " days')" +
                                    (settingNewRemoveSeen.value ? " and seen=0" : "")
        }
    }

    Component.onDestruction: {
        YTDLDownloader.isUpdateAvailableChanged.disconnect(_ytdlUpdateAvailableChanged)
    }

    function _setScraper() {
        console.debug("scraper=" + settingNetworkScraper.value)
        if (sc2CastsDotComScraper.id === settingNetworkScraper.value) {
            vodDatabaseDownloader.scraper = sc2CastsDotComScraper
        } else if (sc2LinksDotComScraper.id === settingNetworkScraper.value) {
            vodDatabaseDownloader.scraper = sc2LinksDotComScraper
        }
    }

    function _onVodsAdded(count) {
        console.debug("vods added " + count)
        _vodsAdded += count
        _notifyOfAddedVods();
    }

    function _busyChanged() {
        _notifyOfAddedVods();
    }

    function _notifyOfAddedVods() {
        switch (vodDatabaseDownloader.status) {
        case VodDatabaseDownloader.Status_Canceled:
        case VodDatabaseDownloader.Status_Finished:
            if (!VodDataManager.busy && _vodsAdded) {
                newVodNotification.itemCount = 1
                //% "%1 VODs added"
                newVodNotification.summary = qsTrId("sf-vods-added-notification-summary", _vodsAdded).arg(_vodsAdded)
                newVodNotification.publish()
                _vodsAdded = 0
            }
            break
        }
    }

    function fetchNewVods() {
        console.debug("fetch new vods")
        settingNumberOfUpdates.value = settingNumberOfUpdates.value + 1
        settingLastUpdateTimestamp.value = Global.secondsSinceTheEpoch() // in case debugging ends
        vodDatabaseDownloader.downloadNew()
    }

    function _setYtdlPath() {
        if (YTDLDownloader.downloadStatus === YTDLDownloader.StatusReady) {
            VodDataManager.setYtdlPath(YTDLDownloader.ytdlPath)
        } else {
            VodDataManager.setYtdlPath("")
        }
    }

    function _setMode() {
        YTDLDownloader.mode = debugApp.value ? YTDLDownloader.ModeTest : YTDLDownloader.ModeRelease
    }

    function _switchContentPage() {
        if (pageStack.currentPage && !pageStack.busy) {
            if (YTDLDownloader.downloadStatus === YTDLDownloader.StatusReady) {
                if (pageStack.currentPage.isYTDLPage) {
                    pageStack.replace(Qt.resolvedUrl("pages/StartPage.qml"))
                }
            } else {
                if (!pageStack.currentPage.isYTDLPage) {
                    pageStack.replace(Qt.resolvedUrl("pages/YTDLPage.qml"))
                }
            }
        }
    }

    function _ytdlUpdateAvailableChanged() {
        if (YTDLDownloader.isUpdateAvailable) {
            updateNotification.publish()
        }
    }

    function _checkForYtdlUpate() {
        if (!_hasCheckedForYtdlUpdate &&
            YTDLDownloader.isOnline &&
            YTDLDownloader.downloadStatus === YTDLDownloader.StatusReady &&
            YTDLDownloader.updateStatus === YTDLDownloader.StatusUnavailable) {
            _hasCheckedForYtdlUpdate = true
            YTDLDownloader.checkForUpdate()
        }
    }

    function playPlaylist(playlist, seen) {
        if (playlist && playlist.isValid) {
            console.debug("playlist parts=" + playlist.parts + " start=" + playlist.startOffset + " end=" + playlist.endOffset + " playback=" + playlist.playbackOffset)
            VodDataManager.recentlyWatched.add(playlist.mediaKey, seen)

            if (!_videoPlayer) {
                _videoPlayer = pageStack.push(Qt.resolvedUrl("pages/VideoPlayerPage.qml"))
                _videoPlayer.bye.connect(function () {
                    _videoPlayer = null
                })

                _videoPlayer.coverFilePath = Global.videoCoverPath
            }

            if (App.isUrlKey(playlist.mediaKey)) {
                _videoPlayer.thumbnailFilePath = VodDataManager.recentlyWatched.getThumbnailPath(playlist.mediaKey)
                if (!_videoPlayer.thumbnailFilePath) {
                    _videoPlayer.thumbnailFilePath = App.makeTemporaryFile(VodDataManager.thumbnailDirectory + "XXXXXX.png")
                    if (_videoPlayer.thumbnailFilePath) {
                        console.debug("created thumbnail file path " + _videoPlayer.thumbnailFilePath + " for use with media key " + playlist.mediaKey)
                        VodDataManager.recentlyWatched.setThumbnailPath(playlist.mediaKey, _videoPlayer.thumbnailFilePath)
                    }
                }
            } else {
                _videoPlayer.thumbnailFilePath = ""
            }

            _videoPlayer.playPlaylist(playlist, !!_videoPlayer.thumbnailFilePath)
        }
    }

    function _onErrorNotification(error) {
        switch (error) {
        case VodDataManager.Error_NoSpaceLeftOnDevice:
            noSpaceLeftOnDeviceNotification.publish()
            break
        }
    }
}


