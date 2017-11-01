import QtQuick 2.0
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0
import ".."

Page {
    id: root
    property var filters: undefined
    property string stage
    SqlVodModel {
        id: sqlModel
        vodModel: Sc2LinksDotCom
        columns: ["side1", "side2", "match_date", "url", "match_count", "match_number"]
        select: "select " + columns.join(",") + " from vods " + Global.whereClause(filters) + " order by match_number asc"
    }

    Component.onCompleted: {
        console.debug(stage + " sql: " + sqlModel.select)
        var count = sqlModel.data(sqlModel.index(0, 4), 0)
        console.debug(stage + " rows: " + count)
    }

    SilicaFlickable {
        anchors.fill: parent

        // Why is this necessary?
        contentWidth: parent.width
//        contentHeight: 1000


        VerticalScrollDecorator {}

        SilicaListView {
            id: listView
            anchors.fill: parent
            model: sqlModel
            header: PageHeader {
                title: stage
            }

            delegate: Component {
                MatchItem {
                    width: listView.width
                    side1_: side1
                    side2_: side2
                    url_: url
                    matchNumber: match_number
                    matchCount: match_count
                    matchDate: match_date
//                    stageName: sqlModel.at(index, 1)
//                    stageIndex: sqlModel.at(index, 1)
//                    filters: Global.merge(page.filters, { "stage_index": sqlModel.at(index, 2) })
                }
            }

            ViewPlaceholder {
                enabled: listView.count === 0
                text: "nothing here"
            }
        }
    }
}

