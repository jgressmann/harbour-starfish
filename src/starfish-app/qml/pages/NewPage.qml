import QtQuick 2.0
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0
import ".."

BasePage {
    id: page

    SqlVodModel {
        id: sqlModel
        dataManager: VodDataManager
        columns: ["event_full_name", "stage_name", "side1_name", "side2_name", "side1_race", "side2_race", "match_date", "match_name", "rowid", "offset"]
        select: "select " + columns.join(",") + " from vods where seen=0 order by match_date desc, match_name asc"
    }

    SilicaFlickable {
        anchors.fill: parent

        // Why is this necessary?
        contentWidth: parent.width

        VerticalScrollDecorator {}
        TopMenu {}
//        BottomMenu {}

        SilicaListView {
            id: listView
            anchors.fill: parent
            model: sqlModel
            header: PageHeader {
                title: qsTr("New")
            }

            delegate: Component {
                MatchItem {
                    width: listView.width
                    contentHeight: Global.itemHeight + Theme.fontSizeMedium // MatchItem is a ListItem
                    eventFullName: event_full_name
                    stageName: stage_name
                    side1: side1_name
                    side2: side2_name
                    race1: side1_race
                    race2: side2_race
                    matchName: match_name
                    matchDate: match_date
                    rowId: rowid
                    startOffsetMs: offset * 1000
                }
            }

            ViewPlaceholder {
                enabled: listView.count === 0
                text: "There are no new VODs available."
                hintText: "Pull down to search for new VODs."
            }
        }
    }

    Component.onCompleted: {
        listView.positionViewAtBeginning()
    }
}

