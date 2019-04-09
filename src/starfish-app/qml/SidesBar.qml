/* The MIT License (MIT)
 *
 * Copyright (c) 2019 Jean Gressmann <jean@0x42.de>
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

Item {
    height: Math.max(imageHeight, label1.height)
    width: parent ? parent.width : Screen.width
    property real imageHeight: Theme.iconSizeSmall
    property real imageWidth: Theme.iconSizeSmall
    property alias side1IconSource: icon1.source
    property alias side2IconSource: icon2.source
    property alias side1Label: label1.text
    property alias side2Label: label2.text
    property real fontSize: Theme.fontSizeMedium
    property real spacing: Theme.paddingSmall
    property alias color: label1.color

    readonly property real _labelSpace: width - 2 * (imageWidth + spacing)
    readonly property real _evenlabelWidth: _labelSpace * 0.5
    readonly property bool _labelSpaceExceeded: label1.contentWidth + label2.contentWidth > _labelSpace

    Image {
        id: icon1
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: imageWidth
        height: imageHeight
        sourceSize.width: width
        sourceSize.height: height
    }

    Item {
        id: spacer1
        width: spacing
        height: parent.height
        anchors.left: icon1.right
    }

    Label {
        id: label1
        anchors.left: spacer1.right
        anchors.verticalCenter: parent.verticalCenter
        width: _labelSpaceExceeded ? Math.min(contentWidth, _evenlabelWidth) : contentWidth
        truncationMode: TruncationMode.Fade
        horizontalAlignment: Text.AlignLeft
        font.pixelSize: fontSize
    }

    Label {
        id: label2
        anchors.right: spacer2.left
        anchors.verticalCenter: parent.verticalCenter
        width: _labelSpaceExceeded ? Math.min(contentWidth, _evenlabelWidth) : contentWidth
        truncationMode: TruncationMode.Fade
        horizontalAlignment: Text.AlignRight
        font.pixelSize: fontSize
        color: label1.color
    }

    Item {
        id: spacer2
        width: spacing
        height: parent.height
        anchors.right: icon2.left
    }

    Image {
        id: icon2
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: imageWidth
        height: imageHeight
        sourceSize.width: width
        sourceSize.height: height
    }
}
