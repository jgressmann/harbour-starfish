import QtQuick 2.0
import QtWebKit 3.0
import QtWebKit.experimental 1.0
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0
import ".."

Page {
    id: root
    property string url: ""

    onUrlChanged: {
        var i = (url || "").indexOf("v=")
        if (i === -1) {
            webView.opacity = 0
        } else {
            currentVideo.vId = url.substring(i + 2)
            console.debug("vid: " + currentVideo.vId)
        }
    }

    property QtObject videoStatus: QtObject {
        property int initial: -1
        property int ready: 0
        property int playing: 1
        property int paused: 2
    }

    QtObject {
        id: currentVideo
        property string vId: ""
        property string title: ""
        property int status: videoStatus.initial
    }

    readonly property int padding: 20

    Rectangle {
        id: content
        anchors.fill: parent
        color: "black"
        focus: true
        WebView {
            id: webView
            anchors.fill: parent
            opacity: 0


            url: "qrc:///youtube.html?" + currentVideo.vId
            //url: "http://www.teamliquid.net"
            //url: "https://www.youtube.com"
            experimental.userAgent:"Mozilla/5.0 (Windows NT 6.3; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/37.0.2049.0 Safari/537.36"
            //unsupported codec experimental.userAgent:"Mozilla/5.0 (Linux; Android 5.1.1; Nexus 5 Build/LMY48B; wv) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Chrome/43.0.2357.65 Mobile Safari/537.36"
            //Behavior on opacity { NumberAnimation { duration: 200 } }

            onLoadingChanged: {
                console.debug("loading: " + loadRequest.status)
                switch (loadRequest.status)
                {
                case WebView.LoadSucceededStatus:
                    console.debug("loading succeeded")
                    opacity = 1
                    return
                case WebView.LoadStartedStatus:
                case WebView.LoadStoppedStatus:
                    break
                case WebView.LoadFailedStatus:
                    topInfo.text = "Failed to load the requested video"
                    break
                }
                opacity = 0
            }
            onTitleChanged: {
                currentVideo.status = 1 * title
//                if (title == videoStatus.paused || title == videoStatus.ready)
//                    panel.state = "list"
//                else if (title == videoStatus.playing)
//                    panel.state = "hidden"
            }
        }
    }
}



