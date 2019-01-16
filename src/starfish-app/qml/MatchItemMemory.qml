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
import org.duckdns.jgressmann 1.0

Item {
    property var _items: []

    Component.onCompleted: {
        VodDataManager.vodAvailable.connect(vodAvailable)
        VodDataManager.fetchingThumbnail.connect(fetchingThumbnail)
        VodDataManager.thumbnailAvailable.connect(thumbnailAvailable)
        VodDataManager.thumbnailDownloadFailed.connect(thumbnailDownloadFailed)
        VodDataManager.fetchingMetaData.connect(fetchingMetaData)
        VodDataManager.metaDataAvailable.connect(metaDataAvailable)
        VodDataManager.metaDataDownloadFailed.connect(metaDataDownloadFailed)
        VodDataManager.vodDownloadFailed.connect(vodDownloadFailed)
        VodDataManager.vodDownloadCanceled.connect(vodDownloadCanceled)
        VodDataManager.titleAvailable.connect(titleAvailable)
        VodDataManager.seenAvailable.connect(seenAvailable)
        VodDataManager.vodEndAvailable.connect(vodEndAvailable)
        VodDataManager.vodDownloadsChanged.connect(vodDownloadsChanged)
    }

    Component.onDestruction: {
//        console.debug("destroy match item memory")
        VodDataManager.vodAvailable.disconnect(vodAvailable)
        VodDataManager.fetchingThumbnail.disconnect(fetchingThumbnail)
        VodDataManager.thumbnailAvailable.disconnect(thumbnailAvailable)
        VodDataManager.thumbnailDownloadFailed.disconnect(thumbnailDownloadFailed)
        VodDataManager.fetchingMetaData.disconnect(fetchingMetaData)
        VodDataManager.metaDataAvailable.disconnect(metaDataAvailable)
        VodDataManager.metaDataDownloadFailed.disconnect(metaDataDownloadFailed)
        VodDataManager.vodDownloadFailed.disconnect(vodDownloadFailed)
        VodDataManager.vodDownloadCanceled.disconnect(vodDownloadCanceled)
        VodDataManager.titleAvailable.disconnect(titleAvailable)
        VodDataManager.seenAvailable.disconnect(seenAvailable)
        VodDataManager.vodEndAvailable.disconnect(vodEndAvailable)
        VodDataManager.vodDownloadsChanged.disconnect(vodDownloadsChanged)
    }
/* CONNECTIONS DO NOT WORK!!! */


    function addMatchItem(item) {
        _items[item.rowId] = item
    }

    function removeMatchItem(item) {
        delete _items[item.rowId]
    }

    function vodAvailable(rowid, filePath, progress, fileSize, width, height, formatId) {
        var item = _items[rowid]
        if (item) {
            item.vodAvailable(filePath, progress, fileSize, width, height, formatId)
        }
    }

    function fetchingThumbnail(rowid) {
        var item = _items[rowid]
        if (item) {
            item.fetchingThumbnail()
        }
    }

    function thumbnailAvailable(rowid, filePath) {
        var item = _items[rowid]
        if (item) {
            item.thumbnailAvailable(filePath)
        }
    }

    function thumbnailDownloadFailed(rowid, error, url) {
        var item = _items[rowid]
        if (item) {
            item.thumbnailDownloadFailed(error, url)
        }
    }

    function titleAvailable(rowid, title) {
        var item = _items[rowid]
        if (item) {
            item.titleAvailable(title)
        }
    }

    function seenAvailable(rowid, seen) {
        var item = _items[rowid]
        if (item) {
            item.seenAvailable(seen)
        }
    }

    function vodEndAvailable(rowid, offset) {
        var item = _items[rowid]
        if (item) {
            item.vodEndAvailable(offset)
        }
    }

    function fetchingMetaData(rowid) {
        var item = _items[rowid]
        if (item) {
            item.fetchingMetaData()
        }
    }

    function metaDataDownloadFailed(rowid, error) {
        var item = _items[rowid]
        if (item) {
            item.metaDataDownloadFailed(error)
        }
    }

    function metaDataAvailable(rowid, vod) {
        var item = _items[rowid]
        if (item) {
            item.metaDataAvailable(vod)
        }
    }

    function vodDownloadFailed(rowid, error) {
        var item = _items[rowid]
        if (item) {
            item.vodDownloadFailed(error)
        }
    }

    function vodDownloadCanceled(rowid) {
        var item = _items[rowid]
        if (item) {
            item.vodDownloadCanceled()
        }
    }

    function vodDownloadsChanged() {
        var downloads = VodDataManager.vodsBeingDownloaded()
        for (var i = 0; i < downloads.length; ++i) {
            var rowid = downloads[i]
            var item = _items[rowid]
            if (item) {
                item.vodDownloading()
            }
        }
    }

    function updateStartOffset(rowid) {
        var item = _items[rowid]
        if (item) {
            item.updateStartOffset()
        }
    }

    function cancelImplicitVodFileFetch(rowid) {
        var item = _items[rowid]
        if (item) {
            item.cancelImplicitVodFileFetch()
        }
    }
}
