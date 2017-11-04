import QtQuick 2.0
import Sailfish.Silica 1.0
import "pages"
//import org.duckdns.jgressmann 1.0

ApplicationWindow
{
    initialPage: Component {
        StartPage { }
//        YoutubePage {
//            url: "https://www.youtube.com/watch?v=Q7l6nQxfYvY"
//        }
    }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations
}

