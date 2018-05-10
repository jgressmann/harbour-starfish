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

BasePage {
    id: page

    property string title: undefined
    property var filters: undefined
    property string key: undefined
    property bool ascending: Global.sortKey(key) === 1
    property bool grid: false
    property bool _grid: grid && Global.hasIcon(key)


    SqlVodModel {
        id: sqlModel
        dataManager: VodDataManager
        columns: [key]
        select: {
            var sql = "select distinct "+ key +" from vods"
            sql += Global.whereClause(filters)
            sql += " order by " + key + " " + (ascending ? "asc" : "desc")
            console.debug(title + ": " + sql)
            return sql
        }
    }

    SilicaFlickable {


        id: flickable
        anchors.fill: parent

        // Why is this necessary?
        contentWidth: parent.width


        VerticalScrollDecorator {}

        TopMenu {}
//        BottomMenu {}



        HighlightingListView {
            id: listView
            anchors.fill: parent
            model: sqlModel
            visible: !_grid
            header: PageHeader {
                title: page.title
            }

            delegate: FilterItem {
                width: listView.width
                //height: Global.itemHeight
                contentHeight: Global.itemHeight // FilterItem is a ListItem
                key: page.key
                value: sqlModel.data(sqlModel.index(index, 0), 0)
                filters: page.filters
                grid: page.grid
                foo: index * 0.1

                onClicked: {
                    ListView.view.currentIndex = index
                }

                Component.onCompleted: {
                    console.debug("filter page key: "+key+" index: " + index + " value: " + value)
                }
            }

            ViewPlaceholder {
                enabled: listView.count === 0
                text: "nothing here"
            }


        }

        SilicaGridView {
            id: gridView
            anchors.fill: parent
            model: sqlModel
            visible: _grid
            header: PageHeader {
                title: page.title
            }

//                    cellWidth: 2*Theme.horizontalPageMargin + 2*Theme.iconSizeExtraLarge
//                    cellHeight: 3*Theme.paddingLarge + 2*Theme.iconSizeExtraLarge
            cellWidth: parent.width / 2
            cellHeight: parent.height / (Global.gridItemsPerPage / 2)
            highlight: Rectangle {
                color: Theme.primaryColor
                opacity: Global.pathTraceOpacity
            }

            delegate: FilterItem {
                key: page.key
                value: sqlModel.data(sqlModel.index(index, 0), 0)
                filters: page.filters
                grid: page.grid
                //height: gridView.cellHeight
                contentHeight: gridView.cellHeight // FilterItem is a ListItem
                width: gridView.cellWidth
//                        width: GridView.view.width
//                        height: GridView.view.height

                onClicked: {
                    GridView.view.currentIndex = index
                }

                Component.onCompleted: {
                    console.debug("filter page key: "+key+" index: " + index + " value: " + value)
                }
            }

            ViewPlaceholder {
                enabled: gridView.count === 0
                text: "nothing here"
            }

            Component.onCompleted: {
                currentIndex = -1
            }
        }
    }


    onStatusChanged: {
        // update items after activation
        if (status === PageStatus.Active) {
            var contentItem = grid ? gridView.contentItem : listView.contentItem
            for(var index in contentItem.children) {
                var item = contentItem.children[index]
                item.update()
            }
        }
    }
}

