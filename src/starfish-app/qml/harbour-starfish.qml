import QtQuick 2.0
import Sailfish.Silica 1.0
import Nemo.Configuration 1.0
import org.duckdns.jgressmann 1.0
import "."


ApplicationWindow {
    id: window
//    initialPage: Component { StartPage {} }
//    initialPage: Qt.resolvedUrl("pages/SettingsPage.qml")
    initialPage: Qt.resolvedUrl("pages/StartPage.qml")
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations

    Sc2LinksDotComScraper {
        id: sc2LinksDotComScraper
    }

    Sc2CastsDotComScraper {
        id: sc2CastsDotComScraper
    }

    VodDatabaseDownloader {
        id: vodDatabaseDownloader
        dataManager: VodDataManager
        scraper: sc2CastsDotComScraper

        onStatusChanged: {
            switch (status) {
            case VodDatabaseDownloader.Status_Canceled:
            case VodDatabaseDownloader.Status_Finished:
                settingLastUpdateTimestamp.value = Global.secondsSinceTheEpoch()
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
            id: settingCacheMaxSize
            key: "/cache/max_size"
            defaultValue: 10.0
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
            id: settingExternalMediaPlayer
            key: "/playback/use_external_player"
            defaultValue: false
        }

        ConfigurationValue {
            id: settingPlaybackOffset
            key: "/playback/offset"
            defaultValue: -1
        }

        ConfigurationValue {
            id: settingPlaybackRowId
            key: "/playback/rowid"
            defaultValue: -1
        }

        ConfigurationValue {
            id: settingPlaybackUrl
            key: "/playback/url"
            defaultValue: ""
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

    Component.onCompleted: {
        VodDataManager.vodCacheLimit = settingCacheMaxSize.value
        VodDataManager.vodman.maxConcurrentMetaDataDownloads = settingNetworkMaxConcurrentMetaDataDownloads.value
        console.debug("last fetch=" + settingLastUpdateTimestamp.value)
    }
}


