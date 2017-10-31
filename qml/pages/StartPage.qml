import QtQuick 2.0
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0

Page {
    id: page

    readonly property bool areVodsAvailable: {
        var status = Sc2LinksDotCom.status
        return Sc2LinksDotCom.Status_VodFetchingComplete === status ||
                (Sc2LinksDotCom.Status_VodFetchingInProgress === status && vodCount.rowCount() > 0)
    }

    property bool _done: false

    Connections {
        target: Sc2LinksDotCom
        onStatusChanged: modelStatusChanged()
    }

    SqlVodModel {
        id: vodCount
        columns: ["count"]
        select: "select count(*) as count from vods"
        vodModel: Sc2LinksDotCom
    }

    function initPage() {
        modelStatusChanged()
    }

    Timer {
        interval: 1000
        running: !_done && !pageStack.busy
        onTriggered: {
            modelStatusChanged()
        }
        onRunningChanged: {
            console.debug("timer enabled: " + running)
        }
    }


    function modelStatusChanged() {
//        var status = Sc2LinksDotCom.
//        if (Sc2LinksDotCom.Status_Error ==
        console.debug("start page status " + Sc2LinksDotCom.status)

        if (areVodsAvailable) {
            console.debug("start page vods are here!")
            //pageStack.completeAnimation()
            //pageStack.replace()
            pageStack.replace(
                Qt.resolvedUrl("FilterPage.qml"),
                {
                    title: qsTr("Games"),
                    filters: [],
                    key: "game"
                })
            //pageStack.navigateForward(PageStackAction.Animated)
            _done = true
        }
    }

    SilicaFlickable {
        anchors.fill: parent

        // Why is this necessary?
        contentWidth: parent.width

        PullDownMenu {
            MenuItem {
                text: qsTr("Refresh")
                onClicked: listView.scrollToBottom()
                visible: Sc2LinksDotCom.status === Sc2LinksDotCom.Status_Ready || Sc2LinksDotCom.status === Sc2LinksDotCom.Status_VodFetchingComplete
            }
        }

        // error label
        Label {
           text: qsTr("Something went wrong TT")
           anchors.centerIn: parent
           visible: Sc2LinksDotCom.status === Sc2LinksDotCom.Status_Error
           font.pixelSize: Theme.fontSizeLarge
           color: Theme.highlightColor
           onVisibleChanged: {

           }
        }

        // download in progress
        Column {
            anchors.centerIn: parent
            visible: !areVodsAvailable && Sc2LinksDotCom.status === Sc2LinksDotCom.Status_VodFetchingInProgress
            onVisibleChanged: {
            }
            Image {
               source: "image://theme/icon-l-transfer"
            }
            Label {
               text: qsTr("Data is being loaded...")
               width: parent.width
               color: Theme.highlightColor
               horizontalAlignment: Text.AlignHCenter
            }
        }

        // no content label
        Label {
           text: qsTr("There seems to be nothing here")
           anchors.centerIn: parent
           visible: !areVodsAvailable && Sc2LinksDotCom.status === Sc2LinksDotCom.Status_VodFetchingComplete
           font.pixelSize: Theme.fontSizeLarge
           color: Theme.highlightColor
           onVisibleChanged: {

           }
        }

        // vods avaialbe
        Image {
            anchors.centerIn: parent
            source: Sc2LinksDotCom.dataDir + "/media/sc2.png"
            visible: areVodsAvailable
        }
    }
}

