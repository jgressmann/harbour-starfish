/* The MIT License (MIT)
 *
 * Copyright (c) 2018, 2019 Jean Gressmann <jean@0x42.de>
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

PullDownMenu {
    MenuItem {
        //% "About %1"
        text: qsTrId("sf-topmenu-about").arg(App.displayName)
        onClicked: pageStack.push(Qt.resolvedUrl("pages/AboutPage.qml"))
    }

    MenuItem {
        //% "Settings"
        text: qsTrId("sf-topmenu-settings")
        onClicked: pageStack.push(Qt.resolvedUrl("pages/SettingsPage.qml"))
    }

    MenuItem {
        //% "Tools"
        text: qsTrId("sf-topmenu-tools")
        onClicked: pageStack.push(Qt.resolvedUrl("pages/ToolsPage.qml"))
    }

    MenuItem {
        visible: debugApp.value
        text: "Stats"
        onClicked: pageStack.push(Qt.resolvedUrl("pages/StatsPage.qml"))
    }

    MenuItem {
        id: openVideo
        //% "Open video"
        text: qsTrId("sf-topmenu-open-video")

        onClicked: {
            var topPage = pageStack.currentPage
            var callback = function (playlist, seen) {
                pageStack.pop(topPage, PageStackAction.Immediate)
                window.playPlaylist(playlist, seen)
            }

            var openPage = pageStack.push(Qt.resolvedUrl("pages/OpenVideoPage.qml"))
            openPage.videoSelected.connect(callback)
        }
    }

    MenuItem {
        //% "Fetch new VODs"
        text: qsTrId("sf-topmenu-fetch")
        enabled: window.canFetchVods
        onClicked: window.fetchNewVods()
    }
}
