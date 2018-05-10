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

    property alias stageName: label.text
    property var filters: undefined
    property var _myFilters: undefined

    Item {
        x: Theme.horizontalPageMargin
        width: parent.width - 2*x
        height: parent.height

        Label {
            id: label
//            height: parent.height
            //anchors { left: image.right; right: parent.right; }
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
                console.debug("seen=" + seen)
                var newValue = seen >= 1 ? false : true
                seen = newValue ? 1 : 0
                VodDataManager.setSeen(_myFilters, newValue)
            }
        }
    }

    onClicked: {
        pageStack.push(Qt.resolvedUrl("StagePage.qml"), {
            filters: filters,
            stage: stageName,
        })
    }

    Component.onCompleted: {
        _myFilters = Global.clone(filters)
        _myFilters["stage_name"] = stageName

        update()
    }


    function update() {
        seenButton.seen = VodDataManager.seen(_myFilters)
    }
}
