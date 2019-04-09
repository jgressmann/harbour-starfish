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
import Nemo.Configuration 1.0
import Nemo.DBus 2.0
import Nemo.Notifications 1.0
import org.duckdns.jgressmann 1.0
import "."


ApplicationWindow {
    id: window

    //    initialPage: Qt.resolvedUrl("pages/SettingsPage.qml")
    initialPage: YTDLDownloader.status === YTDLDownloader.StatusReady
                 ? Qt.resolvedUrl("pages/StartPage.qml")
                 : Qt.resolvedUrl("pages/YTDLPage.qml")
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations
    property int _vodsAdded: 0
    readonly property bool canFetchVods:
        YTDLDownloader.status === YTDLDownloader.StatusReady &&
        !VodDataManager.busy &&
        App.isOnline &&
        vodDatabaseDownloader.status !== VodDatabaseDownloader.Status_Downloading

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
                errorNotification.body = errorNotification.previewBody = qsTrId("database-error-network-failure")
                errorNotification.publish()
                break
            case VodDatabaseDownloader.Error_DataInvalid:
                //% "Data downloaded is invalid"
                errorNotification.body = errorNotification.previewBody = qsTrId("database-error-invalid-data")
                errorNotification.publish()
                break
            case VodDatabaseDownloader.Error_DataDecompressionFailed:
                //% "Data decompression failed"
                errorNotification.body = errorNotification.previewBody = qsTrId("database-error-decompression-failed")
                errorNotification.publish()
                break
            default:
                //% "Yikes! An unknown error has occurred"
                errorNotification.body = errorNotification.previewBody = qsTrId("database-error-unknown")
                errorNotification.publish()
                break
            }
        }
    }

    ConfigurationGroup {
        id: settings

        ConfigurationValue {
            id: settingBroadbandDefaultFormat
            defaultValue: VM.VM_Largest
            key: "/format/broadband"
        }

        ConfigurationValue {
            id: settingMobileDefaultFormat
            defaultValue: VM.VM_Smallest
            key: "/format/mobile"
        }

        ConfigurationValue {
            id: settingBearerMode
            defaultValue: Global.bearerModeAutoDetect
            key: "/bearer/mode"
        }

        ConfigurationValue {
            id: settingNetworkMaxConcurrentMetaDataDownloads
            key: "/network/max_concurrent_meta_data_downloads"
            defaultValue: 4

            onValueChanged: {
                VodDataManager.maxConcurrentMetaDataDownloads = value
            }
        }

        ConfigurationValue {
            id: settingNetworkScraper
            key: "/network/scraper"
            defaultValue: sc2CastsDotComScraper.id

            onValueChanged: _setScraper()
        }

        ConfigurationValue {
            id: settingNetworkContinueDownload
            key: "/network/continue_download"
            defaultValue: true
        }

        ConfigurationValue {
            id: settingNetworkAutoUpdate
            key: "/network/auto_update"
            defaultValue: true
        }

        ConfigurationValue {
            id: settingNetworkAutoUpdateIntervalM
            key: "/network/auto_update_interval_m"
            defaultValue: 60
        }

        ConfigurationValue {
            id: settingExternalMediaPlayer
            key: "/playback/use_external_player"
            defaultValue: false
        }

        ConfigurationValue {
            id: settingPlaybackRecentVideosToKeep
            key: "/playback/recent_videos_to_keep"
            defaultValue: 10
            onValueChanged: VodDataManager.recentlyWatched.count = value
        }

        ConfigurationValue {
            id: settingPlaybackPauseInCoverMode
            key: "/playback/pause_in_cover_mode"
            defaultValue: false
        }

        ConfigurationValue {
            id: settingPlaybackPauseOnDeviceLock
            key: "/playback/pause_on_device_lock"
            defaultValue: true
        }

        ConfigurationValue {
            id: debugApp
            key: "/debug"
            defaultValue: false
            onValueChanged: _setMode()
        }

        ConfigurationValue {
            id: settingLastUpdateTimestamp
            key: "/last_update_timestamp"
            defaultValue: 0
        }

        ConfigurationValue {
            id: settingNumberOfUpdates
            key: "/stats/no_updates"
            defaultValue: 0
        }

        ConfigurationValue {
            id: settingNewWindowDays
            key: "/new/window_days"
            defaultValue: 30
        }

        ConfigurationValue {
            id: settingNewRemoveSeen
            key: "/new/remove_seen"
            defaultValue: true
        }
    }

    Notification {
         id: errorNotification
         category: "x-nemo.transfer.error"
         //% "Download failed"
         summary: qsTrId("vod-download-failed-notification-summary")
         previewSummary: qsTrId("vod-download-failed-notification-summary")
         appName: App.displayName
         appIcon: "/usr/share/icons/hicolor/86x86/apps/harbour-starfish.png"
//         icon: "icon-lock-transfer"
         icon: appIcon
    }

    Notification {
        id: newVodNotification
        appName: App.displayName
        appIcon: "/usr/share/icons/hicolor/86x86/apps/harbour-starfish.png"
        //% "VODs added"
        summary: qsTrId("new-vods-notification-summary")
//        icon: "icon-lock-application-update"
        icon: appIcon
//        icon: "icon-lock-information"
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
        id: deleteVodNotification
        appName: App.displayName
        //% "Seen VODs deleted"
        summary: qsTrId("seen-vods-deleted-notification-summary")
        appIcon: "/usr/share/icons/hicolor/86x86/apps/harbour-starfish.png"
//        icon: "icon-lock-information"
        icon: appIcon
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
                if (settingPlaybackPauseOnDeviceLock.value && Global.videoPlayerPage) {
                    Global.videoPlayerPage.pause()
                }

//                Global.addPlaybackPause()
                break;
            case "unlocked":
                if (settingPlaybackPauseOnDeviceLock.value && Global.videoPlayerPage) {
                    Global.videoPlayerPage.resume()
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
//                        if (settingPlaybackPauseOnDeviceLock.value &&  && Global.videoPlayerPage) {
//                            Global.videoPlayerPage.pause()
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
        onStatusChanged: {
            _setYtdlPath()
            _switchContentPage()
        }
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
        VodDataManager.maxConcurrentMetaDataDownloads = settingNetworkMaxConcurrentMetaDataDownloads.value
        _setMode()
        _setYtdlPath()
        _setScraper()
        console.debug("last fetch=" + settingLastUpdateTimestamp.value)

        Global.playVideoHandler = function (updater, key, url, offset, seen) {
            VodDataManager.recentlyWatched.add(key, seen)
            VodDataManager.recentlyWatched.setOffset(key, offset)
            if (settingExternalMediaPlayer.value && self.url.indexOf("http") !== 0) {
                Qt.openUrlExternally(url)
            } else {
                console.debug("offset=" + offset)
                var playerPage = pageStack.push(Qt.resolvedUrl("pages/VideoPlayerPage.qml"))
                playerPage.play(Global.completeFileUrl(url), offset)
                updater.playerPage = playerPage
                updater.setKey(key)
            }
        }

        Global.deleteVodNotification = deleteVodNotification
        Global.getNewVodsContstraints = function () {
            return "match_date>=date('now', '-" + settingNewWindowDays.value.toFixed(0) + " days')" +
                                    (settingNewRemoveSeen.value ? " and seen=0" : "")
        }


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
                newVodNotification.body = newVodNotification.previewBody = qsTrId("vods-added-notification-body", _vodsAdded).arg(_vodsAdded)
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
        if (YTDLDownloader.status === YTDLDownloader.StatusReady) {
            VodDataManager.setYtdlPath(YTDLDownloader.ytdlPath)
        } else {
            VodDataManager.setYtdlPath("")
        }
    }

    function _setMode() {
        YTDLDownloader.mode = debugApp.value ? YTDLDownloader.ModeTest : YTDLDownloader.ModeRelease
    }

    function _switchContentPage() {
        if (!pageStack.busy) {
            switch (YTDLDownloader.status) {
            case YTDLDownloader.StatusReady:
                if (pageStack.depth === 1 && pageStack.currentPage.isYTDLPage) {
                    pageStack.replace(Qt.resolvedUrl("pages/StartPage.qml"))
                }
                break;
            case YTDLDownloader.StatusUnavailable:
                if (pageStack.depth > 1) {
                    // sucks, after the next line will enter again to end up here once more
                    pageStack.pop(null, PageStackAction.Immediate)
//                        pageStack.completeAnimation()
                }
                if (!pageStack.currentPage.isYTDLPage) {
                    pageStack.replace(Qt.resolvedUrl("pages/YTDLPage.qml"), {backNavigation: false})
                }
                break;
            }

        }
    }
}


