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

    property string table
    property string where

    SqlVodModel {
        id: sqlModel
        dataManager: VodDataManager
        columns: ["event_full_name", "stage_name" ]
        select: "select distinct " + columns.join(",") + " from " + table + where + " order by match_date desc"
    }

    SilicaFlickable {
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
            header: PageHeader {
                title: sqlModel.data(sqlModel.index(0, 0), 0)

                VodDataManagerBusyIndicator {}
            }

            delegate: StageItem {
                width: listView.width
                contentHeight: Global.itemHeight // StageItem is a ListItem
                stageName: stage_name
                table: page.table
                where: {
                    var myFilter = "stage_name='" + stageName + "'"
                    if (page.where.length > 0) {
                        return page.where + " and " + myFilter
                    }

                    return " where " + myFilter
                }

                onClicked: {
                    ListView.view.currentIndex = index
                }
            }

            ViewPlaceholder {
                enabled: listView.count === 0
                text: "nothing here"
            }
        }
    }



    onStatusChanged: {
        // update items after activation
        if (status === PageStatus.Active) {
            var contentItem = listView.contentItem
            for(var index in contentItem.children) {
                var item = contentItem.children[index]
                item.update()
            }
        }
    }
}

