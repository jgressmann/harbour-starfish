import QtQuick 2.0
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0

Page {
    id: page

    readonly property int vodCount: _vodCount
    property int _vodCount: 0
    property bool _done: false


    SqlVodModel {
        id: _model
        columns: ["count"]
        select: "select count(*) as count from vods"
        vodModel: Sc2LinksDotCom
        onModelReset: {
            console.debug("sql model reset")
            _vodCount = data(index(0, 0), 0)
            modelStatusChanged()
        }

        Component.onCompleted: {
            _vodCount = data(index(0, 0), 0)
        }
    }

    Timer {
        id: timer
        interval: 1000
//        repeat: true
        running: true
//        onTriggered: modelStatusChanged()
        onRunningChanged: {
            console.debug("timer enabled: " + running)
        }
    }

    function modelStatusChanged() {
//        var status = Sc2LinksDotCom.
//        if (Sc2LinksDotCom.Status_Error ==
        console.debug("start page status " + Sc2LinksDotCom.status)
        console.debug("start page vod count " + vodCount)
        console.debug("start page vod count call " + _model.data(_model.index(0, 0), 0))

//        if (vodCount == 0 && Sc2LinksDotCom.status === Sc2LinksDotCom.Status_Ready) {
//            Sc2LinksDotCom.poll()
//        }

        if (vodCount > 0 && !busyIndicator.running) {
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
        } else {
//            timer.start()
        }
    }

    SilicaFlickable {
        anchors.fill: parent

        // Why is this necessary?
        contentWidth: parent.width

        PullDownMenu {
            MenuItem {
                text: qsTr("Fetch vods")
                onClicked: Sc2LinksDotCom.poll()
                enabled: Sc2LinksDotCom.status === Sc2LinksDotCom.Status_Ready || Sc2LinksDotCom.status === Sc2LinksDotCom.Status_VodFetchingComplete
            }

            MenuItem {
                text: qsTr("Reset database")
                onClicked: Sc2LinksDotCom.reset()
                enabled: Sc2LinksDotCom.status === Sc2LinksDotCom.Status_VodFetchingComplete || Sc2LinksDotCom.status === Sc2LinksDotCom.Status_VodFetchingInProgress
            }
        }

        BusyIndicator {
            id: busyIndicator
            size: BusyIndicatorSize.Large
            anchors.centerIn: parent
            running: timer.running
            onRunningChanged: modelStatusChanged()
        }

        Item {
            anchors.fill: parent
            visible: !busyIndicator.running
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
                visible: vodCount === 0 && Sc2LinksDotCom.status === Sc2LinksDotCom.Status_VodFetchingInProgress

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

                Rectangle {
                    height: Theme.paddingLarge
                    width: parent.width
                    color: "transparent"
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
               visible: vodCount === 0 && Sc2LinksDotCom.status === Sc2LinksDotCom.Status_VodFetchingComplete
               font.pixelSize: Theme.fontSizeLarge
               color: Theme.highlightColor

            }

            // no content label
            Label {
               text: qsTr("Pull down to fetch vods")
               anchors.centerIn: parent
               visible: vodCount === 0 && Sc2LinksDotCom.status === Sc2LinksDotCom.Status_Ready
               font.pixelSize: Theme.fontSizeLarge
               color: Theme.highlightColor

            }
        }
    }
}

