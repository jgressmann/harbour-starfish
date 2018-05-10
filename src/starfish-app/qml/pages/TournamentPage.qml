import QtQuick 2.0
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0
import ".."

BasePage {
    id: page

    property var filters: undefined

    SqlVodModel {
        id: sqlModel
        dataManager: VodDataManager
        columns: ["event_full_name", "stage_name" ]
        select: "select distinct " + columns.join(",") + " from vods " + Global.whereClause(filters) + " order by match_date desc"
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
                title: sqlModel.data(sqlModel.index(0, 0), 0)
            }

            delegate: StageItem {
                width: listView.width
                //height: Global.itemHeight
                contentHeight: Global.itemHeight // StageItem is a ListItem
                //stageName: sqlModel.at(index, 1)
                //stageName: sqlModel.data(sqlModel.index(index, 1), 0)
                stageName: stage_name
                filters: Global.merge(page.filters, { "stage_name": stageName })
            }

            ViewPlaceholder {
                enabled: listView.count === 0
                text: "nothing here"
            }
        }
    }



    onStatusChanged: {
        // update items after activation
        if (status === PageStatus.Active) {
            var contentItem = listView.contentItem
            for(var index in contentItem.children) {
                var item = contentItem.children[index]
                item.update()
            }
        }
    }
}

