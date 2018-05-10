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
