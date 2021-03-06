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

pragma Singleton
import QtQuick 2.0
import Sailfish.Silica 1.0
import Nemo.Notifications 1.0
import org.duckdns.jgressmann 1.0

Item { // Components can't declare functions
    readonly property var filterKeys: ["game", "year", "event_name", "season"]
    readonly property int gridItemsPerPage: 8
    readonly property int bearerModeAutoDetect: 0
    readonly property int bearerModeBroadband: 1
    readonly property int bearerModeMobile: 2
    readonly property int itemHeight: Theme.itemSizeLarge
    readonly property int labelFontSize: Theme.fontSizeLarge
    readonly property string videoFramePath: App.dataDir + "/video.png"
    readonly property string videoThumbnailPath: App.dataDir + "/thumbnail.png"
    readonly property string videoCoverPath: App.dataDir + "/cover.png"
    readonly property real pathTraceOpacity: 0.25
    readonly property string defaultTable: "url_share_vods"
    readonly property string propTitle: "title"
    readonly property string propTable: "table"
    readonly property string propWhere: "where"
    readonly property string propKey: "key"
    readonly property string propBreadcrumbs: "breadcrumbs"
    readonly property string propHiddenFlags: "hiddenFlags"
    property var playVideoHandler
    property var videoPlayerPage
    property Notification deleteVodNotification
    property Notification undeleteVodNotification
    property Notification deleteSeenVodFilesNotification
    property var getNewVodsContstraints


    readonly property int matchItemHeaderModeDefault: 0
    readonly property int matchItemHeaderModeMatchName: 1
    readonly property int matchItemHeaderModeNone: 2

    readonly property int matchItemBodyModeDefault: 0
    readonly property int matchItemBodyModeMatchName: 1
    readonly property int matchItemBodyModeNone: 2

    function values(sql) {
//        console.debug(sql)

        _model.select = sql;

        var result = []
        var rows = _model.rowCount()
//        console.debug(sql + ": " + rows + " rows")
        for (var i = 0; i < rows; ++i) {
            var cell = _model.data(_model.index(i, 0), 0)
            result.push(cell)
        }

        return result
    }

    function rowCount(sqlOrDict) {
        var sql = ""
        if (typeof(sqlOrDict) === "string") {
            sql = sqlOrDict
        } else {
            var where = whereClause(sqlOrDict)
            sql = "select distinct " + filterKeys.join(",") + " from vods" + where
        }

        sql = "select count(*) as count from (" + sql  + ")"
//        console.debug(sql)


        _model.select = sql;
        if (_model.rowCount() !== 1) {
            var error = _model.lastError
            console.debug("sql error: " + error)
            return
        }

        var result = _model.data(_model.index(0, 0), 0)
//        console.debug("sql result: " + result)
        return result
    }

    function clone(obj) {
        if (null === obj || "object" !== typeof obj) return obj;
        var copy = obj.constructor();
        for (var attr in obj) {
            if (obj.hasOwnProperty(attr)) copy[attr] = clone(obj[attr]);
        }
        return copy;
    }

    function merge(dict1, dict2) {
        var result = {}
        for (var key in dict1) {
            result[key] = dict1[key]
        }

        for (var key in dict2) {
            result[key] = dict2[key]
        }

        return result
    }

    function showIcon(key) {
        return key !== "year" && key !== "season"
    }

    function remainingKeys(where) {
        var result = []

        for (var i = 0; i < filterKeys.length; ++i) {
            var key = filterKeys[i]
            var index = where.indexOf(key)
            if (index >= 0) {
                continue
            }

            result.push(key)
        }

        return result
    }


    function hasIcon(key) {
        return key in {
            "game": 1,
            "series": 1,
        }
    }

    function sortKey(key) {
        return {
            "year": -1,
            "season": -1,
        }[key] || 1
    }

    function performNext(props) {
        var _table = props[propTable]
        var _where = props[propWhere]
        var rem = remainingKeys(_where)

        var breadCrumbs = props[propBreadcrumbs]

        // we need to figure out if there is only as single value for any of the subkeys
        for (var change = true; change; ) {
            change = false
            var newRem = []

            if (rem.length > 0) {
                var xsql = "select\n";
                for (var i = 0; i < rem.length; ++i) {
                    if (i > 0) {
                        xsql += ",\n"
                    }
                    xsql += "count(distinct " + rem[i] + ")," + rem[i]
                }
                xsql += " from " + _table + _where + " limit 1";

//                console.debug(xsql);

                // do select
                _model.select = xsql

                for (var i = 0; i < rem.length; ++i) {
                    var count = _model.data(_model.index(0, i*2), 0)
                    var value = _model.data(_model.index(0, i*2+1), 0)
                    console.debug("count=" + count + " value=" + value)
                    if (count === 1) {
                        var newFilter = rem[i] + "='" + value + "'"
                        if (_where.length > 0) {
                            _where += " and " + newFilter
                        } else {
                            _where = " where " + newFilter
                        }

                        change = true

                        breadCrumbs.push(value)
                    } else if (count > 1) {
                        newRem.push(rem[i])
                    }
                }
            }

            rem = newRem
        }

        props[propWhere] = _where

        if (0 === rem.length) {
            // exhausted all filter keys, show tournament


            return [Qt.resolvedUrl("pages/TournamentPage.qml"),
                    {props: props}, rem]
        } else {
            props[propTitle] = VodDataManager.label(rem[0])
            props[propKey] = rem[0]
            return [Qt.resolvedUrl("pages/FilterPage.qml"),
                    {props: props}, rem]
        }
    }

    function millisSinceTheEpoch() {
         return new Date().getTime()
    }

    function secondsSinceTheEpoch() {
         return Math.floor(millisSinceTheEpoch() / 1000)
    }

    function completeFileUrl(str) {
        if (str) {
            if (str.indexOf("/") === 0) {
                return "file://" + str
            }
        }

        return str
    }

    function secondsToTimeString(n) {
        n = Math.round(n)
        var h = Math.floor(n / 3600)
        n = n - 3600 * h
        var m = Math.floor(n / 60)
        n = n - 60 * m
        var s = Math.floor(n)

        var result = ""
        if (h > 0) {
            result = (h < 10 ? ("0" + h.toString()) : h.toString()) + ":"
        }

        result = result + (m < 10 ? ("0" + m.toString()) : m.toString()) + ":"
        result = result + (s < 10 ? ("0" + s.toString()) : s.toString())
        return result
    }

    function performOwnerGone(contentItem) {
        for (var i = 0; i < contentItem.children.length; ++i) {
            var item = contentItem.children[i]
            if (item.is_match_item) {
                item.ownerGone()
            }
        }
    }

//    function addPlaybackPause() {
//        _pausedCount +=1
//        console.debug("_pauseCount=" + _pausedCount)
//        if (_pausedCount === 1) {
//            _pausedPlayback = true
//            if (videoPlayerPage) {
//                console.debug("pause video")
//                videoPlayerPage.pause()
//            }
//        }
//    }

//    function removePlaybackPause() {
//        _pausedCount -=1
//        console.debug("_pauseCount=" + _pausedCount)
//        if (_pausedCount === 0 && _pausedPlayback) {
//            _pausedPlayback = false
//            if (videoPlayerPage) {
//                console.debug("resume video")
//                videoPlayerPage.resume()
//            }
//        }
//    }

    function deleteSeenVodFiles(_where) {
        if (!_where) {
            _where = ""
        }

        console.debug("delete seen vod files where: " + _where)
        var count = VodDataManager.deleteSeenVodFiles(_where)
        if (count) {
            //% "%1 seen VOD files deleted"
            deleteSeenVodFilesNotification.summary = qsTrId("sf-global-seen-vod-files-deleted-notification-summary", count).arg(count)
            deleteSeenVodFilesNotification.publish()
        }
    }

    function deleteVods(_where) {
        if (!_where) {
            _where = ""
        }

        console.debug("delete vods where: " + _where)
        var count = VodDataManager.deleteVods(_where)
        if (count) {
            //% "%1 VODs deleted"
            deleteVodNotification.summary = qsTrId("sf-global-vods-deleted-notification-summary", count).arg(count)
            deleteVodNotification.publish()
        }
    }

    function undeleteVods(_where) {
        if (!_where) {
            _where = ""
        }

        console.debug("undelete vods where: " + _where)
        var count = VodDataManager.undeleteVods(_where)
        if (count) {
            //% "%1 VODs undeleted"
            undeleteVodNotification.summary = qsTrId("sf-global-vods-undeleted-notification-summary", count).arg(count)
            undeleteVodNotification.publish()
        }
    }

    function openNewVodPage(pageStack) {
        pageStack.push(
                    Qt.resolvedUrl("pages/NewPage.qml"),
                    {
                        table: defaultTable,
                        constraints: getNewVodsContstraints()
                    })
    }

    function dump(obj, prefix) {
        if (!prefix) {
            prefix = ""
        }

        if (null === obj) {
            console.debug(prefix + "null")
        } else if ("object" === typeof obj) {
            for (var key in obj) {
                console.debug(prefix + key + ":")
                dump(obj[key], "\t" + prefix)
            }
        } else {
            console.debug(prefix + obj)
        }
    }

    SqlVodModel {
        id: _model
//        columns: ["count"]
        database: VodDataManager.database
    }


}
