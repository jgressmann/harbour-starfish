import QtQuick 2.0
import Sailfish.Silica 1.0
//import org.duckdns.jgressmann 1.0
//import ".."

BackgroundItem {
    id: root
    property alias label: label.text
    property alias icon: image.text
    property var owner: undefined

    Row {
        height: 128
        spacing: Theme.paddingMedium
        Image {
            id: image
//            source: Sc2LinksDotCom.gameIconPath(game)
        }

        Label {
            id: label
        }
    }

    onClicked: {
        pageStack.push(Qt.resolvedUrl("pages/FilterPage.qml"), {  })
    }
}

