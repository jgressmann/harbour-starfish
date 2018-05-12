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
        VodDataManager.vodman.maxConcurrentMetaDataDownloads = settingNetworkMaxConcurrentMetaDataDownloads.value
        _setScraper()
        console.debug("last fetch=" + settingLastUpdateTimestamp.value)
    }

    function _setScraper() {
        console.debug("scraper=" + settingNetworkScraper.value)
        if (sc2CastsDotComScraper.id === settingNetworkScraper.value) {
            vodDatabaseDownloader.scraper = sc2CastsDotComScraper
        } else if (sc2LinksDotComScraper.id === settingNetworkScraper.value) {
            vodDatabaseDownloader.scraper = sc2LinksDotComScraper
        }
    }
}


