import QtQuick 2.0
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0
import ".."

Column {
    id: root
    property var filters: undefined
    property alias stage: header.text

    SqlVodModel {
        id: sqlModel
        vodModel: Sc2LinksDotCom
        columns: ["side1", "side2", "played", "url", "match_count", "match_number"]
        select: "select " + columns.join(",") + " from vods " + Global.whereClause(filters) + " order by match_number asc"
    }

    Component.onCompleted: {
        console.debug(stage + " sql: " + sqlModel.select)
        var count = sqlModel.data(sqlModel.index(0, 4), 0)
        console.debug(stage + " rows: " + count)
    }

//    SilicaFlickable {
//        anchors.fill: parent

//        // Why is this necessary?
//        contentWidth: parent.width
//        contentHeight: header.height + (sqlModel.data(sqlModel.index(0, 4), 0)+1) * Theme.fontSizeLarge+ Theme.paddingLarge


//    }

    SectionHeader {
        id: header
        //text: "Text styling"
    }

    SilicaListView {
        id: listView
//        anchors.fill: parent
        width: parent.width

        model: sqlModel
//            header: PageHeader {
//                title: sqlModel.data(0, 0)
//            }

        delegate: MatchItem {
//                    height: Theme.fontSizeMedium + Theme.paddingSmall
            width: listView.width
            side1_: side1
            side2_: side2
            matchNumber: match_number
            matchCount: match_count
            url_: url
            Component.onCompleted: {
                console.debug(side1 +" vs " + side2 + " match " + match_number + "/"+ match_count)
            }
        }


        ViewPlaceholder {
            enabled: listView.count === 0
            text: "nothing here"
        }
    }


}

