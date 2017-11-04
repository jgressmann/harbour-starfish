import QtQuick 2.0
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0
import ".."

BackgroundItem {
    id: root
    property string key: ""
    property var value: undefined
    property var filters: undefined
    property bool showImage: Global.showIcon(key)
    property bool grid: false

    Rectangle {
        anchors.fill: parent
        color: "transparent"

        Row {
            x: Theme.paddingLarge
            width: parent.width - 2*Theme.paddingLarge
            height: parent.height
            visible: !grid
            //anchors.fill: parent
            spacing: Theme.paddingSmall
            Image {
                y: parent.y + (parent.height - sourceSize.height) * 0.5
                id: imageList
                source: Sc2LinksDotCom.icon(key, value)
                sourceSize.width: 96
                sourceSize.height: 96
                fillMode: Image.PreserveAspectFit
                visible: showImage
            }

            Label {
                height: parent.height
                //anchors { left: image.right; right: parent.right; }
                width: parent.width - imageList.width
                text:  Sc2LinksDotCom.label(key, value)
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: Theme.fontSizeHuge
                truncationMode: TruncationMode.Fade
            }
        }

        Column {
    //        x: Theme.paddingLarge
    //        width: parent.width - 2*Theme.paddingLarge
            visible: grid
            anchors.fill: parent
            //width: imageGrid.width + 2*Theme.paddingLarge
            //height: imageGrid.height + gridLabel.height + 2*Theme.paddingMedium


            Image {
                id: imageGrid
                //x: Theme.paddingLarge
//                width: 256
//                height: 256
                anchors.horizontalCenter: parent.horizontalCenter
                source: Sc2LinksDotCom.icon(key, value)
                sourceSize.width: 256
                sourceSize.height: 256
                fillMode: Image.PreserveAspectFit
            }

            Label {
                id: gridLabel
                anchors.horizontalCenter: parent.horizontalCenter
                //width: parent.width
                text:  Sc2LinksDotCom.label(key, value)
                //verticalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontSizeSmall
                truncationMode: TruncationMode.Fade
            }
        }
    }

    onClicked: {
        console.debug("filter item click " + key + "=" + value)
        var myFilters = Global.clone(filters)
        myFilters[key] = value

        var rem = Global.remainingKeys(myFilters)
        if (0 === rem.length) {
            // exhausted all filter keys, show tournament
            pageStack.push(Qt.resolvedUrl("TournamentPage.qml"), {
                filters: myFilters
            })
        } else {
            if (1 === rem.length) {
                var sql = "select distinct "+ rem[0] +" from vods" + Global.whereClause(myFilters)
                var values = Global.values(sql)
                if (values.length === 1) {
                    myFilters[rem[0]] = values[0]
                    pageStack.push(Qt.resolvedUrl("TournamentPage.qml"), {
                        filters: myFilters
                    })
                } else {
                    pageStack.push(Qt.resolvedUrl("FilterPage.qml"), {
                                        title: Sc2LinksDotCom.label(rem[0]),
                                        filters: myFilters,
                                        key: rem[0],
                                    })
                }
            } else {
                pageStack.push(Qt.resolvedUrl("SelectionPage.qml"), {
                                title: "Filter",
                                filters: myFilters,
                            })
            }
        }
    }
}

