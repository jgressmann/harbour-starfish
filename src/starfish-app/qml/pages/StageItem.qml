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

ListItem {
    id: root

    property var page
    readonly property var props: page.props
    readonly property var breadCrumbs: props[Global.propBreadcrumbs]
    readonly property int hidden: props[Global.propHiddenFlags]
    property alias stageName: label.text
    property string _where
    readonly property bool _toolEnabled: !VodDataManager.busy

    menu: ContextMenu {
        MenuItem {
            visible: (hidden & VodDataManager.HT_Deleted) === 0
            text: Strings.deleteSeenVodFiles
            enabled: _toolEnabled
            onClicked: {
                root.remorseAction(Strings.deleteSeenVodFileRemorse(stageName), function() {
                    Global.deleteSeenVodFiles(_where)
                })
            }
        }

        MenuItem {
            visible: (hidden & VodDataManager.HT_Deleted) === 0
            text: Strings.deleteVods
            enabled: _toolEnabled
            onClicked: {
                root.remorseAction(Strings.deleteVodsRemorse(stageName), function() {
                    Global.deleteVods(_where)
                })
            }
        }

        MenuItem {
            visible: (hidden & VodDataManager.HT_Deleted) !== 0
            text: Strings.undeleteVods
            enabled: _toolEnabled
            onClicked: {
                root.remorseAction(Strings.undeleteVodsRemorse(stageName), function() {
                    Global.undeleteVods(_where)
                })
            }
        }
    }

    Item {
        x: Theme.horizontalPageMargin
        width: parent.width - 2*x
        height: parent.height

        Label {
            id: label
            width: parent.width
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
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
                progress = newValue ? 1 : 0
                VodDataManager.setSeen(page.table, _where, newValue)
            }
        }
    }

    onClicked: {
        var newBreadCrumbs = Global.clone(breadCrumbs)
        newBreadCrumbs.push(stageName)
        var newProps = Global.clone(props)
        newProps[Global.propBreadcrumbs] = newBreadCrumbs
        newProps[Global.propWhere] = _where
        pageStack.push(Qt.resolvedUrl("StagePage.qml"), {
            props: newProps,
            stage: stageName
        })
    }

    Component.onCompleted: {
        if (!page) {
            console.error("no page set")
            return
        }

        if (typeof(page.__type_TournamentPage) === "undefined") {
            console.error("Page must be set to a TournamentPage")
            return
        }

        var myFilters = "stage_name='" + stageName + "'"
        if (page.where.length > 0) {
            _where = page.where + " and " + myFilters
        } else {
            _where = " where " + myFilters
        }

        update()
    }


    function update() {
        seenButton.progress = VodDataManager.seen(table, _where)
    }
}

