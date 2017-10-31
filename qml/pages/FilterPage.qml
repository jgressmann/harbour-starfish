import QtQuick 2.0
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0
import ".."

Page {
    id: page

    property string title: undefined
    property var filters: undefined
    property string key: undefined
    property int sorted: 0

    SqlVodModel {
        id: sqlModel
        vodModel: Sc2LinksDotCom
        columns: [key]
        select: {
            var sql = "select distinct "+ key +" from vods"
            sql += Global.whereClause(filters)
//            if (filters) {
//                var where = ""
//                for (var dictKey in filters) {
//                    if (where.length > 0) {
//                        where += " and "
//                    } else {
//                        where += " where "
//                    }

//                    where += dictKey + "=\"" + filters[dictKey] + "\""
//                }

//                sql += where
//            }
            if (sorted !== 0) {
                sql += " order by " + key + " " + (sorted > 0 ? "asc" : "desc")
            }
            console.debug(title + ": " + sql)
            return sql
        }
    }

    SilicaFlickable {
        anchors.fill: parent

        // Why is this necessary?
        contentWidth: parent.width


        VerticalScrollDecorator {}

        SilicaListView {
            id: listView
            anchors.fill: parent
            model: sqlModel
            header: PageHeader {
                title: page.title
            }

            delegate: FilteredItem {
                id: filteredItem
                width: listView.width
                key: page.key
                value: sqlModel.at(index, 0)
                filters: page.filters

                Component.onCompleted: {
                    console.debug("filter page key: "+key+" index: " + index + " value: " + value)
                }
            }

            ViewPlaceholder {
                enabled: listView.count === 0
                text: "nothing here"
            }
        }
    }
}

