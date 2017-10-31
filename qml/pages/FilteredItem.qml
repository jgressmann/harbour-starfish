import QtQuick 2.0
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0
import ".."

BackgroundItem {
    id: root
    property string key: undefined
    property var value: undefined
    property var filters: undefined
    property alias showImage: image.visible

    Column {
        x: Theme.paddingLarge
        width: parent.width - 2*Theme.paddingLarge
        Row {
            height: 128
            width: parent.width
            spacing: Theme.paddingLarge
            Image {
                id: image
                source: Sc2LinksDotCom.icon(key, value)
                sourceSize.width: 128
                sourceSize.height: 128
            }

            Label {
                id: label
                height: parent.height
                //anchors { left: image.right; right: parent.right; }
                width: parent.width - image.width
                text:  Sc2LinksDotCom.label(key, value)
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: Theme.fontSizeHuge
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
        if (rows === 1) {
            pageStack.push(Qt.resolvedUrl("TournamentPage.qml"), {
                filters: newFilters,
            })
        } else {

            if (newFilters.length === Global.filterKeys.length) {
                pageStack.push(Qt.resolvedUrl("Tournament.qml"), {
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
}

