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

ShaderEffect {

    property variant src
    property real progress: 0.314
    property bool inverse: false
    blending: false
    vertexShader: "
#ifndef GL_FRAGMENT_PRECISION_HIGH
#define highp mediump
#endif
uniform highp mat4 qt_Matrix;
attribute highp vec4 qt_Vertex;
attribute highp vec2 qt_MultiTexCoord0;
varying highp vec2 coord;
void main() {
    coord = qt_MultiTexCoord0;
    gl_Position = qt_Matrix * qt_Vertex;
}"
    fragmentShader: "
#ifndef GL_FRAGMENT_PRECISION_HIGH
#define highp mediump
#endif
uniform highp float progress;
uniform bool inverse;
varying highp vec2 coord;
uniform sampler2D src;
uniform lowp float qt_Opacity;
void main() {
    lowp vec4 tex = texture2D(src, coord);
    bool x;
    if (inverse) {
        x = coord.x >= progress;
    } else {
        x = coord.x <= progress;
    }

    if (x) {
        discard;
    } else {
        gl_FragColor = tex*qt_Opacity;
    }
}"
    }
