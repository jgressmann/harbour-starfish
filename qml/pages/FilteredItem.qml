import QtQuick 2.0
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0
import ".."

BackgroundItem {
    id: root
    property string key: ""
    property var value: undefined
    property var filters: undefined
    property bool showImage: true
    property bool grid: false

    Rectangle {
        anchors.fill: parent
        color: "red"

        Column {
            x: Theme.paddingLarge
            width: parent.width - 2*Theme.paddingLarge
            visible: !grid
            Row {
                height: 128
                width: parent.width
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
                anchors.horizontalCenter: parent.horizontalCenter
                source: Sc2LinksDotCom.icon(key, value)
    //            sourceSize.width: 256
    //            sourceSize.height: 256
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
        var newFilters = Global.clone(filters)
        newFilters[key] = value;
//        for (var aKey in root.filters) {
//            console.debug(label.text + ": " + aKey + "=" + root.filters[aKey])
//        }

        var rows = Global.rowCount(newFilters)
        console.debug("constraints " + Global.whereClause(newFilters) + ": " + rows + " rows")
        if (rows === 1 || newFilters.length === Global.filterKeys.length) {
            pageStack.push(Qt.resolvedUrl("TournamentPage.qml"), {
                filters: newFilters,
            })
        } else {
            pageStack.push(Qt.resolvedUrl("SelectionPage.qml"), {
                title: "Filter",
                filters: newFilters,
            })
        }
    }
}

