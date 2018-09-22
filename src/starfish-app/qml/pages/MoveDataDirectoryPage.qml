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
import Nemo.Notifications 1.0
import org.duckdns.jgressmann 1.0
import ".."

Page {
    backNavigation: false

    property string targetDirectory
    property string warnings: ""
    property bool _done: false

    PageHeader {
        title: "Data directory move"
    }

    Column {
        width: parent.width
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.paddingLarge

        Label {
            wrapMode: Text.WordWrap
            x: Theme.horizontalPageMargin
            width: parent.width - 2*x
            font.pixelSize: Theme.fontSizeLarge
            color: Theme.highlightColor
            text: "The data directory is being moved. Do not close the application forcefully or you might end up in an inconsistent state.

You can cancel the move at any time using the button below."
        }

        ProgressCircle {
            anchors.horizontalCenter: parent.horizontalCenter
            id: progressBar
            width:  Theme.iconSizeLarge
            height: width
        }

        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Cancel"
            onClicked: {
                console.debug("cancel move data directory");
                VodDataManager.cancelMoveDataDirectory()
            }
        }

        Label {
            id: description
            wrapMode: Text.WordWrap
            x: Theme.horizontalPageMargin
            width: parent.width - 2*x
            font.pixelSize: Theme.fontSizeMedium
            color: Theme.highlightColor
        }
    }

    Component.onCompleted: {
        VodDataManager.dataDirectoryChanging.connect(dataDirectoryChanging)
//        VodDataManager.moveDataDirectory(targetDirectory)
    }

    function dataDirectoryChanging(changeType, path, progress, error, errorDescription) {
        console.debug("change type=" + changeType + ", path=" + path + ", progress=" + progress
                      + ", error=" + error + ", error desc=" + errorDescription)

        progressBar.value = progress

        if (VodDataManager.Error_Warning === error) {
            warnings += errorDescription + "\n"
        }

        switch (changeType) {
        case VodDataManager.DDCT_Copy:
            description.text = "Copying " + path
            break
        case VodDataManager.DDCT_Remove:
            description.text = "Removing " + path
            break
        case VodDataManager.DDCT_Move:
            description.text = "Moving " + path
            break
        case VodDataManager.DDCT_Finished:
            _done = true
            switch (error) {
            case VodDataManager.Error_None:
                if (warnings.length > 0) {
                    notification.previewBody = notification.summary = "Data directory moved"
                    notification.body = "Data directory moved to " + targetDirectory + ".

    " + warnings
                    notification.category = "x-nemo.general.warning"
                } else {
                    notification.previewBody = notification.summary = "Data directory moved"
                    notification.body = "Data directory moved to " + targetDirectory
                }
                break
            case VodDataManager.Error_Warning:
                //x-nemo.general.warning
                //notification.icon = "icon-lock-warning"
                notification.previewBody = notification.summary = "Data directory moved"
                notification.body = "Data directory moved to " + targetDirectory + ".

" + errorDescription
                notification.category = "x-nemo.general.warning"
                break
            case VodDataManager.Error_Canceled:
                notification.previewBody = notification.summary = "Data directory move canceled"
                break
            case VodDataManager.Error_NoSpaceLeftOnDevice:
                notification.category = "x-nemo.transfer.error"
                notification.previewBody = notification.summary = "Data directory moved failed"
                notification.body = "There is no space left on the device."
                break;
            default:
                notification.category = "x-nemo.transfer.error"
                //notification.icon = "icon-lock-warning"
                notification.previewBody = notification.summary = "Data directory moved failed"
                notification.body = errorDescription
                break;
            }

            notification.publish()
            _tryPop()
            break
        }
    }

    Notification {
         id: notification
         appName: App.displayName
         appIcon: "/usr/share/icons/hicolor/86x86/apps/harbour-starfish.png"
    }

    Connections {
        target: pageStack
        onBusyChanged: _tryPop()
    }

    function _tryPop() {
        if (_done && !pageStack.busy) {
            pageStack.pop()
        }
    }

}
