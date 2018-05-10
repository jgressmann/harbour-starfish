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

//    property var scraper

    function whereClause(filters) {
        var where = ""
        if (typeof(filters) === "object") {
            for (var key in filters) {
                var skey = sanitize(key.toString())
                var value = sanitize(filters[key].toString())
                if (where.length > 0) {
                    where += " and "
                } else {
                    where += " where "
                }

                where += skey + "='" + value + "'"
            }
        }

        return where;
    }

    function sanitize(str) {
        str = _replaceWhileChange(str, "\\", "")
        str = _replaceWhileChange(str, ";", "")
        str = _replaceWhileChange(str, "'", "\\'")
        str = _replaceWhileChange(str, "\"", "\\\"")
//        var index = str.indexOf(" ")
//        if (index >= 0) {
//            str = "\"" + str + "\""
//        }

        return str
    }

    function _replaceWhileChange(str, needle, replacement) {
        var index = str.indexOf(needle)
        if (index >= 0) {
            var x = str;
            do {
                str = x;
                x = str.replace(needle, replacement)
            } while (x !== str)
        }

        return str
    }

    function values(sql) {
        console.debug(sql)

        _model.select = sql;

        var result = []
        var rows = _model.rowCount()
        console.debug(sql + ": " + rows + " rows")
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
        console.debug(sql)


        _model.select = sql;
        if (_model.rowCount() !== 1) {
            var error = _model.lastError
            console.debug("sql error: " + error)
            return
        }

        var result = _model.data(_model.index(0, 0), 0)
        console.debug("sql result: " + result)
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

//    function singleRemainingKey(filters) {
//        var rem = undefined
//        for (var key in filters) {
//            var index = filterKeys.indexOf(key)
//            if (index === -1) {
//                rem = key
//                break
//            }
//        }

//        return rem
//    }

    function remainingKeys(filters) {
        var result = []

        for (var i = 0; i < filterKeys.length; ++i) {
            var key = filterKeys[i]
            if (key in filters) {
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

    function performNext(myFilters) {
        var rem = remainingKeys(myFilters)

        // we need to figure out if there is only as single value for any of the subkeys
        for (var change = true; change; ) {
            change = false
            var newRem = []
            for (var i = 0; i < rem.length; ++i) {
                var sql = "select distinct "+ rem[i] +" from vods" + whereClause(myFilters)
                var v = values(sql)
                if (v.length === 1) {
                    console.debug("performNext single value " + rem[i] + "=" + v[0])
                    myFilters[rem[i]] = v[0]
                    change = true
                } else {
                    newRem.push(rem[i])
                }
            }
            rem = newRem
        }

        if (0 === rem.length) {
            // exhausted all filter keys, show tournament
            return [Qt.resolvedUrl("pages/TournamentPage.qml"), {
                filters: myFilters
            },
                    rem]
        } else if (1 === rem.length) {
            return [Qt.resolvedUrl("pages/FilterPage.qml"), {
                                title: VodDataManager.label(rem[0]),
                                filters: myFilters,
                                key: rem[0],
                            },rem]
        } else {
            return [Qt.resolvedUrl("pages/FilterPage.qml"), {
                                title: VodDataManager.label(rem[0]),
                                filters: myFilters,
                                key: rem[0],
                            },rem]
//            return [Qt.resolvedUrl("pages/SelectionPage.qml"), {
//                            title: "Filter",
//                            filters: myFilters,
//                            remainingKeys: rem,
//                    },rem]
        }
    }

    function millisSinceTheEpoch() {
         return new Date().getTime()
    }

    function secondsSinceTheEpoch() {
         return Math.floor(millisSinceTheEpoch() / 1000)
    }

    SqlVodModel {
        id: _model
        columns: ["count"]
        dataManager: VodDataManager
    }


}
