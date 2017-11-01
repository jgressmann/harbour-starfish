import QtQuick 2.0
import Sailfish.Silica 1.0
//import org.duckdns.jgressmann 1.0
import ".."

BackgroundItem {
    id: root
    property string side1_: ""
    property string side2_: ""
    property string url_: ""
    property int matchNumber: 0
    property int matchCount: 0
    property var matchDate
    property bool seen: false
    //"Match "+matchNumber + "/" + matchCount + ": " +

    Column {
        x: Theme.paddingLarge
        width: parent.width - 2*Theme.paddingLarge
        Label {
            width: parent.width
            text: Qt.formatDate(matchDate)
            horizontalAlignment: Text.AlignLeft
            font.pixelSize: Theme.fontSizeTiny
        }

        Label {
            text: seen ? (side1_ + qsTr(" vs. " )+ side2_) : qsTr("Match " + matchNumber)
        }

//        Label {
//            text: "Match "+matchNumber + "/" + matchCount + ": " + side1_ + " vs. " + side2_
//        }
    }

    onClicked: {
        Qt.openUrlExternally(url_)
        console.debug(url_)
    }


}

