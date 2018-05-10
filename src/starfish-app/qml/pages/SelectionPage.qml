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
    property var remainingKeys: undefined
    property var filters: undefined

    ListModel {
        id: listModel
    }

    Component.onCompleted: {
        //remainingKeys = Global.remainingKeys(filters)
        console.debug("remaining keys " + remainingKeys.join(", "))

        var entries = []
        for (var i = 0; i < remainingKeys.length; ++i) {
            var key = remainingKeys[i]
            console.debug("key: " + key)
            var entry = {
                "key": key,
                "label": VodDataManager.label(key, undefined),
            }
            entries.push(entry)
        }

        entries.sort()

        for (var i = 0; i < entries.length; ++i) {
            console.debug("key: " + entries[i].key + " value: " + entries[i].label)
            listModel.append(entries[i])
        }
    }

    SilicaFlickable {
        anchors.fill: parent

        // Why is this necessary?
        contentWidth: parent.width

        VerticalScrollDecorator {}
        TopMenu {}
//        BottomMenu {}

        SilicaListView {
            id: listView
            anchors.fill: parent
            model: listModel
            header: PageHeader {
                title: page.title
            }

            delegate: Component {
                BackgroundItem {
                    height: Global.itemHeight
                    width: listView.width
                    Label {
                        height: parent.height
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2*x
                        text: label
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: Global.labelFontSize
                        truncationMode: TruncationMode.Fade

                    }

                    onClicked: {
                        console.debug("selection item click " + key)
                        // see if we have a single value left, then go straight to tournament
                        var myFilters = Global.clone(filters)
                        var myKey = key
                        while (true) {
                            var sql = "select distinct "+ myKey +" from vods" + Global.whereClause(myFilters)
                            var values = Global.values(sql)
                            var rem = Global.remainingKeys(myFilters)
                            if (values.length === 1) {
                                myFilters[myKey] = values[0]
                                if (0 === rem.length) {
                                    // exhausted all filter keys, show tournament
                                    pageStack.push(Qt.resolvedUrl("TournamentPage.qml"), {
                                        filters: myFilters
                                    })
                                    break
                                } else {
                                    myKey = rem[0]
                                }
                            } else {
                                // more than 1 value, show filter page
                                pageStack.push(Qt.resolvedUrl("FilterPage.qml"), {
                                    title: VodDataManager.label(rem[0], undefined),
                                    filters: myFilters,
                                    key: myKey,
                                })
                                break
                            }
                        }
                    }
                }
            }

            ViewPlaceholder {
                enabled: listView.count === 0
                text: "you should never have glimpsed sight of me..."
            }
        }
    }
}

