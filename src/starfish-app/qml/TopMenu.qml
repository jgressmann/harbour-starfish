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
        text: "Settings"
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
//        enabled: !!Global.scraper && Global.scraper.canStartFetch
//        onClicked: Global.scraper.startFetch()
        enabled: vodDatabaseDownloader.status !== VodDatabaseDownloader.Status_Downloading
        onClicked: vodDatabaseDownloader.downloadNew()
    }




//    MenuItem {
//        text: qsTr("Play video file")

//        property string filePath
////        onClicked: {
////            var dialog = pageStack.push(filePickerPage)
////            dialog.accepted.connect(function() {
////                if (filePath) {
////                    console.debug("open vod page for")
////                    pageStack.push(Qt.resolvedUrl(
////                                       "pages/VodPage.qml", {
////                                           source: filePath
////                                       }))
////                }
////            })
////        }

//        onClicked: pageStack.push(filePickerPage)

//        Component {
//             id: filePickerPage
//             FilePickerPage {
////                 nameFilters: [ '*.pdf', '*.doc' ]
//                 onSelectedContentPropertiesChanged: {
//                     var filePath = selectedContentProperties.filePath
//                     if (filePath) {
//                         console.debug("open vod page for " + filePath)
//                         pageStack.replace(Qt.resolvedUrl(
//                                            "pages/VodPage.qml",
//                                            {
//                                                source: filePath
//                                            },
//                                            PageStackAction.Immediate))
//                     }
//                 }
//             }
//         }
//    }

//        Component {
//             id: videoPickerPage
//             FilePickerPage {
//                 onSelectedContentPropertiesChanged: {
//                     page.selectedVideo = selectedContentProperties.filePath
//                 }
//             }
//         }

//    MenuItem {
//        text: qsTr("Fetch thumbnails")
//        onClicked: Sc2LinksDotCom.fetchThumbnails()
//    }

//    MenuItem {
//        text: qsTr("Load 2017")
//        onClicked: Sc2LinksDotCom.loadFromXml(2017)
//    }

//    MenuItem {
//        text: qsTr("Vacuum")
//        onClicked: Sc2LinksDotCom.vacuum()
//    }
}
