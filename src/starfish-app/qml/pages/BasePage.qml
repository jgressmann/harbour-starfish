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

    property bool wasActive: false

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

        Column {
            width: parent.width - buttonRow.width
            anchors.verticalCenter: parent.verticalCenter

            ProgressBar {
                width: parent.width
                indeterminate: vodDatabaseDownloader.progressIndeterminate
                value: vodDatabaseDownloader.progress
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * x
                text: vodDatabaseDownloader.progressDescription
                truncationMode: TruncationMode.Fade
                font.pixelSize: Theme.fontSizeSmall
                horizontalAlignment: _fadeText ? Text.AlignLeft : Text.AlignHCenter
            }
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

    onStatusChanged: {
        switch (status) {
        case PageStatus.Active:
            wasActive = true
            break
        }
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

