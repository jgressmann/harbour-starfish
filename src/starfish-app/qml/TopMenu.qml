import QtQuick 2.0
import Sailfish.Silica 1.0
import Sailfish.Pickers 1.0
import org.duckdns.jgressmann 1.0
import "."

PullDownMenu {
    MenuItem {
        text: "About " + App.displayName
        onClicked: pageStack.push(Qt.resolvedUrl("pages/AboutPage.qml"))
    }

    MenuItem {
        text: qsTr("Settings")
        onClicked: pageStack.push(Qt.resolvedUrl("pages/SettingsPage.qml"))
    }

    MenuItem {
        text: qsTr("Clear VOD data")
        onClicked: {
            var dialog = pageStack.push(
                        Qt.resolvedUrl("pages/ConfirmClearDialog.qml"),
                        {
                            acceptDestination: Qt.resolvedUrl("pages/StartPage.qml"),
                            acceptDestinationAction: PageStackAction.Replace
                        })
            dialog.accepted.connect(function() {
                console.debug("clear")
                VodDataManager.clear()
            })
        }
    }


    MenuItem {
        text: qsTr("Video player")
        onClicked: pageStack.push(Qt.resolvedUrl("pages/VideoPlayerPage.qml"))
    }

    MenuItem {
        text: qsTr("Fetch new VODs")
        enabled: vodDatabaseDownloader.status !== VodDatabaseDownloader.Status_Downloading
        onClicked: vodDatabaseDownloader.downloadNew()
    }



}
