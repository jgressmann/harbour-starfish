import QtQuick 2.0

ShaderEffect {

    property variant src0
    property variant src1
    property real progress: 0.314

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
varying highp vec2 coord;
uniform sampler2D src0;
uniform sampler2D src1;
uniform lowp float qt_Opacity;
void main() {
    lowp vec4 tex0 = texture2D(src0, coord);
    lowp vec4 tex1 = texture2D(src1, coord);
    if (coord.x <= progress) {
        gl_FragColor = tex0*qt_Opacity;
    } else {
        gl_FragColor = tex1*qt_Opacity;
    }
}"
    }
