import QtQuick 2.0
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0
import ".."

BasePage {
    id: page
    property var filters: undefined
    property alias stage: header.title
    property bool _sameDate: false
    property bool _sameSides: false
    property date _matchDate
    property string _side1
    property string _side2

    SqlVodModel {
        id: sqlModel
        dataManager: VodDataManager
        columns: ["side1_name", "side2_name", "side1_race", "side2_race", "match_date", "match_name", "rowid", "offset"]
        select: "select " + columns.join(",") + " from vods " + Global.whereClause(filters) + " order by match_date, match_name desc"
    }

    SqlVodModel {
        id: matchModel
        dataManager: VodDataManager
        columns: ["match_date"]
        select: "select distinct " + columns.join(",") + " from vods " + Global.whereClause(filters)

        onModelReset: _updateSameMatchDate()
        Component.onCompleted: _updateSameMatchDate()
    }

    SqlVodModel {
        id: sidesModel
        dataManager: VodDataManager
        columns: ["side1_name", "side2_name"]
        select: "select distinct " + columns.join(",") + " from vods " + Global.whereClause(filters)

        onModelReset: _updateSameSides()
        Component.onCompleted: _updateSameSides()
    }


    SilicaFlickable {
        anchors.fill: parent

        // Why is this necessary?
        contentWidth: parent.width


        VerticalScrollDecorator {}
        TopMenu {}
//        BottomMenu {}

        PageHeader {
            id: header
        }

        Column {
            id: fixture
            x: Theme.horizontalPageMargin
            width: parent.width - 2*x
            anchors.top: header.bottom

            Label {
                id: sidesLabel
                visible: _sameSides && !!_side2
                width: parent.width
                text: _side1 + " - " + _side2
                truncationMode: TruncationMode.Fade
                font.pixelSize: Theme.fontSizeExtraLarge
                color: Theme.highlightColor
            }

            Label {
                id: dateLabel
                visible: _sameDate
                width: parent.width
                text: Qt.formatDate(_matchDate)
                font.pixelSize: Theme.fontSizeSmall
                color: Theme.highlightColor
            }

            Item { // spacer to move list view down a little
                width: parent.width
                height: sidesLabel.visible || dateLabel.visible ? Theme.paddingSmall : 0
            }
        }

        SilicaListView {
            id: listView
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.top: fixture.visible ? fixture.bottom : header.bottom
            model: sqlModel

            delegate: Component {
                MatchItem {
                    width: listView.width
                    contentHeight: Global.itemHeight // MatchItem is a ListItem
                    side1: side1_name
                    side2: side2_name
                    race1: side1_race
                    race2: side2_race
                    matchName: match_name
                    matchDate: match_date
                    rowId: rowid
                    startOffsetMs: offset * 1000
                    showDate: !_sameDate
                    showSides: !_sameSides
                    showTitle: !_side2
//                    vodLabel: !_side2 ? "Episode" : "Match"

    //                    Component.onCompleted: {
    //                        console.debug("match item id=" + rowId)
    //                    }
                }
            }

            ViewPlaceholder {
                enabled: listView.count === 0
                text: "nothing here"
            }
        }
    }

    function _updateSameMatchDate() {
        _sameDate = matchModel.rowCount() === 1
        if (_sameDate) {
            _matchDate = matchModel.data(matchModel.index(0, 0))
        }
    }


    function _updateSameSides() {
        _sameSides = sidesModel.rowCount() === 1
        if (_sameSides) {
            _side1 = sidesModel.data(sidesModel.index(0, 0))
            _side2 = sidesModel.data(sidesModel.index(0, 1))
        }
    }
}

