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

Page {
    backNavigation: false

    property string targetDirectory

    PageHeader {
        title: "Data directory move"
    }

    Column {
        width: parent.width
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.paddingLarge
        ProgressBar {
            id: progressBar
            width: parent.width
        }

        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Cancel"
            onClicked: {
                console.debug("cancel move data directory");
                VodDataManager.cancelMoveDataDirectory()
            }
        }
    }

    Component.onCompleted: {
        VodDataManager.dataDirectoryChanging.connect(dataDirectoryChanging)
        VodDataManager.moveDataDirectory(targetDirectory)
    }

    function dataDirectoryChanging(changeType, path, progress, error, errorDescription) {
        console.debug("change type=" + changeType + ", path=" + path + ", progress=" + progress
                      + ", error=" + error + ", error desc=" + errorDescription)

        progressBar.progressValue = progress
        switch (changeType) {
        case VodDataManager.DDCT_Copy:
            progressBar.label = "Copying " + path
            break
        case VodDataManager.DDCT_Remove:
            progressBar.label = "Removing " + path
            break
        case VodDataManager.DDCT_Move:
            progressBar.label = "Moving " + path
            break
        case VodDataManager.DDCT_Finished:
            switch (error) {
            case VodDataManager.Error_None:
                break
            case VodDataManager.Error_Warning:
                break
            }

            pageStack.pop()
            break
        }
    }
}
