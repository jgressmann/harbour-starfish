import QtQuick 2.0
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0

Page {
    id: page

    SqlVodModel {
        id: games
        columns: ["games"]
        select: "select distinct game from vods"
        vodModel: Sc2LinksDotCom
    }

    SilicaFlickable {
        anchors.fill: parent

        // Why is this necessary?
        contentWidth: parent.width

        VerticalScrollDecorator {}

//        PullDownMenu {
//            MenuItem {
//                text: qsTr("Refresh")
//                onClicked: listView.scrollToBottom()
//                visible: Sc2LinksDotCom.status === Sc2LinksDotCom.Status_Ready || Sc2LinksDotCom.status === Sc2LinksDotCom.Status_VodFetchingComplete
//            }
//        }

        SilicaListView {
            id: listView
            anchors.fill: parent
            visible: cozy.connected
            model: taskModel
            header: PageHeader {
                title: qsTr("Games")
            }
            delegate: Game {
                width: listView.width
                task: getTaskByIndex(index)
            }

            ViewPlaceholder {
                enabled: listView.count === 0
                text: Strings.noContent
            }
        }
    }
}

