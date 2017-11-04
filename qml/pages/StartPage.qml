import QtQuick 2.0
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0

Page {
    id: page

    readonly property int vodCount: _vodCount.data(_vodCount.index(0, 0), 0)
    readonly property bool areVodsAvailable: {
        var status = Sc2LinksDotCom.status
        return Sc2LinksDotCom.Status_VodFetchingComplete === status ||
                (Sc2LinksDotCom.Status_VodFetchingInProgress === status && vodCount > 0)
    }

    property bool _done: false

    Connections {
        target: Sc2LinksDotCom
        onStatusChanged: modelStatusChanged()
    }

    SqlVodModel {
        id: _vodCount
        columns: ["count"]
        select: "select count(*) as count from vods"
        vodModel: Sc2LinksDotCom
    }

    Timer {
        interval: 100
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
                    key: "game",
                    grid: true,
                })

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
                if (visible) {
                    console.debug("error: " + Sc2LinksDotCom.errorString)
                }
           }
        }

        // download in progress
        Column {
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
            visible: !areVodsAvailable && Sc2LinksDotCom.status === Sc2LinksDotCom.Status_VodFetchingInProgress

//            Image {
//               source: "image://theme/icon-l-transfer"
//               anchors.horizontalCenter: parent.horizontalCenter
//            }

//            ProgressBar {
//                value: Sc2LinksDotCom.vodFetchingProgress
//                width: parent.width
//                valueText: Math.round(100*value)
//                label: qsTr("Data is being loaded...")
//            }

            ProgressCircle {
                borderWidth: Theme.paddingMedium
                anchors.horizontalCenter: parent.horizontalCenter
                width: Theme.itemSizeHuge
                height: Theme.itemSizeHuge
                value: Sc2LinksDotCom.vodFetchingProgress
            }

            Row {
                height: Theme.paddingLarge
            }

            Label {
                text: qsTr("Data is being loaded...")
                anchors.horizontalCenter: parent.horizontalCenter
                font.pixelSize: Theme.fontSizeLarge
                color: Theme.highlightColor
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
//        Image {
//            anchors.centerIn: parent
//            source: Sc2LinksDotCom.dataDir + "/media/sc2.png"
//            visible: areVodsAvailable
//        }
    }
}

