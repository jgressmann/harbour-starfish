import QtQuick 2.0
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0
import ".."

ListItem {
    id: root

    property alias stageName: label.text
    property var filters: undefined
    property var _myFilters: undefined

    Item {
        x: Theme.horizontalPageMargin
        width: parent.width - 2*x
        height: parent.height

        Label {
            id: label
//            height: parent.height
            //anchors { left: image.right; right: parent.right; }
            width: parent.width
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.right: seenButton.left
            font.pixelSize: Global.labelFontSize
            truncationMode: TruncationMode.Fade
        }


        SeenButton {
            id: seenButton
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right

            onClicked: {
                console.debug("seen=" + seen)
                var newValue = seen >= 1 ? false : true
                seen = newValue ? 1 : 0
                VodDataManager.setSeen(_myFilters, newValue)
            }
        }
    }

    onClicked: {
        pageStack.push(Qt.resolvedUrl("StagePage.qml"), {
            filters: filters,
            stage: stageName,
        })
    }

    Component.onCompleted: {
        _myFilters = Global.clone(filters)
        _myFilters["stage_name"] = stageName

        update()
    }


    function update() {
        seenButton.seen = VodDataManager.seen(_myFilters)
    }
}