/*
import QtQuick 2.0
import QtWebKit 3.0
import QtQuick.XmlListModel 2.0
import "qrc:/shared" as Shared
import "qrc:/content" as Content

Rectangle {
    id: container
    width: 850
    height: 480
    color: "black"
    focus: true

    property QtObject videoStatus: QtObject {
        property int initial: -1
        property int ready: 0
        property int playing: 1
        property int paused: 2
    }

    QtObject {
        id: currentVideo
        property string vId: ""
        property string title: ""
        property int status: videoStatus.initial
    }

    readonly property int padding: 20

    Rectangle {
        id: content
        anchors.fill: parent
        color: "black"

        WebView {
            id: webView
            anchors.fill: parent
            opacity: 0

            url: "qrc:///content/player.html?autoplay=true&" + currentVideo.vId

            Behavior on opacity { NumberAnimation { duration: 200 } }

            onLoadingChanged: {
                switch (loadRequest.status)
                {
                case WebView.LoadSucceededStatus:
                    opacity = 1
                    return
                case WebView.LoadStartedStatus:
                case WebView.LoadStoppedStatus:
                    break
                case WebView.LoadFailedStatus:
                    topInfo.text = "Failed to load the requested video"
                    break
                }
                opacity = 0
            }
            onTitleChanged: {
                currentVideo.status = 1 * title
                if (title == videoStatus.paused || title == videoStatus.ready)
                    panel.state = "list"
                else if (title == videoStatus.playing)
                    panel.state = "hidden"
            }
        }

        Content.YouTubeDialog {
            id: presetDialog
            anchors.fill: parent
            visible: false
            onPresetClicked: {
                model.userName = name
                model.startIndex = 1
                panel.state = "list"
                searchBinding.when = false
                presetsBinding.when = true
                model.reload()
            }
        }
    }

    Rectangle {
        id: panel
        height: 100
        color: "black";
        state: "list"

        Behavior on y { NumberAnimation { duration: 200 } }
        Behavior on height { NumberAnimation { duration: 200 } }
        Behavior on opacity { NumberAnimation { duration: 400 } }

        Binding { id: presetsBinding; target: model; property: "source"; value: model.usersSource; when: false }
        Binding { id: searchBinding; target: model; property: "source"; value: model.searchSource; when: false }

        anchors {
            left: container.left
            right: container.right
        }

        states: [
            State {
                name: "search"
                PropertyChanges { target: panel; color: "black"; opacity: 0.8; y: -height + topInfo.height + searchPanel.height + button.height }
                PropertyChanges { target: listView; visible: false }
                PropertyChanges { target: searchPanel; opacity: 0.8 }
                PropertyChanges { target: hideTimer; running: false }
                PropertyChanges { target: presetDialog; visible: true }
            },

            State {
                name: "list"
                PropertyChanges { target: panel; color: "black"; opacity: 0.8; y: 0 }
                PropertyChanges { target: listView; visible: true; focus: true }
                PropertyChanges { target: searchPanel; visible: false }
                PropertyChanges { target: listView; visible: true }
            },

            State {
                name: "hidden"
                PropertyChanges { target: panel; color: "gray"; opacity: 0.2; y: -height }
            }
        ]

        Timer {
            id: hideTimer
            interval: 3000
            repeat: false
            onTriggered: panel.state = "hidden"
        }

        ListView {
            id: listView
            orientation: "Horizontal"

            anchors {
                top: panel.top
                bottom: button.top
                left: panel.left
                right: panel.right
            }

            focus: true
            model: model

            header: Component {
                Rectangle {
                    visible: model.startIndex != 1 && model.status == XmlListModel.Ready
                    color: "black"
                    anchors.verticalCenter: parent.verticalCenter
                    width: height
                    height: visible ? listView.contentItem.height : 0
                    Image { anchors.centerIn: parent; width: 50; height: 50; source: "qrc:/shared/images/less.png" }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: model.requestLess()
                    }
                }
            }

            footer: Component {
                Rectangle {
                    visible: model.totalResults > model.endIndex && model.status == XmlListModel.Ready
                    color: "black"
                    anchors.verticalCenter: parent.verticalCenter
                    width: height
                    height: visible ? listView.contentItem.height : 0
                    Image { anchors.centerIn: parent; width: 50; height: 50; source: "qrc:/shared/images/more.png" }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: model.requestMore()
                    }
                }
            }

            delegate: Component {
                Image {
                    source: thumbnail
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            currentVideo.vId = id
                            currentVideo.title = title
                        }
                    }
                    Component.onCompleted: {
                        if (currentVideo.title == "") {
                            currentVideo.vId = id
                            currentVideo.title = title
                        }
                    }
                }
            }

            onDraggingChanged: {
                if (dragging)
                    hideTimer.stop()
                else if (currentVideo.status == videoStatus.playing)
                    hideTimer.start()
            }
        }

        Shared.LoadIndicator {
            anchors.fill: parent
            color: "black"
            running: panel.state == "list" && model.status != XmlListModel.Ready
        }

        Rectangle {
            id: searchPanel
            Behavior on opacity { NumberAnimation { duration: 400 } }

            height: searchField.height + container.padding

            anchors {
                left: parent.left
                right: parent.right
                bottom: button.top
            }

            opacity: 0
            color: "black"

            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: "grey"
                }
                GradientStop {
                    position: 1.0
                    color: "black"
                }
            }

            Rectangle {
                id: searchField
                color: "white"
                radius: 2
                anchors.centerIn: parent
                width: 220
                border.color: "black"
                border.width: 2
                height: input.height + container.padding
                TextInput {
                    id: input
                    color: "black"
                    anchors.centerIn: parent
                    horizontalAlignment: TextInput.AlignHCenter
                    font.capitalization: Font.AllLowercase
                    maximumLength: 30
                    cursorVisible: true
                    focus: parent.visible
                    text: "movie trailers"
                    Keys.onPressed: {
                        if (event.key == Qt.Key_Return || event.key == Qt.Key_Enter) {
                            model.startIndex = 1
                            panel.state = "list"
                            presetsBinding.when = false
                            searchBinding.when = true
                            model.reload()
                        }
                    }
                }
                MouseArea {
                    anchors.fill: parent
                    onPressed: input.focus = true
                }
            }
        }

        Shared.Button {
            id: button
            buttonHeight: container.padding
            buttonWidth: container.width
            fontSize: 8

            visible: panel.state != "hidden"

            anchors {
                bottom: parent.bottom
                left: parent.left
                right: parent.right
            }

            states: [
                State {
                    name: "search"
                    PropertyChanges { target: button; text: "Press to switch back to the video list" }
                },

                State {
                    name: "list"
                    PropertyChanges { target: button; text: "Press to search for videos" }
                }
            ]

            state: panel.state

            onClicked: {
                if (panel.state == "search")
                    panel.state = "list"
                else
                    panel.state = "search"
            }
        }
    }

    Rectangle {
        height: 10
        color: "black"
        opacity: (panel.state == "hidden") ? 0 : 0.8

        Behavior on opacity { NumberAnimation { duration: 200 } }

        anchors {
            top: container.top
            left: container.left
            right: container.right
        }

        Text {
            id: topInfo
            color: "white"
            font.pointSize: 8
            anchors.centerIn: parent
            Binding on text {
                value: "Results " + model.startIndex + " through " + ((model.endIndex > model.totalResults) ? model.totalResults : model.endIndex) + " out of " + model.totalResults
                when: model.status == XmlListModel.Ready && panel.state == "list" && model.count
            }
            Binding on text {
                value: "No results found.";
                when: model.state == XmlListModel.Ready && !model.count
            }
            Binding on text {
                value: "Search for videos"
                when: panel.state == "search"
            }
        }
    }

    Rectangle {
        height: container.padding
        color: "black"
        opacity: (panel.state == "hidden") ? 0.2 : 0.8

        Behavior on opacity { NumberAnimation { duration: 200 } }

        anchors {
            top: panel.bottom
            left: container.left
            right: container.right
        }

        Text {
            id: bottomInfo
            color: "white"
            font.weight: Font.DemiBold
            font.pointSize: 8
            anchors.centerIn: parent
            text: {
                if (panel.state == "search")
                    return "Choose from preset video streams"
                else
                    return currentVideo.title
            }
        }

        MouseArea {
            // Responsible for showing and hiding the thumbnail list.
            anchors.fill: parent
            onPressed: {
                if (panel.state != "list") {
                    panel.state = "list"
                    if (currentVideo.status == videoStatus.playing)
                        hideTimer.restart()
                } else
                    panel.state = "hidden"
            }
        }
    }

    XmlListModel {
        id: model

        property int totalResults: 0
        property int itemsPerPage: 0
        property int startIndex: 1
        property int endIndex: itemsPerPage + startIndex - 1

        property string userName: "trailers"
        property string baseUrl: "https://gdata.youtube.com/feeds/api"
        property string defaultQuery: "alt=rss&orderby=published&v=2&start-index=" + startIndex
        property string searchSource: baseUrl + "/videos?" + defaultQuery + "&q=\'" + input.text + "\'"
        property string usersSource: baseUrl + "/users/" + userName + "/uploads?" + defaultQuery

        function requestMore() {
            startIndex += itemsPerPage
            reload()
        }

        function requestLess() {
            startIndex -= itemsPerPage
            reload()
        }

        onSourceChanged: {
            var xhr = new XMLHttpRequest();
            xhr.onreadystatechange = function(){
                if (xhr.readyState == XMLHttpRequest.DONE) {
                    if (xhr.status != 200) {
                        console.log("Something went wrong, received HTTP status code " + xhr.status);
                        return;
                    }
                    var doc = xhr.responseXML.documentElement;
                    for (var i = 0; i < doc.childNodes.length; ++i) {
                        var child = doc.childNodes[i];
                        for (var j = 0; j < child.childNodes.length; ++j) {
                            if (child.childNodes[j].nodeName == "itemsPerPage")
                                itemsPerPage = child.childNodes[j].childNodes[0].nodeValue;
                            if (child.childNodes[j].nodeName == "totalResults")
                                totalResults = child.childNodes[j].childNodes[0].nodeValue;
                        }
                    }
                }
            }
            xhr.open("GET", source);
            xhr.send();
        }

        namespaceDeclarations: "declare namespace media='http://search.yahoo.com/mrss/';declare namespace yt='http://gdata.youtube.com/schemas/2007';"

        source: usersSource
        query: "/rss/channel/item"

        XmlRole { name: "id"; query: "media:group/yt:videoid/string()"}
        XmlRole { name: "title"; query: "media:group/media:title/string()" }
        XmlRole { name: "thumbnail"; query: "media:group/media:thumbnail[1]/@url/string()" }
        XmlRole { name: "thumbnailHeight"; query: "media:group/media:thumbnail[1]/@height/number()" }
    }
}

*/
