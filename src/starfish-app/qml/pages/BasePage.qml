import QtQuick 2.0
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0
import ".."

Page {
    id: page

    property DockedPanel progressPanel: null
    property bool _hasScraper: !!vodDatabaseDownloader.scraper
    property bool _isScraping: {
        if (_hasScraper) {
            switch (vodDatabaseDownloader.scraper.status) {
            case VodScraper.Status_VodFetchingInProgress:
                return true
            }
        }

        return false
    }

    property bool _isDownloadingDatabase: {
        switch (vodDatabaseDownloader.status) {
        case VodDatabaseDownloader.Status_Downloading:
            return true
        }
        return false
    }

    DockedPanel {

        id: _progressPanel
        width: page.isPortrait ? (parent ? parent.width : 0) : Theme.itemSizeExtraLarge + Theme.paddingLarge
        height: page.isPortrait ? Theme.itemSizeExtraLarge + Theme.paddingLarge : (parent ? parent.height : 0)
        dock: page.isPortrait ? Dock.Bottom : Dock.Right
        open: _isScraping || _isDownloadingDatabase
//        open: false

//        onOpenChanged: {
//            console.debug("progress panel open=" + open)
//        }

        ProgressBar {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - buttonRow.width
            indeterminate: vodDatabaseDownloader.progressIndeterminate
            value: vodDatabaseDownloader.progress
            label: vodDatabaseDownloader.progressDescription
        }

        Row {
            id: buttonRow
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
//            spacing: cancel.width / 2

            IconButton {
                id: cancel
                icon.source: "image://theme/icon-m-dismiss"
//                enabled: {
//                    if (_isScraping) {
//                        return VodDatabaseDownloader.scraper.canCancelFetch
//                    }

//                    return false
//                }

                onClicked: {
                    console.debug("cancel")
                    vodDatabaseDownloader.cancel()
                }
            }

            IconButton {
                id: skip
                visible: _hasScraper && vodDatabaseDownloader.scraper.canSkip
                icon.source: "image://theme/icon-m-next"
                onClicked: {
                    console.debug("skip scrape")
                    vodDatabaseDownloader.skip()
                }
            }
        }
    }

    Component.onCompleted: {
        progressPanel = _progressPanel
    }

    onVisibleChildrenChanged: {
//        console.debug("visible children changed")
        _moveToTop()
    }

    function _moveToTop() {
        var maxZ = 0
        var vc = visibleChildren
//        console.debug("#" + vc.length + " children")
        for (var i = 0; i < vc.length; ++i) {
            var child = vc[i]
            if (child !== _progressPanel) {
                maxZ = Math.max(maxZ, child.z)
            }
        }
//        console.debug("max z " + maxZ)
        _progressPanel.z = maxZ + 1
    }
}

