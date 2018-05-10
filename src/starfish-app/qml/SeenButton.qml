/* The MIT License (MIT)
 *
 * Copyright (c) 2018 Jean Gressmann <jean@0x42.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

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
