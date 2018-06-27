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
        icon.source: "image://theme/icon-m-home"
        icon.width: width
        icon.height: height
        onClicked: pageStack.replaceAbove(null, Qt.resolvedUrl("pages/EntryPage.qml"))
    }

    VodDataManagerBusyIndicator {
        id: busy
    }
}