import QtQuick 2.0
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0
import ".."

BasePage {
    id: page

    readonly property int vodCount: _vodCount
    property int _vodCount: 0

    onVodCountChanged: {
        console.debug("vod count="+ vodCount)
        _tryNavigateAway()
    }

    onStatusChanged: _tryNavigateAway()


    SqlVodModel {
        columns: ["count"]
        select: "select count(*) as count from vods"
        dataManager: VodDataManager
        onModelReset: {
            console.debug("sql model reset")
            _vodCount = data(index(0, 0), 0)
        }

        Component.onCompleted: {
            _vodCount = data(index(0, 0), 0)
        }
    }

    SilicaFlickable {
        anchors.fill: parent

        // Why is this necessary?
        contentWidth: parent.width

        TopMenu {}
//        BottomMenu {}

        ViewPlaceholder {
            enabled: vodCount === 0
            text: "There seems to be nothing here."
            hintText: " Pull down to fetch new VODs."
        }
    }

    function _tryNavigateAway() {
        if (PageStatus.Active === status && vodCount > 0) {
//            pageStack.replace(
//                Qt.resolvedUrl("FilterPage.qml"),
//                {
//                    title: qsTr("Game"),
//                    filters: {},
//                    key: "game",
//                    grid: true,
//                })
//            pageStack.replace(
//                Qt.resolvedUrl("NewPage.qml"))
            pageStack.replace(
                        Qt.resolvedUrl("EntryPage.qml"))
        }
    }
}

