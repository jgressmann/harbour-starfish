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
    property string key: ""
    property var value: undefined
    property var filters: undefined
    property bool showImage: Global.showIcon(key)
    property bool grid: false
    property var _myFilters: undefined
    property real foo: 0

    Item {
        anchors.fill: parent

        Item {
            x: Theme.horizontalPageMargin
            width: parent.width - 2*x
            height: parent.height
            visible: !grid

            Item {
                id: image
                width: visible ? Theme.iconSizeMedium + Theme.paddingMedium : 0
                height: Theme.iconSizeMedium
                visible: showImage
                anchors.verticalCenter: parent.verticalCenter

                Image {
                    source: VodDataManager.icon(key, value)
                    width: Theme.iconSizeMedium
                    height: Theme.iconSizeMedium
                    sourceSize.width: width
                    sourceSize.height: height
                    //anchors.verticalCenter: parent.verticalCenter
                    fillMode: Image.PreserveAspectFit
                }
            }


            Label {
//                height: parent.height
                //anchors { left: image.right; right: parent.right; }
                text:  VodDataManager.label(key, value)
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
                    console.debug("seen=" + seen)
                    var newValue = seen >= 1 ? false : true
                    seen = newValue ? 1 : 0
                    VodDataManager.setSeen(_myFilters, newValue)
                }
            }
        }

        Column {
//            x: Theme.horizontalPageMargin
//            y: Theme.paddingSmall
//            width: parent.width - 2*x
//            height: parent.height - 2*y
            anchors.centerIn: parent
            visible: grid
//            anchors.fill: parent
            //width: imageGrid.width + 2*Theme.paddingLarge
            //height: imageGrid.height + gridLabel.height + 2*Theme.paddingMedium


            Image {
                id: imageGrid
                //x: Theme.paddingLarge
//                width: 256
//                height: 256
                anchors.horizontalCenter: parent.horizontalCenter
                source: VodDataManager.icon(key, value)
                width: 2*Theme.iconSizeLarge
                height: 2*Theme.iconSizeLarge
                sourceSize.width: width
                sourceSize.height: height
                fillMode: Image.PreserveAspectFit

//                Component.onCompleted: {
//                    console.debug("grid icon " + width + "x" + height)
//                }
            }

            Label {
                id: gridLabel
                anchors.horizontalCenter: parent.horizontalCenter
                //width: parent.width
                text:  VodDataManager.label(key, value)
                //verticalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeMedium
                truncationMode: TruncationMode.Fade
            }
        }
    }

    onClicked: {
        console.debug("filter item click " + key + "=" + value)
        var result = Global.performNext(_myFilters)
        pageStack.push(result[0], result[1])
    }

    Component.onCompleted: {
        _myFilters = Global.clone(filters)
        _myFilters[key] = value

//        console.debug("filters=", _myFilters)
//        console.debug(_myFilters)
//        dump(_myFilters)
//        seenButton.seen = VodDataManager.seen(_myFilters) >= 1
        update()
    }


    function update() {
        seenButton.seen = VodDataManager.seen(_myFilters)
//        seenButton.seen = foo
    }
}

