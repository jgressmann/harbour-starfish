import QtQuick 2.0
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0
import ".."

Page {
    id: page

    property string title: undefined
    property var filters: undefined
    property string key: undefined
    property bool ascending: Global.sortKey(key) === 1
    property bool grid: false
    property bool _grid: grid && Global.hasIcon(key)
    property bool _showProgress: false



    SqlVodModel {
        id: sqlModel
        vodModel: Sc2LinksDotCom
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

        PullDownMenu {
            MenuItem {
                text: qsTr("Clear vods")
                onClicked: Sc2LinksDotCom.reset()
                enabled: Sc2LinksDotCom.status === Sc2LinksDotCom.Status_VodFetchingComplete || Sc2LinksDotCom.status === Sc2LinksDotCom.Status_VodFetchingInProgress
            }

            MenuItem {
                text: qsTr("Cancel fetching vods")
                onClicked: Sc2LinksDotCom.cancelPoll()
                enabled: Sc2LinksDotCom.status === Sc2LinksDotCom.Status_VodFetchingInProgress
            }

            MenuItem {
                text: qsTr("Fetch vods")
                onClicked: Sc2LinksDotCom.poll()
                enabled: Sc2LinksDotCom.status === Sc2LinksDotCom.Status_Ready || Sc2LinksDotCom.status === Sc2LinksDotCom.Status_VodFetchingComplete
            }

            MenuItem {
                text: qsTr("Hide progress")
                onClicked: { _showProgress = false }
                enabled: Sc2LinksDotCom.status === Sc2LinksDotCom.Status_VodFetchingInProgress
                visible: progress.visible
            }

            MenuItem {
                text: qsTr("Show progress")
                onClicked: { _showProgress = true }
                enabled: Sc2LinksDotCom.status === Sc2LinksDotCom.Status_VodFetchingInProgress
                visible: !progress.visible
            }
        }

        Column {
            width: parent.width
            Item {
//                color: "yellow"
                id: progress
                width: parent.width
                height: column.height
                visible: _showProgress && Sc2LinksDotCom.status === Sc2LinksDotCom.Status_VodFetchingInProgress

                Column {
                    id: column
                    width: parent.width
                    ProgressBar {
                        width: parent.width
                        id: bar
    //                    label: Sc2LinksDotCom.vodFetchingProgressDescription
                        value: Sc2LinksDotCom.vodFetchingProgress
                    }
                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: Sc2LinksDotCom.vodFetchingProgressDescription
                        font.pixelSize: Theme.fontSizeTiny
                        truncationMode: TruncationMode.Fade
                    }
                }
            }

            Item {
//                color: "red"
                width: parent.width
                height: flickable.height - progress.height

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
                        height: 128
                        key: page.key
                        value: sqlModel.data(sqlModel.index(index, 0), 0)
                        filters: page.filters
                        grid: page.grid

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

                    cellWidth: width / 2;
                    cellHeight: height / (Global.gridItemsPerPage / 2)

                    delegate: FilterItem {
                        key: page.key
                        value: sqlModel.data(sqlModel.index(index, 0), 0)
                        filters: page.filters
                        grid: page.grid
                        height: gridView.cellHeight
                        width: gridView.cellWidth

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
        }
    }
}

