import QtQuick 2.0
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0
import ".."

Page {
    id: page

    property string title: undefined
    property var remainingKeys: undefined
    property var filters: undefined

    ListModel {
        id: listModel
    }




    Component.onCompleted: {
        remainingKeys = Global.remainingKeys(filters)
        console.debug("remaining keys " + remainingKeys.join(", "))

        var entries = []
        for (var i = 0; i < remainingKeys.length; ++i) {
            var key = remainingKeys[i]
            console.debug("key: " + key)
            var entry = {
                "key": key,
                "label": Sc2LinksDotCom.label(key, undefined),
            }
            entries.push(entry)
        }

        entries.sort()

        for (var i = 0; i < entries.length; ++i) {
            console.debug("key: " + entries[i].key + " value: " + entries[i].label)
            listModel.append(entries[i])
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
            model: listModel
            header: PageHeader {
                title: page.title
            }

            delegate: Component {
                BackgroundItem {
                    height: 128
                    width: listView.width
                    Label {
                        height: parent.height
                        x: Theme.paddingLarge
                        width: parent.width - 2*Theme.paddingLarge
                        text: label
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: Theme.fontSizeHuge
                        truncationMode: TruncationMode.Fade
                    }

                    onClicked: {
                        console.debug("selection item click " + key)
                        // see if we have a single value left, then go straight to tournament
                        var myFilters = Global.clone(filters)
                        var myKey = key
                        while (true) {
                            var sql = "select distinct "+ myKey +" from vods" + Global.whereClause(myFilters)
                            var values = Global.values(sql)
                            var rem = Global.remainingKeys(myFilters)
                            if (values.length === 1) {
                                myFilters[myKey] = values[0]
                                if (0 === rem.length) {
                                    // exhausted all filter keys, show tournament
                                    pageStack.push(Qt.resolvedUrl("TournamentPage.qml"), {
                                        filters: myFilters
                                    })
                                    break
                                } else {
                                    myKey = rem[0]
                                }
                            } else {
                                // more than 1 value, show filter page
                                pageStack.push(Qt.resolvedUrl("FilterPage.qml"), {
                                    title: Sc2LinksDotCom.label(rem[0], undefined),
                                    filters: myFilters,
                                    key: myKey,
                                })
                                break
                            }
                        }
                    }
                }
            }

            ViewPlaceholder {
                enabled: listView.count === 0
                text: "you should never have glimpsed sight of me..."
            }
        }
    }
}

