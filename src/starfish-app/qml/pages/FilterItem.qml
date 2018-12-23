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
import "."

ListItem {
    id: root
    property var /*FilterPage*/ page
    property var value: undefined
    property bool showImage: _showImage
    property bool _showImage
    property string _where
    readonly property string title: _title
    property string _title
    property string _icon
    property var updateSeen: function () {}
    readonly property bool _toolEnabled: !VodDataManager.busy

    menu: ContextMenu {
        MenuItem {
            text: Strings.deleteSeenVodFiles
            enabled: _toolEnabled
            onClicked: {
                root.remorseAction(Strings.deleteSeenVodFileRemorse(title), function() {
                    Global.deleteSeenVodFiles(_where)
                })
            }
        }

        MenuItem {
            text: Strings.deleteVods
            enabled: _toolEnabled
            onClicked: {
                root.remorseAction(Strings.deleteVodsRemorse(title), function() {
                    Global.deleteVods(_where)
                })
            }
        }
    }

    Item {
        id: container
        anchors.fill: parent

        Component {
            id: listItem
            Item {
                x: Theme.horizontalPageMargin
                width: parent.width - 2*x
                height: parent.height


                Item {
                    id: image
                    width: visible ? Theme.iconSizeMedium + Theme.paddingMedium : 0
                    height: Theme.iconSizeMedium
                    visible: showImage
                    anchors.verticalCenter: parent.verticalCenter

                    Image {
                        source: _icon
                        width: Theme.iconSizeMedium
                        height: Theme.iconSizeMedium
                        sourceSize.width: width
                        sourceSize.height: height
                        //anchors.verticalCenter: parent.verticalCenter
                        fillMode: Image.PreserveAspectFit
                        asynchronous: true
                    }
                }


                Label {
                    text: title
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: image.right
                    anchors.right: seenButton.left
                    font.pixelSize: Global.labelFontSize
                    truncationMode: TruncationMode.Fade
                }

                SeenButton {
                    id: seenButton
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: parent.right

                    onClicked: {
                        console.debug("seen=" + progress)
                        var newValue = progress >= 1 ? false : true
                        if (newValue) {
                            progress = 1
                        } else {
                            progress = 0
                        }

                        VodDataManager.setSeen(page.table, _where, newValue)
                    }

                    Component.onCompleted: {
                        root.updateSeen = function () {
                            progress = VodDataManager.seen(page.table, _where)
                        }
                    }
                }
            }
        }

        Component {
            id: gridItem
            Column {
                anchors.centerIn: parent

                Image {
                    anchors.horizontalCenter: parent.horizontalCenter
                    source: _icon
                    width: 2*Theme.iconSizeLarge
                    height: 2*Theme.iconSizeLarge
                    sourceSize.width: width
                    sourceSize.height: height
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    //width: parent.width
                    text: title
                    //verticalAlignment: Text.AlignHCenter
                    font.pixelSize: Theme.fontSizeMedium
                    truncationMode: TruncationMode.Fade
                }
            }
        }

        Loader {
            id: loader
            onLoaded: item.parent = container
        }
    }

    onClicked: {
        //console.debug("filter item click " + key + "=" + value)
        var newBreadCrumbs = Global.clone(page.breadCrumbs)
        newBreadCrumbs.push(title)
        var result = Global.performNext(page.table, _where, newBreadCrumbs)
        pageStack.push(result[0], result[1])
    }


    Component.onCompleted: {
        if (!page) {
            console.error("no page set")
            return
        }

        if (typeof(page.__type_FilterPage) === "undefined") {
            console.error("Page must be set to a FilterPage")
            return
        }

        _title = VodDataManager.label(page.key, value)
        _showImage = Global.showIcon(page.key)
        if (_showImage) {
            _icon = VodDataManager.icon(page.key, value)
        }

        var myFilter = page.key + "='" + value + "'"
        if (page.where.length > 0) {
            _where = page.where + " and " + myFilter
        } else {
            _where = " where " + myFilter
        }

        loader.sourceComponent = page.grid ? gridItem : listItem

        updateSeen()
    }
}

