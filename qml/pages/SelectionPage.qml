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

    function computeRemainingKeys() {
        var result = []

        for (var i = 0; i < Global.filterKeys.length; ++i) {
            var key = Global.filterKeys[i]
            if (key in filters) {
                continue
            }

            result.push(key)
        }

        return result
    }

    function hasIcon(key) {
        return key in {
            "game": 1,
            "tournament": 1,
        }
    }

    function sortKey(key) {
        return {
            "year": -1,
        }[key] || 1
    }

    Component.onCompleted: {
        remainingKeys = computeRemainingKeys()
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
                        pageStack.push(Qt.resolvedUrl("FilterPage.qml"), {
                            title: label,
                            filters: filters,
                            key: key,
                            showImage: hasIcon(key),
                            sorted: sortKey(key),
                        })
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

