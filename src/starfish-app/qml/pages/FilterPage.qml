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
    id: root

    property string title
    property string table
    property string where
    property string key: undefined
    property bool ascending: Global.sortKey(key) === 1
    property bool grid: false
    property bool _grid: grid && Global.hasIcon(key)
    property var _view
    readonly property int __type_FilterPage: 1

    SqlVodModel {
        id: sqlModel
        dataManager: VodDataManager
        columns: [key]
        select: {
            var sql = "select distinct "+ key +" from " + table + where
            sql += " order by " + key + " " + (ascending ? "asc" : "desc")
            console.debug(title + ": " + sql)
            return sql
        }
    }

    contentItem: SilicaFlickable {
        id: flickable
        anchors.fill: parent

        // Why is this necessary?
        contentWidth: parent.width


        VerticalScrollDecorator {}
        TopMenu {}




        Component {
            id: listComponent

            HighlightingListView {
                id: listView
                anchors.fill: parent
                model: sqlModel
                header: ContentPageHeader {
                    title: root.title
                }

                property var _root: root

                delegate:
                    FilterItem {
                        width: listView.width
                        //height: Global.itemHeight
                        contentHeight: Global.itemHeight // FilterItem is a ListItem
                        value: sqlModel.data(sqlModel.index(index, 0), 0)
                        page: _root

                        onClicked: {
                            ListView.view.currentIndex = index
                        }

        //                Component.onCompleted: {
        //                    console.debug("filter page key: "+key+" index: " + index + " value: " + value)
        //                }
                    }


                ViewPlaceholder {
                    enabled: listView.count === 0
                    text: Global.noContent
                }


                Component.onCompleted: {
                    currentIndex = -1
                    _view = listView
                }
            }
        }

        Component {
            id: gridComponent

            SilicaGridView {
                id: gridView
                anchors.fill: parent
                model: sqlModel

                header: ContentPageHeader {
                    title: root.title
                }

    //                    cellWidth: 2*Theme.horizontalPageMargin + 2*Theme.iconSizeExtraLarge
    //                    cellHeight: 3*Theme.paddingLarge + 2*Theme.iconSizeExtraLarge
                cellWidth: parent.width / 2
                cellHeight: parent.height / (Global.gridItemsPerPage / 2)
                highlight: Rectangle {
                    color: Theme.primaryColor
                    opacity: Global.pathTraceOpacity
                }

                delegate:
                    FilterItem {
                        value: sqlModel.data(sqlModel.index(index, 0), 0)
                        contentHeight: gridView.cellHeight // FilterItem is a ListItem
                        width: gridView.cellWidth
                        page: root

                        onClicked: {
                            GridView.view.currentIndex = index
                        }

        //                Component.onCompleted: {
        //                    console.debug("filter page key: "+key+" index: " + index + " value: " + value)
        //                }
                    }


                ViewPlaceholder {
                    enabled: gridView.count === 0
                    text: Global.noContent
                }

                Component.onCompleted: {
                    currentIndex = -1
                    _view = gridView
                }
            }
        }

        Loader {
            sourceComponent: grid ? gridComponent : listComponent
            onLoaded: item.parent = flickable.contentItem
        }
    }

    onStatusChanged: {
        // update items after activation
        if (wasActive && status === PageStatus.Activating) {
            if (_view.currentItem) {
                _view.currentItem.updateSeen()
            }
        }
    }
}

