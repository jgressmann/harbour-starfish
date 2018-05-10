import QtQuick 2.0
import Sailfish.Silica 1.0
import "."

IconButton {
    id: root
    width: Theme.iconSizeMedium
    height: Theme.iconSizeMedium
    icon.source: "image://theme/icon-m-favorite"
    property real seen: 0.5


//    layer.enabled: true

    ProgressImageBlendEffect {
        src0: Image {
            source: "image://theme/icon-m-favorite-selected"
        }

        src1: Image {
            source: "image://theme/icon-m-favorite"
        }

        anchors.fill: parent
        progress: {
            var s = Math.max(0, Math.min(seen, 1))
            if (s > 0) {
                if (s >= 1) {
                    return 1
                }

                return 0.3 + s * 0.4 // star fills from 0.3-0.8
            }

            return 0
        }
    }
}
