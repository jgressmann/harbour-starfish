import QtQuick 2.0
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0
import ".."

BackgroundItem {
    id: root
    property string side1_: ""
    property string side2_: ""
    property string url_: ""
    property int matchNumber: 0
    property int matchCount: 0


//    Rectangle {
//        width: parent.width
//        height: 100
//        color: "yellow"
//    }

//    height: 100


    Label {
        text: "Match "+matchNumber + "/" + matchCount + ": " + side1_ + " vs. " + side2_
    }



    Rectangle {
        anchors.fill: parent
        color: "green"

        Row {

            //width: parent.width
            height: parent.height

                        Label {
                id: side1Label
                text: side1_
            }
            Label {
                text: "vs"
            }

            Label {
                id: side2Label
                text: side2_
            }


        }


    }

    //    onClicked: {
//        Qt.openUrlExternally(url)
////        console.debug()
//    }


}

