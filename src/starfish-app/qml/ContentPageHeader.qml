import QtQuick 2.0
import Sailfish.Silica 1.0
import org.duckdns.jgressmann 1.0
import "."

PageHeader {
    leftMargin: Theme.horizontalPageMargin + busy.width + Theme.paddingSmall
    rightMargin: Theme.horizontalPageMargin + homeButton.width  + Theme.paddingMedium

    IconButton {
        x: (parent.page ? parent.page.width : Screen.width) - (Theme.horizontalPageMargin + width)
        anchors.verticalCenter: parent.verticalCenter
        id: homeButton
        width: Theme.iconSizeSmallPlus
        height: Theme.iconSizeSmallPlus
        icon.source: "/usr/share/icons/hicolor/128x128/apps/harbour-starfish.png"
        icon.width: width
        icon.height: height
        onClicked: pageStack.replace(Qt.resolvedUrl("pages/EntryPage.qml"))
    }

    VodDataManagerBusyIndicator {
        id: busy
    }
}
