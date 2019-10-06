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
import "."

PageHeader {
    id: root
    leftMargin: Theme.horizontalPageMargin + busy.width + Theme.paddingSmall
    rightMargin: Theme.horizontalPageMargin + homeButton.width  + Theme.paddingMedium

    IconButton {
        x: (parent.page ? parent.page.width : Screen.width) - (Theme.horizontalPageMargin + width)
        anchors.verticalCenter: parent.verticalCenter
        id: homeButton
        width: Theme.iconSizeSmallPlus
        height: Theme.iconSizeSmallPlus
        icon.source: "image://theme/icon-m-home"
        icon.width: width
        icon.height: height
        onClicked: pageStack.replaceAbove(null, Qt.resolvedUrl("pages/EntryPage.qml"))
    }

    VodDataManagerBusyIndicator {
        id: busy
    }

    Label {
        id: breadCrumbLabel
        anchors.top: homeButton.bottom
        x: Theme.horizontalPageMargin
        width: root.width - 2*x
        font.pixelSize: Theme.fontSizeTiny
        color: Theme.highlightColor
        text: "lorem ipsum"
        elide: Text.ElideLeft
    }

    onPageChanged: _onBreadCrumbsChanged()

    function _onBreadCrumbsChanged() {
//        console.debug("bread crumbs changed")
        var s = ""
        if (page && page.props) {
            var breadCrumbs = page.props[Global.propBreadcrumbs]
            for (var i = 0; i < breadCrumbs.length; ++i) {
                if (s.length) {
                    s += " / "
                }

                s += breadCrumbs[i]
            }
        }

        breadCrumbLabel.text = s
    }
}
