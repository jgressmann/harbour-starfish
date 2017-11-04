import QtQuick 2.0
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0
import ".."

Page {
    id: page

    property var filters: undefined

    SqlVodModel {
        id: sqlModel
        vodModel: Sc2LinksDotCom
        columns: ["title", "stage_name", "stage_index" ]
        select: "select distinct " + columns.join(",") + " from vods " + Global.whereClause(filters) + " order by stage_index"
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
                title: sqlModel.data(sqlModel.index(0, 0), 0)
            }

            delegate: Component {
                StageItem {
                    width: listView.width
                    //stageName: sqlModel.at(index, 1)
                    stageName: {
                        console.debug("tournament page stage name index: " + index)
                        if (index >= 0) {
                            return sqlModel.data(sqlModel.index(index, 1), 0)
                        }

                        return "fix me"
                    }
//                    stageIndex: sqlModel.at(index, 1)
                    filters: Global.merge(page.filters, { "stage_index": sqlModel.data(sqlModel.index(index, 2), 0) })
                }
            }

            ViewPlaceholder {
                enabled: listView.count === 0
                text: "nothing here"
            }
        }
    }
}

