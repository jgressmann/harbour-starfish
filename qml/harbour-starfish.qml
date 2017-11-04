import QtQuick 2.0
import Sailfish.Silica 1.0
import "pages"
//import org.duckdns.jgressmann 1.0

ApplicationWindow
{
    initialPage: Component {
        StartPage {}
//        YoutubePage {
//            url: "https://www.youtube.com/watch?v=Q7l6nQxfYvY&autoplay=1&iv_load_policy=3"
//        }

    }
//    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations
}

