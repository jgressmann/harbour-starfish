import QtQuick 2.0
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0
import ".."

BasePage {
    id: page

    property string title: undefined
    property var filters: undefined
    property string key: undefined
    property bool ascending: Global.sortKey(key) === 1
    property bool grid: false
    property bool _grid: grid && Global.hasIcon(key)


    SqlVodModel {
        id: sqlModel
        dataManager: VodDataManager
        columns: [key]
        select: {
            var sql = "select distinct "+ key +" from vods"
            sql += Global.whereClause(filters)
            sql += " order by " + key + " " + (ascending ? "asc" : "desc")
            console.debug(title + ": " + sql)
            return sql
        }
    }

    SilicaFlickable {


        id: flickable
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
            visible: !_grid
            header: PageHeader {
                title: page.title
            }

            delegate: FilterItem {
                width: listView.width
                //height: Global.itemHeight
                contentHeight: Global.itemHeight // FilterItem is a ListItem
                key: page.key
                value: sqlModel.data(sqlModel.index(index, 0), 0)
                filters: page.filters
                grid: page.grid
                foo: index * 0.1

                Component.onCompleted: {
                    console.debug("filter page key: "+key+" index: " + index + " value: " + value)
                }
            }

            ViewPlaceholder {
                enabled: listView.count === 0
                text: "nothing here"
            }
        }

        SilicaGridView {
            id: gridView
            anchors.fill: parent
            model: sqlModel
            visible: _grid
            header: PageHeader {
                title: page.title
            }

//                    cellWidth: 2*Theme.horizontalPageMargin + 2*Theme.iconSizeExtraLarge
//                    cellHeight: 3*Theme.paddingLarge + 2*Theme.iconSizeExtraLarge
            cellWidth: parent.width / 2
            cellHeight: parent.height / (Global.gridItemsPerPage / 2)

            delegate: FilterItem {
                key: page.key
                value: sqlModel.data(sqlModel.index(index, 0), 0)
                filters: page.filters
                grid: page.grid
                //height: gridView.cellHeight
                contentHeight: gridView.cellHeight // FilterItem is a ListItem
                width: gridView.cellWidth
//                        width: GridView.view.width
//                        height: GridView.view.height

                Component.onCompleted: {
                    console.debug("filter page key: "+key+" index: " + index + " value: " + value)
                }
            }

            ViewPlaceholder {
                enabled: gridView.count === 0
                text: "nothing here"
            }
        }
    }


    onStatusChanged: {
        // update items after activation
        if (status === PageStatus.Active) {
            var contentItem = grid ? gridView.contentItem : listView.contentItem
            for(var index in contentItem.children) {
                var item = contentItem.children[index]
                item.update()
            }
        }
    }
}

