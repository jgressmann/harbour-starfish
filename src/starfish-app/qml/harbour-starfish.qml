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
import Nemo.Configuration 1.0
import Nemo.DBus 2.0
import Nemo.Notifications 1.0
import org.duckdns.jgressmann 1.0
import "."


ApplicationWindow {
    id: window
//    initialPage: Component { StartPage {} }
//    initialPage: Qt.resolvedUrl("pages/SettingsPage.qml")
    initialPage: Qt.resolvedUrl("pages/StartPage.qml")
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations
    property int _vodsAdded: 0

    Sc2LinksDotComScraper {
        id: sc2LinksDotComScraper
    }

    Sc2CastsDotComScraper {
        id: sc2CastsDotComScraper
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
                errorNotification.body = errorNotification.previewBody = "Network down"
                errorNotification.publish()
                break
            case VodDatabaseDownloader.Error_DataInvalid:
                errorNotification.body = errorNotification.previewBody = "Data downloaded is invalid"
                errorNotification.publish()
                break
            case VodDatabaseDownloader.Error_DataDecompressionFailed:
                errorNotification.body = errorNotification.previewBody = "Data decompression failed"
                errorNotification.publish()
                break
            default:
                errorNotification.body = errorNotification.previewBody = "Yikes! An unknown error has occurred"
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
                VodDataManager.vodman.maxConcurrentMetaDataDownloads = value
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
            id: settingExternalMediaPlayer
            key: "/playback/use_external_player"
            defaultValue: false
        }

        ConfigurationValue {
            id: settingPlaybackRecentVideosToKeep
            key: "/playback/recent_videos_to_keep"
            defaultValue: 10
        }

        ConfigurationValue {
            id: debugApp
            key: "/debug"
            defaultValue: false
        }

        ConfigurationValue {
            id: settingLastUpdateTimestamp
            key: "/last_update_timestamp"
            defaultValue: 0
        }
    }

    RecentlyUsedModel {
        id: recentlyUsedVideos
        columnNames: ["video_id", "url", "offset", "thumbnail_path"]
        keyColumns: ["video_id", "url"]
        columnTypes: ["INTEGER DEFAULT -1", "TEXT", "INTEGER DEFAULT 0", "TEXT"]
        table: "recently_used_videos"
        count: settingPlaybackRecentVideosToKeep.value
        database: VodDataManager.database
        changeForcesReset: true

        onRowsAboutToBeRemoved: function (parent, from, to) {
            console.debug("removing rows from=" + from + " to=" + to)
            for (var i = from; i <= to; ++i) {
                var thumbNailFilePath = data(index(i, 3), 0)
                console.debug("row=" + i + " thumbnail path=" + thumbNailFilePath)
                if (thumbNailFilePath) {
                    App.unlink(thumbNailFilePath)
                }
            }
        }
    }

    Notification {
         id: errorNotification
         category: "x-nemo.transfer.error"
         summary: "Download failed"
         previewSummary: "Download failed"
         appName: App.displayName
         appIcon: "/usr/share/icons/hicolor/86x86/apps/harbour-starfish.png"
    }

    Notification {
        id: newVodNotification
        appName: App.displayName
        appIcon: "/usr/share/icons/hicolor/86x86/apps/harbour-starfish.png"
        icon: "icon-lock-application-update"
        remoteActions: [ {
             "name": "default",
             "displayName": "Show new VODs",
             "icon": "icon-lock-application-update",
             "service": "org.duckdns.jgressmann.starfish.app",
             "path": "/instance",
             "iface": "org.duckdns.jgressmann.starfish.app",
             "method": "showNewVods",
         } ]
    }

    Notification {
        id: deleteVodNotification
        appName: App.displayName
        appIcon: "/usr/share/icons/hicolor/86x86/apps/harbour-starfish.png"
        icon: "icon-lock-information"
    }

    DBusAdaptor {
        id: dbus
        bus: DBus.SessionBus
        service: 'org.duckdns.jgressmann.starfish.app'
        iface: 'org.duckdns.jgressmann.starfish.app'
        path: '/instance'

        function showNewVods() {
            var item = pageStack.currentPage
            if (!item || item.objectName !== "NewPage") {
                pageStack.push(Qt.resolvedUrl("pages/NewPage.qml"))
            }
        }
    }

    Component.onCompleted: {
        VodDataManager.vodsAdded.connect(_onVodsAdded)
        VodDataManager.busyChanged.connect(_busyChanged)
        VodDataManager.vodman.maxConcurrentMetaDataDownloads = settingNetworkMaxConcurrentMetaDataDownloads.value
        _setScraper()
        console.debug("last fetch=" + settingLastUpdateTimestamp.value)

        Global.playVideoHandler = function (updater, obj, url, offset) {
            recentlyUsedVideos.add(obj)
            if (settingExternalMediaPlayer.value && self.url.indexOf("http") !== 0) {
                Qt.openUrlExternally(url)
            } else {
                console.debug("offset=" + offset)
                var playerPage = pageStack.push(Qt.resolvedUrl("pages/VideoPlayerPage.qml"))
                playerPage.play(url, offset)
                updater.playerPage = playerPage
                updater.setKey(obj)
            }
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
    }

    function _busyChanged() {
        _notifyOfAddedVods();
    }

    function _notifyOfAddedVods() {
        switch (vodDatabaseDownloader.status) {
        case VodDatabaseDownloader.Status_Canceled:
        case VodDatabaseDownloader.Status_Finished:
            if (!VodDataManager.busy && _vodsAdded) {
                newVodNotification.body = newVodNotification.previewBody = _vodsAdded + " new VOD" + (_vodsAdded > 1 ? "s" : "")
                newVodNotification.publish()
            }
            break
        }
    }
}


