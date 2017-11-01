import QtQuick 2.0
import Sailfish.Silica 1.0
//import org.duckdns.jgressmann 1.0
//import ".."

BackgroundItem {
    id: root

    property alias stageName: label.text
//    property int stageIndex: -1
    property var filters: undefined

    Column {
        x: Theme.paddingLarge
        width: parent.width - 2*Theme.paddingLarge
//        height: label.height + separator.height + Theme.paddingSmall
        Label {
            id: label
//            height: parent.height
            //anchors { left: image.right; right: parent.right; }
            width: parent.width
//            text:  Sc2LinksDotCom.label(key, value)
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: Theme.fontSizeLarge
            truncationMode: TruncationMode.Fade
        }

//        Separator {
//            id: separator
//            width: parent.width

//        }
    }

    onClicked: {
        pageStack.push(Qt.resolvedUrl("StagePage.qml"), {
            filters: filters
        })
    }
}

